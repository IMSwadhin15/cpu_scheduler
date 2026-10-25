#include "thread_pool.hpp"

#include <stdexcept>
#include <utility>

namespace cpusim {

ThreadPool::ThreadPool(std::size_t num_threads) {
  if (num_threads == 0) throw std::invalid_argument("ThreadPool needs at least one thread");
  workers_.reserve(num_threads);
  for (std::size_t i = 0; i < num_threads; ++i) {
    workers_.emplace_back([this] { worker_loop(); });
  }
}

ThreadPool::~ThreadPool() { shutdown(); }

void ThreadPool::submit(std::function<void()> task) {
  if (!task) throw std::invalid_argument("ThreadPool::submit: empty task");
  {
    // The mutex guards tasks_ and stopping_. Hold it only while touching them.
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) throw std::runtime_error("ThreadPool::submit: pool is shut down");
    tasks_.push(std::move(task));
  }
  // Notify after unlocking so the woken worker does not immediately block on
  // the mutex we still hold. One new task => wake exactly one worker.
  work_cv_.notify_one();
}

void ThreadPool::wait_idle() {
  std::unique_lock<std::mutex> lock(mutex_);
  // wait() atomically unlocks the mutex and sleeps; it re-locks before
  // returning. The predicate is re-checked on every wake-up, which protects
  // against spurious wake-ups.
  idle_cv_.wait(lock, [this] { return tasks_.empty() && active_ == 0; });
}

void ThreadPool::shutdown() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;  // from now on submit() throws
  }
  // Wake every sleeping worker so it can notice stopping_. Workers keep
  // running until the queue is empty, so queued tasks are not lost.
  work_cv_.notify_all();

  for (std::thread& t : workers_) {
    if (t.joinable()) t.join();
  }
}

std::size_t ThreadPool::failed_tasks() {
  std::lock_guard<std::mutex> lock(mutex_);
  return failed_;
}

void ThreadPool::worker_loop() {
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      // Sleep (no CPU use) until there is work or we are told to stop.
      work_cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });

      // Woken with an empty queue => it must be because we are stopping and
      // everything has been drained, so this worker can exit.
      if (tasks_.empty()) return;

      task = std::move(tasks_.front());
      tasks_.pop();
      ++active_;  // counted while still holding the lock so wait_idle() never
                  // sees "queue empty, nobody active" while a task is in flight
    }

    // Run the task WITHOUT holding the mutex, otherwise only one task could
    // run at a time and submit() would be blocked while tasks execute.
    bool failed = false;
    try {
      task();
    } catch (...) {
      failed = true;
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      --active_;
      if (failed) ++failed_;
      if (tasks_.empty() && active_ == 0) idle_cv_.notify_all();
    }
  }
}

}  // namespace cpusim
