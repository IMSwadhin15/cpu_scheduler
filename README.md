# CPU Scheduler Simulator and Thread Pool

A C++17 project with two parts that are then used together:

1. **A CPU scheduling simulator**: FCFS, Round Robin (configurable quantum) and
   non-preemptive Shortest Job First. It computes per-process start, finish,
   waiting, turnaround and response times, the averages, and draws a text Gantt
   chart.
2. **A thread pool** built from `std::thread`, `std::mutex`,
   `std::condition_variable` and a task queue (producer-consumer), with clean
   shutdown and no busy waiting.

The `compare` program runs thousands of random workloads through all three
schedulers in parallel on the thread pool, prints a comparison table of average
waiting time, and measures how the run time changes with 1, 2, 4 and 8 worker
threads.

There are no third-party dependencies. Tests use plain `assert` and CTest.

New to the code? Read [LEARN.md](LEARN.md) first. It walks through the files in
reading order with everyday examples.

## Build and test

Requirements: a C++17 compiler (developed with GCC 15), CMake 3.13 or newer, and
a POSIX-style threads library.

```bash
cmake -B build && cmake --build build && ctest --test-dir build
```

This builds in Release mode by default and produces `build/cpu_sched`,
`build/compare` and three test programs.

ThreadSanitizer build (builds everything with `-fsanitize=thread`, runs the
tests and a small parallel batch):

```bash
bash scripts/run_tsan.sh
```

## Run

### One algorithm on one workload: `cpu_sched`

```bash
./build/cpu_sched --algo rr --quantum 2 --file examples/workload.txt
./build/cpu_sched --algo sjf --random 20 --seed 42
./build/cpu_sched --help
```

| Option | Meaning |
| --- | --- |
| `--algo fcfs\|rr\|sjf` | scheduling algorithm (default `fcfs`) |
| `--quantum N` | time slice for `rr` (default 4) |
| `--file PATH` | workload file |
| `--random N` | generate N random processes instead (use `--seed S`, `--max-arrival A`, `--max-burst B`) |
| `--no-gantt` | do not print the Gantt chart |

Workload file format: one process per line as `<id> <arrival> <burst>`; blank
lines and `#` comments are ignored. `examples/workload.txt`:

```
# id arrival burst
1 0 5
2 1 3
3 2 8
4 3 6
```

Example output (`--algo rr --quantum 2 --file examples/workload.txt`):

```
Algorithm: RR (q=2)

  PID  Arrival  Burst  Start  Finish  Waiting  Turnaround  Response
    1        0      5      0      14        9          14         0
    2        1      3      2      11        7          10         1
    3        2      8      4      22       12          20         2
    4        3      6      8      20       11          17         5

Average waiting time:    9.75
Average turnaround time: 15.25
Average response time:   2.00

Gantt chart:
| P1 | P2 | P3 | P1 | P4 | P2 | P3 | P1 | P4 | P3 | P4 | P3 |
0    2    4    6    8    10   11   13   14   16   18   20   22
```

For the same four processes, FCFS gives an average waiting time of 5.75, SJF
5.25 and RR (q=2) 9.75. These numbers are also checked by the unit tests against
hand calculations.

### Many workloads in parallel: `compare`

```bash
./build/compare                       # defaults: 2000 workloads x 200 processes
./build/compare --workloads 5000 --procs 100 --quantum 8 --repeats 9 --csv results/timings.csv
```

It prints the average waiting time per algorithm, checks that the results are
identical for every thread count, and writes `results/timings.csv`.

## Design

```
include/ + src/

Process               one job: id, arrival, burst + computed start/finish/waiting/turnaround/response
Scheduler (abstract)  name(), schedule(procs) -> timeline of Slice{pid, start, end}
 |-- FCFS             run in arrival order
 |-- SJF              shortest ready job first, non-preemptive
 `-- RoundRobin       time slices of `quantum`, preempted job goes to the back
simulate()            validates input, calls scheduler.schedule(), derives all metrics from the timeline
format_gantt() / print_report()    text output
parse_workload() / load_workload() / generate_workload()    input files and seeded random workloads
make_scheduler()      "fcfs"/"rr"/"sjf" -> Scheduler object (used by the CLI)

ThreadPool            workers + std::queue<std::function<void()>> + mutex + 2 condition variables
run_batch()           one pool task per random workload; every scheduler runs on it
```

Key decisions:

* **A scheduler only produces a timeline.** `schedule()` returns who held the
  CPU when. All the arithmetic (waiting, turnaround, response, averages) lives
  in one place, `simulate()`, and it also checks the timeline is sane (no
  overlaps, nobody runs before arriving, everyone gets exactly their burst
  time). A new algorithm is just a new subclass implementing `name()` and
  `schedule()`; `tests/test_schedulers.cpp` contains a throw-away one to show
  this.
* **Schedulers are stateless and `const`**, so one object can be shared by all
  worker threads.
* **Parallel batch without locks on the results.** `run_batch` allocates the
  whole result table first; each task writes only its own row. The pool's mutex
  provides the happens-before ordering when `wait_idle()` returns.
* **Thread pool**: one mutex protects the queue and flags. `work_cv_` is where
  idle workers sleep; `idle_cv_` is where `wait_idle()` sleeps. Tasks are run
  with the mutex released. See the comments in `src/thread_pool.cpp`.

Tie-break conventions: SJF breaks ties by earlier arrival then smaller id. In
Round Robin, a process arriving exactly when a quantum expires is queued before
the process that was just preempted.

## Tests

`ctest --test-dir build` runs three programs (plain `assert`; `tests/test_common.hpp`
makes sure `NDEBUG` does not silently disable them in Release builds):

| Test | What it checks |
| --- | --- |
| `schedulers` | FCFS, SJF and RR against hand-calculated times for a 4-process example, idle gaps, tie-breaks, RR quantum boundary, input validation, a custom algorithm subclass, Gantt text, file parsing, seeded generator, factory |
| `thread_pool` | every task runs exactly once (20,000 tasks), shutdown finishes queued tasks, destructor drains the queue, submit after shutdown throws, many producer threads, tasks really run in parallel, a throwing task does not kill the pool, `wait_idle()` is reusable |
| `batch` | the parallel batch gives exactly the same numbers as a sequential run, for 1, 2, 4 and 8 threads |

## Results measured on this machine

All numbers below were produced by `./build/compare --repeats 15` on:
Intel Core i5-1235U (12 hardware threads), Ubuntu on WSL2, GCC 15.2, CMake
Release build. Raw output: [results/compare_output.txt](results/compare_output.txt).

### Average waiting time (2000 random workloads of 200 processes, seed 1, RR quantum 4)

| Algorithm | Average waiting time |
| --- | --- |
| FCFS | 945.06 |
| RR (q=4) | 1244.90 |
| SJF | 625.31 |

SJF is lowest, as theory predicts for average waiting time. RR has the highest
average waiting time here because it makes every job share the CPU; its benefit
is better response time, not waiting time.

### Run time vs worker threads

The same batch (2000 workloads x 3 algorithms) timed 15 times per thread count;
[results/timings.csv](results/timings.csv):

| Worker threads | Median time (s) | Fastest run (s) | Speed-up (median) |
| --- | --- | --- | --- |
| 1 | 0.561 | 0.470 | 1.00x |
| 2 | 0.285 | 0.219 | 1.97x |
| 4 | 0.194 | 0.149 | 2.89x |
| 8 | 0.143 | 0.089 | 3.91x |

Notes on these numbers:

* The batch is short (about half a second on one thread) and this is a laptop
  CPU under WSL2, so timings are noisy. Early runs with a less careful timing
  method varied by more than 2x between invocations, so the program now times
  all thread counts in interleaved rounds and reports the median and the
  minimum. Across four invocations of that final method (the table above plus
  three more), the 8-thread median speed-up ranged from 3.9x to 4.8x, so treat
  the numbers as accurate to roughly +-15%.
* Speed-up is below the thread count at 4 and 8 threads. This CPU has a mix of
  performance and efficiency cores and the machine was not otherwise idle; I did
  not investigate the exact cause.
* Results are bit-for-bit identical for every thread count (the program checks
  this and exits with an error otherwise).

### ThreadSanitizer

`bash scripts/run_tsan.sh` builds everything with `-fsanitize=thread`, runs all
three test programs and a parallel batch. **ThreadSanitizer reported no data
races**, so no fix was needed. To confirm the tool actually detects races in
this setup, I also compiled a small program that increments an unprotected
counter from 1000 pool tasks. ThreadSanitizer flagged it immediately.

## Layout

```
CMakeLists.txt
include/    headers (one per class)
src/        implementations, main.cpp (cpu_sched), compare_main.cpp (compare)
tests/      test_schedulers.cpp, test_thread_pool.cpp, test_batch.cpp, test_common.hpp
examples/   sample workload file
results/    timings.csv and compare_output.txt (produced by ./build/compare)
scripts/    run_tsan.sh
README.md   this file
LEARN.md    plain-language walkthrough and interview questions
```
