#include "batch.hpp"

#include "simulator.hpp"
#include "workload.hpp"

namespace cpusim {

double BatchResult::mean_waiting(std::size_t a) const {
  if (waiting.empty()) return 0;
  double sum = 0;
  for (const auto& row : waiting) sum += row[a];
  return sum / static_cast<double>(waiting.size());
}

BatchResult run_batch(ThreadPool& pool, const std::vector<const Scheduler*>& schedulers,
                      const BatchConfig& config) {
  BatchResult result;
  for (const Scheduler* s : schedulers) result.algorithms.push_back(s->name());

  // Allocate every row BEFORE any task starts. Tasks then only write to their
  // own row's elements and never resize anything shared, so they need no lock.
  result.waiting.assign(static_cast<std::size_t>(config.workloads),
                        std::vector<double>(schedulers.size(), 0.0));

  for (int w = 0; w < config.workloads; ++w) {
    pool.submit([&result, &schedulers, &config, w] {
      GeneratorOptions gen;
      gen.count = config.procs_per_workload;
      gen.seed = config.base_seed + static_cast<unsigned>(w);
      gen.max_arrival = config.max_arrival;
      gen.max_burst = config.max_burst;
      const std::vector<Process> procs = generate_workload(gen);

      // Every algorithm sees exactly the same processes. The schedulers are
      // shared between threads, which is safe because schedule() is const and
      // keeps no state.
      for (std::size_t a = 0; a < schedulers.size(); ++a) {
        result.waiting[static_cast<std::size_t>(w)][a] =
            simulate(*schedulers[a], procs).avg_waiting;
      }
    });
  }

  // Wait for all tasks. This also makes the workers' writes visible to us
  // (the pool's mutex gives the needed happens-before ordering).
  pool.wait_idle();
  return result;
}

}  // namespace cpusim
