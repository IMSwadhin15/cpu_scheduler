# LEARN.md: how this project works

This is a walkthrough for you to read later. It goes in the order I suggest you
open the files. Each part has a plain explanation and an everyday example.

Contents

1. [The big picture](#1-the-big-picture)
2. [Reading order](#2-reading-order)
3. [Process and the Scheduler base class](#3-process-and-the-scheduler-base-class)
4. [FCFS](#4-fcfs-first-come-first-served)
5. [SJF](#5-sjf-shortest-job-first)
6. [Round Robin](#6-round-robin)
7. [The simulator: where the numbers come from](#7-the-simulator-where-the-numbers-come-from)
8. [Input, output and the command line](#8-input-output-and-the-command-line)
9. [The thread pool](#9-the-thread-pool)
10. [Mutex vs condition variable](#10-mutex-vs-condition-variable)
11. [Race conditions](#11-race-conditions)
12. [Putting both together: batch and compare](#12-putting-both-together-batch-and-compare)
13. [How the tests work](#13-how-the-tests-work)
14. [Ten interview questions](#14-ten-interview-questions)

---

## 1. The big picture

A computer has few CPU cores and many programs that want to run. The
**scheduler** is the part of the operating system that decides who gets the CPU
next. Different rules (algorithms) give different results: some are fair, some
are fast on average, some respond quickly.

This project has two halves:

* A **simulator** that pretends to be a CPU and applies a rule to a list of jobs,
  then measures how good the rule was.
* A **thread pool**, a small group of worker threads that take jobs from a shared
  to-do list.

Then the two are combined: we simulate thousands of different job lists, and
use the thread pool to do many of those simulations at the same time.

Everyday picture: the scheduler is a **receptionist at a doctor's clinic**.
Patients (processes) walk in at different times and need different amounts of
doctor time. The receptionist's rule decides who goes in next.

Three numbers measure how well the rule worked, for each patient:

| Number | Meaning | Clinic version |
| --- | --- | --- |
| Turnaround | finish time - arrival time | from walking in to walking out |
| Waiting | turnaround - burst | time spent in the waiting room (not with the doctor) |
| Response | first time with doctor - arrival | how long until you first see the doctor |

(*Burst* is how much doctor time the patient needs. *Arrival* is when they walk in.)

## 2. Reading order

Open the files in this order:

1. `include/process.hpp`: the data for one job
2. `include/scheduler.hpp` and `src/scheduler.cpp`: the base class every algorithm shares
3. `src/fcfs.cpp`, then `src/sjf.cpp`, then `src/round_robin.cpp`: the algorithms (their `.hpp` files are tiny)
4. `src/simulator.cpp`: turns a timeline into the numbers above
5. `src/workload.cpp`, `src/report.cpp`, `src/main.cpp`: input, output (Gantt chart), command line
6. `include/thread_pool.hpp` and `src/thread_pool.cpp`: the thread pool (the most important file for interviews)
7. `src/batch.cpp` and `src/compare_main.cpp`: using the pool to run many simulations
8. `tests/`: `test_schedulers.cpp` first (it has the hand-worked example), then `test_thread_pool.cpp`

## 3. Process and the Scheduler base class

**`Process`** is just a record: `id`, `arrival`, `burst` (the inputs), and
`start`, `finish`, `waiting`, `turnaround`, `response` (filled in later by the
simulator).

**`Scheduler`** is an *abstract class*, a contract that says "anything that calls
itself a scheduler must answer two questions":

```cpp
virtual std::string name() const = 0;
virtual std::vector<Slice> schedule(const std::vector<Process>& procs) const = 0;
```

The answer to `schedule` is a **timeline**: a list of `Slice{pid, start, end}`
meaning "process `pid` used the CPU from `start` until `end`". A gap where nobody
is ready becomes an `idle` slice.

Everyday example: think of a **restaurant menu template**. The template says
"every dish needs a name and a recipe". FCFS, SJF and Round Robin are three
dishes. The kitchen (the simulator) doesn't care which dish it is cooking,
it just follows the recipe and times it.

Why this design helps: to add a new algorithm (say, Priority scheduling) you
write **one new class** with those two functions. You do not touch the simulator,
the Gantt chart or the reports. `tests/test_schedulers.cpp` proves this with a
made-up "last arrived first" algorithm that works with no other changes.

Small details worth knowing:

* `virtual ~Scheduler() = default;` The base class has a *virtual destructor* so
  deleting a `FCFS` through a `Scheduler*` pointer cleans up properly.
* `schedule` is `const` and stores nothing between calls. That makes a scheduler
  safe to share between threads (this matters in section 12).
* `append()` in `scheduler.cpp` adds a slice and glues it to the previous one if
  the same process continues. So "P1 0-2" followed by "P1 2-4" becomes one box
  "P1 0-4" in the Gantt chart.

## 4. FCFS (First-Come First-Served)

**Rule:** whoever arrived first goes first, and runs until completely done.

**Everyday example:** a queue at a **supermarket checkout**. Nobody skips ahead
and nobody gets interrupted. If the first person has a full trolley, everyone
behind them waits, even someone with one bottle of milk. This is called the
*convoy effect*.

**Code idea** (`src/fcfs.cpp`): sort by arrival time, then walk through them. If
the CPU is free before the next process arrives, add an idle slice. Then run
that process for its whole burst.

Good: simple and fair in the "first in line" sense. Bad: one long job can make
everyone's waiting time large.

## 5. SJF (Shortest Job First)

**Rule:** when the CPU becomes free, pick the **ready** process with the
smallest burst. *Non-preemptive* means once a job starts, it is never interrupted.

**Everyday example:** an **express lane** at the supermarket. Short jobs go first,
so the average wait drops a lot. The price: someone with a huge trolley may wait
a very long time (called *starvation*).

**Code idea** (`src/sjf.cpp`): each time the CPU is free, scan all unfinished
processes that have already arrived (`arrival <= now`) and choose the smallest
burst. Ties go to the earlier arrival, then the smaller id, so the result is
always the same. If nobody has arrived yet, jump the clock forward (idle slice).

Worked example from the tests: P1(arrive 0, burst 5), P2(1, 3), P3(2, 8), P4(3, 6).
P1 is the only one there at time 0 so it runs 0-5. At time 5 the ready ones are
P2 (3), P3 (8), P4 (6), so the order is P2, P4, P3. Average waiting time is 5.25
compared with 5.75 for FCFS.

SJF gives the **lowest possible average waiting time** among non-preemptive
algorithms when all jobs are available together. The catch in real life is that
you do not know the burst time in advance.

## 6. Round Robin

**Rule:** each process gets the CPU for at most one **quantum** (a fixed time
slice). If it is not done, it goes to the back of the queue.

**Everyday example:** **kids taking turns on one game controller**, two minutes
each. Everybody gets a turn quickly, so nobody stares at a dead screen for long.
But the total play time gets stretched out because of all the hand-overs and
re-waiting.

**Code idea** (`src/round_robin.cpp`): keep a queue of ready processes and the
remaining time of each. Repeat: take the front process, run it for
`min(quantum, remaining)`, then add any newly arrived processes to the queue, then
(if the process isn't finished) add it to the back.

Convention to remember: if a new process arrives at exactly the moment a slice
ends, it is queued **before** the process that was just preempted. Different
textbooks do this differently, and the project uses and tests this one.

The quantum is a trade-off:

* Very big quantum: nobody gets interrupted, so RR turns into FCFS (a test
  checks this).
* Very small quantum: great response time, but in a real OS the cost of
  switching between processes starts to dominate. (The simulator does not model
  switching cost.)

RR's main strength is **response time**, not waiting time. In the project's
random benchmark, RR has the *highest* average waiting time (1244.9, versus 945.1
for FCFS and 625.3 for SJF).

## 7. The simulator: where the numbers come from

`simulate(scheduler, processes)` in `src/simulator.cpp` does four things:

1. **Validate the input** (unique ids, arrival >= 0, burst > 0).
2. **Ask the scheduler for a timeline.**
3. **Check the timeline is sane** (no overlapping slices, nobody runs before
   arriving, everybody gets exactly their burst time in total). If a brand-new
   algorithm has a bug, this check shouts immediately instead of printing
   quietly wrong numbers.
4. **Compute the numbers** from the timeline:
   * `start` = start of the process's first slice, `finish` = end of its last slice
   * `turnaround = finish - arrival`
   * `waiting = turnaround - burst`
   * `response = start - arrival`
   * then the three averages.

Because all the math is in this one function, FCFS/SJF/RR can't disagree about
what "waiting time" means.

## 8. Input, output and the command line

* `workload.cpp` reads lines like `1 0 5` (`id arrival burst`) and can also
  **generate random workloads from a seed**. A *seed* is a starting number for
  the random generator: the same seed always gives the same "random" workload.
  Think of it as a recipe number: "recipe 42" always makes the same cake. This is
  what makes experiments repeatable.
* `report.cpp` prints the table and the **Gantt chart** (a bar of boxes showing who
  had the CPU when, with times underneath).
* `main.cpp` is the command-line wrapper. It parses flags like `--algo rr
  --quantum 2 --file examples/workload.txt` and calls the pieces above.
* `scheduler_factory.cpp` turns the text `"rr"` into a real `RoundRobin` object.

## 9. The thread pool

### Why a pool?

Starting a thread is relatively expensive. If you have 2000 small jobs, creating
2000 threads would be wasteful and could overload the machine. A **thread pool**
creates a fixed number of threads once and keeps them alive, feeding them jobs
from a queue.

Everyday example: a **kitchen with 4 cooks**. Orders (tasks) arrive on a ticket
rail (the queue). Each cook takes the next ticket when free. You don't hire a new
cook for every order.

This is the **producer-consumer** pattern:

* **Producers** put tasks on the queue (`submit`).
* **Consumers** (the worker threads) take tasks off and run them.

### The pieces (`include/thread_pool.hpp`)

| Member | Purpose |
| --- | --- |
| `workers_` | the `std::thread`s |
| `tasks_` | the queue of `std::function<void()>` jobs |
| `mutex_` | the lock protecting the queue and flags |
| `work_cv_` | where idle workers sleep until there's work or shutdown |
| `idle_cv_` | where `wait_idle()` sleeps until everything is finished |
| `stopping_` | "no more tasks are being accepted" |
| `active_` | how many tasks are running right now |

### `submit(task)`

1. Lock the mutex.
2. If the pool is stopping, throw an error. Otherwise push the task onto the queue.
3. Unlock.
4. `notify_one()` to wake up one sleeping worker.

### The worker loop (`worker_loop`)

```
forever:
    lock the mutex
    sleep until (stopping or queue not empty)    <- no busy waiting
    if the queue is empty: (we're stopping) -> exit the thread
    take a task off the queue, active++
    unlock the mutex
    run the task                                 <- NOT holding the lock
    lock; active--; if everything is idle, wake wait_idle(); unlock
```

Two important things:

* **The task runs without the lock.** If a worker held the mutex while running,
  only one task could ever run at a time, which would defeat the purpose of the pool.
* **No busy waiting.** A worker with nothing to do is *asleep* inside
  `condition_variable::wait`, using no CPU. A busy-waiting version would be a
  loop `while (queue.empty()) {}` burning a whole core doing nothing.

### Clean shutdown

`shutdown()` (also called by the destructor):

1. Set `stopping_ = true` (under the lock). From now on `submit` throws.
2. `notify_all()` so every sleeping worker wakes up and checks.
3. `join()` every thread, which means "wait until it has finished".

Workers only exit when `stopping_` is true **and** the queue is empty. So tasks
that were already queued still run, and nothing is dropped. This is the
requirement "finish queued tasks, then join all threads". A test covers exactly
this: it blocks the only worker, queues 100 more tasks, then calls `shutdown()`
and checks that all 101 ran.

### `wait_idle()`

Blocks until the queue is empty **and** no worker is mid-task. The `active_`
counter is what makes that possible (an empty queue isn't enough: a worker could
have just taken the last task and still be running it). It's how `run_batch`
knows "all the simulations are finished".

## 10. Mutex vs condition variable

These two are always used together, but they do different jobs.

**Mutex ("mutual exclusion"): protects shared data.**
Everyday example: the **key to a single bathroom**. Only the person holding the
key can be inside. Everyone else waits. In the code, `std::lock_guard` or
`std::unique_lock` takes the key and **automatically returns it** when the scope
ends, even if an exception is thrown.

**Condition variable: lets a thread sleep until something changes.**
Everyday example: a **restaurant pager** (the buzzer they give you while you wait
for a table). You don't stand at the counter asking "is it ready?" every second
(busy waiting). You sit down and the buzzer goes off when your table is ready.
`wait()` is sitting down; `notify_one()` / `notify_all()` is the buzzer.

Why do you need both? The condition (e.g. "queue not empty") is shared data, so
it has to be checked under the mutex. But a thread can't sleep while holding the
mutex, or nobody else could ever change the queue. `cv.wait(lock, predicate)`
solves this: it **atomically** unlocks the mutex and goes to sleep, and when woken
it **re-locks** before returning.

Rules of thumb used in the code:

* **Always wait with a predicate:** `cv.wait(lock, [this]{ return stopping_ || !tasks_.empty(); })`.
  Two reasons:
  * *Spurious wake-ups*: a waiting thread can occasionally wake up for no reason.
    The predicate makes it go back to sleep if the condition isn't really true.
  * *Lost wake-ups*: if the notify happens before the thread starts waiting, the
    predicate is already true, so the thread simply doesn't sleep.
* **Change the condition while holding the mutex**, then notify. That's why
  `submit` pushes under the lock and `shutdown` sets `stopping_` under the lock.
* **Two condition variables** because there are two different questions
  ("is there work?" and "is everything done?"). Mixing them would wake threads
  that have nothing to check.

## 11. Race conditions

A **data race** happens when two threads touch the same memory at the same time
and at least one of them writes, with no lock in between. The result is
unpredictable.

Everyday example: two people **editing the same shared spreadsheet cell**. Both
read "10", both add 1, both write "11". Two additions happened, but the cell says
11 instead of 12. In code:

```cpp
int counter = 0;
// 1000 pool tasks each do:   ++counter;     // ++ is really: read, add, write
```

`++counter` is three steps (read, add, write), and two threads can interleave
those steps. Fixes: protect it with a mutex, or use `std::atomic<int>`.

Where races could have happened in this project, and how they're avoided:

| Shared thing | How it's protected |
| --- | --- |
| the task queue, `stopping_`, `active_` | `mutex_` |
| the results table in `run_batch` | each task writes only **its own row**, which was allocated before any task started, so no two threads touch the same memory |
| the scheduler objects | `schedule()` is `const` and keeps no state, so concurrent reads are safe |
| reading results after the tasks finish | `wait_idle()` locks the same mutex the workers used, which gives the "happens-before" guarantee that the writes are visible |

**ThreadSanitizer (TSan)** is a tool that watches memory accesses while the
program runs and reports races. We build with `-fsanitize=thread` and run the
tests (`bash scripts/run_tsan.sh`). It found no races in this project. As a
check that TSan really works here, a tiny program with an unprotected counter
was deliberately written and TSan flagged it at once. The key point for an
interview: *"no race reported" only means none happened in the runs that were
tested*, so the tests deliberately use many tasks and many threads.

## 12. Putting both together: batch and compare

`run_batch` (`src/batch.cpp`):

1. Make a results table with one row per workload (before any thread starts).
2. For each workload number `w`, `submit` a task that:
   * generates workload `w` from seed `base_seed + w`,
   * runs FCFS, RR and SJF on it,
   * writes the three average waiting times into row `w`.
3. `wait_idle()`, then return the table.

Because each workload has its own seed, **the answer is the same whether you use
1 thread or 8**. The `compare` program checks that, so parallelism changes only
the speed, never the answer.

`compare_main.cpp` times the batch with 1, 2, 4 and 8 threads and writes
`results/timings.csv`. It runs several rounds and reports the median and the
minimum, because timing on a normal laptop is noisy.

Speed-up isn't perfectly proportional to thread count (8 threads gave roughly
4x here, not 8x). Typical reasons are a shared CPU with other programs, memory
bandwidth, cores of different speeds, and the serial parts of the program.

## 13. How the tests work

* Plain `assert` with a tiny helper header (`tests/test_common.hpp`). It undefines
  `NDEBUG` first, because in a Release build `assert` is normally compiled to
  nothing and the tests would "pass" without checking anything.
* **Hand-calculated examples:** the 4-process example is worked out on paper in
  comments next to each assertion. If you want to learn the algorithms, redo
  those calculations yourself and compare.
* **Thread pool tests** check: every task runs *exactly once* (each task bumps its
  own array slot, so a skipped or duplicated task shows up), shutdown drains the
  queue, and 4 tasks that wait for each other prove the pool really runs tasks in
  parallel.
* CTest (`add_test` in `CMakeLists.txt`) just runs each test program and treats a
  non-zero exit code (a failed assert aborts the program) as a failure.

## 14. Ten interview questions

Answers are written the way you could say them out loud.

**1. What does this project do?**
It's a CPU scheduling simulator in C++17 with FCFS, Round Robin and non-preemptive
SJF, plus a thread pool I wrote myself. I use the pool to run thousands of random
workloads through all three schedulers in parallel and compare the average
waiting time. I also measured how the run time changes with 1, 2, 4 and 8 threads.

**2. How would you add a new scheduling algorithm?**
I'd write one new subclass of `Scheduler` and implement `name()` and
`schedule()`, which returns a timeline of who ran when. The simulator computes
all the waiting, turnaround and response times from that timeline, so nothing
else changes. The tests include a throw-away algorithm that proves it.

**3. Explain the difference between FCFS, SJF and Round Robin.**
FCFS runs jobs in arrival order to completion, so it's simple but one long job
delays everyone. SJF always picks the shortest ready job, which gives the lowest
average waiting time but can starve long jobs and needs to know burst times in
advance. Round Robin gives each job a fixed time slice in turns, which gives good
response time but, as my benchmark showed, a higher average waiting time.

**4. How do you calculate waiting, turnaround and response time?**
Turnaround is finish minus arrival. Waiting is turnaround minus burst, so the
time spent ready but not running. Response is the first time it got the CPU
minus its arrival.

**5. How does your thread pool work?**
There's a fixed set of worker threads and a queue of `std::function` tasks
protected by one mutex. `submit` locks, pushes a task, unlocks and notifies one
worker. Each worker sleeps on a condition variable until the queue has something
or the pool is stopping, pops a task, releases the lock and runs it. So it's a
producer-consumer setup.

**6. What's the difference between a mutex and a condition variable?**
A mutex protects shared data so that only one thread touches it at a time. A
condition variable lets a thread sleep until some condition becomes true,
instead of spinning. You use them together: the condition is checked under the
mutex, and `wait` releases the mutex while sleeping and re-takes it on waking.

**7. Why do you wait on a condition variable with a predicate?**
Because of spurious wake-ups, where a thread can wake without anyone notifying
it, and because the state might already have changed before the thread went to
sleep. The predicate is re-checked every time, so the thread only continues when
the condition is really true.

**8. How does shutdown work, and how do you make sure queued tasks aren't lost?**
Shutdown sets a `stopping` flag under the lock, so new submits are rejected, and
wakes all workers. A worker only exits when stopping is set *and* the queue is
empty, so it keeps draining queued tasks first. Then I join all threads. I have
a test that blocks the only worker, queues 100 tasks, shuts down, and checks all
of them ran.

**9. How did you check for data races, and what did you find?**
I built everything with ThreadSanitizer and ran the thread pool tests, the
batch test and a parallel run. It reported no races, so I had nothing to fix. To
be sure the tool worked, I also wrote a small program with an unprotected
counter, and it reported that race right away. I'd also say a clean run isn't a
proof, only that none occurred in those runs.

**10. How do you avoid locks when running the simulations in parallel, and what speed-up did you get?**
Each task writes only to its own row of a results table that's allocated before
any task starts, and the schedulers are `const` and stateless, so tasks never
write to shared memory and I don't need extra locks. Waiting for completion goes
through the pool's mutex, which makes the results visible. On my machine (a
12-thread laptop CPU under WSL2) the batch got about 2x faster with 2 threads
and about 4x with 8 threads, not a perfect 8x, and the results were identical
for every thread count.
