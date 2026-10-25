#pragma once

#include <string>
#include <vector>

#include "scheduler.hpp"
#include "thread_pool.hpp"

namespace cpusim {

struct BatchConfig {
  int workloads = 1000;          // how many random workloads to simulate
  int procs_per_workload = 100;  // processes in each workload
  unsigned base_seed = 1;        // workload i uses seed base_seed + i
  int max_arrival = 200;
  int max_burst = 20;
};

// Average waiting time of each algorithm on each workload.
// waiting[w][a] = average waiting time of algorithm a on workload w.
struct BatchResult {
  std::vector<std::string> algorithms;       // names, in column order
  std::vector<std::vector<double>> waiting;  // [workload][algorithm]

  // Mean over all workloads for algorithm `a`.
  double mean_waiting(std::size_t a) const;
};

// Simulate `config.workloads` random workloads with every scheduler in
// `schedulers`, spreading the workloads over `pool`'s threads (one task per
// workload), and return once all of them are done.
//
// The result does not depend on the number of threads: each task writes only
// its own row of the result, and workload i is always generated from the same
// seed.
BatchResult run_batch(ThreadPool& pool, const std::vector<const Scheduler*>& schedulers,
                      const BatchConfig& config);

}  // namespace cpusim
