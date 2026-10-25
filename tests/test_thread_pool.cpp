#include "test_common.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "thread_pool.hpp"

using cpusim::ThreadPool;

// Every submitted task runs exactly once. The slots are plain ints on purpose:
// each task writes only its own slot, and wait_idle() must make those writes
// visible to this thread. ThreadSanitizer would flag it if it did not.
static void every_task_runs_exactly_once() {
  const int kTasks = 20000;
  std::vector<int> ran(kTasks, 0);
  std::atomic<int> total{0};
  {
    ThreadPool pool(4);
    for (int i = 0; i < kTasks; ++i) {
      pool.submit([&ran, &total, i] {
        ++ran[i];
        total.fetch_add(1);
      });
    }
    pool.wait_idle();
    assert(total.load() == kTasks);
    for (int i = 0; i < kTasks; ++i) assert(ran[i] == 1);
  }
  for (int i = 0; i < kTasks; ++i) assert(ran[i] == 1);  // still 1 after destruction
}

// Shutdown must finish everything already queued, not drop it.
static void shutdown_drains_queue() {
  std::mutex m;
  std::condition_variable cv;
  bool release = false;
  std::atomic<int> done{0};

  ThreadPool pool(1);
  // Task 0 blocks the only worker, so the other 100 pile up in the queue.
  pool.submit([&] {
    std::unique_lock<std::mutex> lock(m);
    cv.wait(lock, [&] { return release; });
    done.fetch_add(1);
  });
  for (int i = 0; i < 100; ++i) pool.submit([&] { done.fetch_add(1); });

  std::thread releaser([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    {
      std::lock_guard<std::mutex> lock(m);
      release = true;
    }
    cv.notify_all();
  });

  pool.shutdown();  // returns only after all 101 tasks ran and workers joined
  releaser.join();
  assert(done.load() == 101);
}

static void destructor_runs_pending_tasks() {
  std::atomic<int> done{0};
  {
    ThreadPool pool(2);
    for (int i = 0; i < 500; ++i) {
      pool.submit([&done] {
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        done.fetch_add(1);
      });
    }
  }  // destructor: finish queued work, join
  assert(done.load() == 500);
}

static void submit_after_shutdown_throws_and_shutdown_is_idempotent() {
  ThreadPool pool(2);
  pool.shutdown();
  pool.shutdown();  // second call is harmless
  bool threw = false;
  try {
    pool.submit([] {});
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
}

static void bad_arguments_are_rejected() {
  bool threw = false;
  try {
    ThreadPool pool(0);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  ThreadPool pool(1);
  threw = false;
  try {
    pool.submit(std::function<void()>());
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

// Several producer threads submit at the same time.
static void many_producers() {
  std::atomic<int> done{0};
  ThreadPool pool(4);
  std::vector<std::thread> producers;
  for (int p = 0; p < 8; ++p) {
    producers.emplace_back([&] {
      for (int i = 0; i < 1000; ++i) pool.submit([&done] { done.fetch_add(1); });
    });
  }
  for (std::thread& t : producers) t.join();
  pool.wait_idle();
  assert(done.load() == 8000);
}

// Four tasks that each wait for all four to be running at once. This can only
// finish if the pool really runs tasks in parallel (and sleeps/wakes properly).
static void tasks_run_in_parallel() {
  const int kThreads = 4;
  std::mutex m;
  std::condition_variable cv;
  int arrived = 0;
  std::atomic<int> met{0};

  ThreadPool pool(kThreads);
  for (int i = 0; i < kThreads; ++i) {
    pool.submit([&] {
      std::unique_lock<std::mutex> lock(m);
      ++arrived;
      cv.notify_all();
      if (cv.wait_for(lock, std::chrono::seconds(10), [&] { return arrived == kThreads; })) {
        met.fetch_add(1);
      }
    });
  }
  pool.wait_idle();
  assert(met.load() == kThreads);
}

static void throwing_task_does_not_kill_pool() {
  std::atomic<int> done{0};
  ThreadPool pool(2);
  pool.submit([] { throw std::runtime_error("boom"); });
  for (int i = 0; i < 100; ++i) pool.submit([&done] { done.fetch_add(1); });
  pool.wait_idle();
  assert(done.load() == 100);
  assert(pool.failed_tasks() == 1);
}

static void wait_idle_can_be_reused() {
  std::atomic<int> done{0};
  ThreadPool pool(3);
  for (int round = 1; round <= 5; ++round) {
    for (int i = 0; i < 100; ++i) pool.submit([&done] { done.fetch_add(1); });
    pool.wait_idle();
    assert(done.load() == round * 100);
  }
  pool.wait_idle();  // nothing queued: returns immediately
}

int main() {
  std::cout << "thread pool tests\n";
  RUN_TEST(every_task_runs_exactly_once);
  RUN_TEST(shutdown_drains_queue);
  RUN_TEST(destructor_runs_pending_tasks);
  RUN_TEST(submit_after_shutdown_throws_and_shutdown_is_idempotent);
  RUN_TEST(bad_arguments_are_rejected);
  RUN_TEST(many_producers);
  RUN_TEST(tasks_run_in_parallel);
  RUN_TEST(throwing_task_does_not_kill_pool);
  RUN_TEST(wait_idle_can_be_reused);
  std::cout << "all passed\n";
  return 0;
}
