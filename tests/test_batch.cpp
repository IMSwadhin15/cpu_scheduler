#include "test_common.hpp"

#include <vector>

#include "batch.hpp"
#include "fcfs.hpp"
#include "round_robin.hpp"
#include "sjf.hpp"
#include "simulator.hpp"
#include "thread_pool.hpp"
#include "workload.hpp"

using namespace cpusim;

static BatchConfig small_config() {
  BatchConfig c;
  c.workloads = 64;
  c.procs_per_workload = 40;
  c.base_seed = 7;
  return c;
}

// The parallel batch gives the same numbers regardless of thread count, and
// those numbers match a plain sequential computation.
static void parallel_batch_matches_sequential() {
  FCFS fcfs;
  RoundRobin rr(4);
  SJF sjf;
  const std::vector<const Scheduler*> schedulers = {&fcfs, &rr, &sjf};
  const BatchConfig config = small_config();

  // Sequential reference, no pool involved.
  std::vector<std::vector<double>> expected;
  for (int w = 0; w < config.workloads; ++w) {
    GeneratorOptions g;
    g.count = config.procs_per_workload;
    g.seed = config.base_seed + static_cast<unsigned>(w);
    g.max_arrival = config.max_arrival;
    g.max_burst = config.max_burst;
    std::vector<Process> procs = generate_workload(g);
    std::vector<double> row;
    for (const Scheduler* s : schedulers) row.push_back(simulate(*s, procs).avg_waiting);
    expected.push_back(row);
  }

  for (std::size_t threads : {1u, 2u, 4u, 8u}) {
    ThreadPool pool(threads);
    BatchResult r = run_batch(pool, schedulers, config);
    assert(r.algorithms.size() == 3);
    assert(r.algorithms[0] == "FCFS" && r.algorithms[1] == "RR (q=4)" && r.algorithms[2] == "SJF");
    assert(r.waiting == expected);
  }
}

static void sjf_wins_on_average() {
  FCFS fcfs;
  RoundRobin rr(4);
  SJF sjf;
  ThreadPool pool(4);
  BatchResult r = run_batch(pool, {&fcfs, &rr, &sjf}, small_config());
  assert(r.mean_waiting(2) < r.mean_waiting(0));  // SJF < FCFS
}

int main() {
  std::cout << "batch (scheduler + thread pool) tests\n";
  RUN_TEST(parallel_batch_matches_sequential);
  RUN_TEST(sjf_wins_on_average);
  std::cout << "all passed\n";
  return 0;
}
