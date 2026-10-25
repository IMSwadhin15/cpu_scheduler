#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace cpusim {

// A fixed-size pool of worker threads fed by a task queue (producer-consumer).
//
//   producers (any thread)           consumers (the pool's workers)
//   submit(task) --push--> [ queue ] --pop--> run task
//
// * submit() never blocks on the work itself: it queues the task and wakes one
//   sleeping worker.
// * Idle workers sleep on a condition variable; there is no busy waiting.
// * shutdown() / the destructor finish every task already queued, then join
//   all workers.
// * Tasks must not throw; if one does, the exception is swallowed so the worker
//   survives, and failed_tasks() counts it.
class ThreadPool {
 public:
  // Starts `num_threads` workers. Throws std::invalid_argument if it is 0.
  explicit ThreadPool(std::size_t num_threads);

  // Same as shutdown().
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  // Queue `task` to run on some worker thread. Thread-safe.
  // Throws std::runtime_error if the pool is shutting down and
  // std::invalid_argument if `task` is empty.
  void submit(std::function<void()> task);

  // Block until the queue is empty and no worker is running a task.
  // Thread-safe, and the pool stays usable afterwards. Everything the tasks
  // wrote is visible to the caller once this returns.
  void wait_idle();

  // Stop accepting tasks, let the workers drain what is queued, join them.
  // Safe to call more than once, but call it from one thread at a time and
  // never from inside a task (a worker cannot join itself).
  void shutdown();

  std::size_t size() const { return workers_.size(); }

  // Number of tasks that exited with an exception.
  std::size_t failed_tasks();

 private:
  void worker_loop();

  std::vector<std::thread> workers_;

  // ---- everything below is protected by mutex_ ----
  std::mutex mutex_;
  std::queue<std::function<void()>> tasks_;
  bool stopping_ = false;     // set by shutdown(); no new tasks accepted
  std::size_t active_ = 0;    // tasks currently being executed by workers
  std::size_t failed_ = 0;    // tasks that threw

  // Two condition variables, two different questions:
  std::condition_variable work_cv_;  // workers wait here: "is there a task, or are we stopping?"
  std::condition_variable idle_cv_;  // wait_idle() waits here: "is everything finished?"
};

}  // namespace cpusim
