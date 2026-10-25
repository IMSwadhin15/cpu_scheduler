// compare: simulate many random workloads with FCFS, Round Robin and SJF in
// parallel on the thread pool, print a comparison table, then time the same
// batch with 1, 2, 4 and 8 worker threads and save the timings as CSV.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "batch.hpp"
#include "fcfs.hpp"
#include "round_robin.hpp"
#include "sjf.hpp"
#include "thread_pool.hpp"

namespace {

void usage(std::ostream& out) {
  out << "Usage: compare [--workloads N] [--procs N] [--quantum Q] [--seed S]\n"
         "               [--repeats R] [--csv PATH]\n\n"
         "  --workloads  random workloads per batch   (default 2000)\n"
         "  --procs      processes per workload        (default 200)\n"
         "  --quantum    Round Robin time quantum      (default 4)\n"
         "  --seed       base random seed              (default 1)\n"
         "  --repeats    timed runs per thread count; the median is reported (default 5)\n"
         "  --csv        output file                   (default results/timings.csv)\n";
}

std::string next_arg(int& i, int argc, char** argv) {
  if (i + 1 >= argc) {
    std::cerr << "error: " << argv[i] << " needs a value\n";
    std::exit(2);
  }
  return argv[++i];
}

double seconds_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace

int main(int argc, char** argv) {
  cpusim::BatchConfig config;
  config.workloads = 2000;
  config.procs_per_workload = 200;
  int quantum = 4;
  int repeats = 5;
  std::string csv_path = "results/timings.csv";

  try {
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--help" || arg == "-h") {
        usage(std::cout);
        return 0;
      } else if (arg == "--workloads") {
        config.workloads = std::stoi(next_arg(i, argc, argv));
      } else if (arg == "--procs") {
        config.procs_per_workload = std::stoi(next_arg(i, argc, argv));
      } else if (arg == "--quantum") {
        quantum = std::stoi(next_arg(i, argc, argv));
      } else if (arg == "--seed") {
        config.base_seed = static_cast<unsigned>(std::stoul(next_arg(i, argc, argv)));
      } else if (arg == "--repeats") {
        repeats = std::stoi(next_arg(i, argc, argv));
      } else if (arg == "--csv") {
        csv_path = next_arg(i, argc, argv);
      } else {
        std::cerr << "error: unknown option '" << arg << "'\n\n";
        usage(std::cerr);
        return 2;
      }
    }
    if (config.workloads <= 0 || config.procs_per_workload <= 0 || repeats <= 0) {
      std::cerr << "error: --workloads, --procs and --repeats must be positive\n";
      return 2;
    }

    cpusim::FCFS fcfs;
    cpusim::RoundRobin rr(quantum);
    cpusim::SJF sjf;
    const std::vector<const cpusim::Scheduler*> schedulers = {&fcfs, &rr, &sjf};

    std::cout << "Workloads: " << config.workloads << " x " << config.procs_per_workload
              << " processes, seed " << config.base_seed
              << ", hardware threads: " << std::thread::hardware_concurrency() << "\n\n";

    const std::vector<std::size_t> thread_counts = {1, 2, 4, 8};
    std::vector<double> medians;
    cpusim::BatchResult reference;  // result of the 1-thread run
    bool identical = true;

    for (std::size_t threads : thread_counts) {
      cpusim::ThreadPool pool(threads);
      cpusim::run_batch(pool, schedulers, config);  // warm-up, not timed

      std::vector<double> times;
      for (int r = 0; r < repeats; ++r) {
        const auto t0 = std::chrono::steady_clock::now();
        cpusim::BatchResult result = cpusim::run_batch(pool, schedulers, config);
        times.push_back(seconds_since(t0));

        if (threads == thread_counts.front() && r == 0) reference = result;
        if (result.waiting != reference.waiting) identical = false;
      }
      std::sort(times.begin(), times.end());
      medians.push_back(times[times.size() / 2]);
      std::cout << "  " << threads << " thread(s): median " << std::fixed << std::setprecision(4)
                << medians.back() << " s over " << repeats << " runs\n";
    }

    // ---- comparison table (from the 1-thread reference run) ----
    std::cout << "\nAverage waiting time, mean over " << config.workloads << " workloads\n";
    std::cout << std::left << std::setw(14) << "Algorithm" << std::right << std::setw(14)
              << "Avg waiting" << "\n";
    for (std::size_t a = 0; a < reference.algorithms.size(); ++a) {
      std::cout << std::left << std::setw(14) << reference.algorithms[a] << std::right
                << std::setw(14) << std::fixed << std::setprecision(2)
                << reference.mean_waiting(a) << "\n";
    }
    std::cout << "\nResults identical for every thread count: " << (identical ? "yes" : "NO")
              << "\n";
    if (!identical) return 1;

    // ---- CSV ----
    const std::filesystem::path csv(csv_path);
    if (csv.has_parent_path()) std::filesystem::create_directories(csv.parent_path());
    std::ofstream out(csv);
    if (!out) throw std::runtime_error("cannot write " + csv_path);
    out << "threads,workloads,procs_per_workload,repeats,median_seconds,speedup_vs_1_thread\n";
    for (std::size_t i = 0; i < thread_counts.size(); ++i) {
      out << thread_counts[i] << ',' << config.workloads << ',' << config.procs_per_workload << ','
          << repeats << ',' << std::fixed << std::setprecision(6) << medians[i] << ','
          << std::setprecision(3) << medians[0] / medians[i] << '\n';
    }
    std::cout << "Timings written to " << csv_path << "\n";
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
