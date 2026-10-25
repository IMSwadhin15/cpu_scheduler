// cpu_sched: run one scheduling algorithm on one workload and print the
// per-process table, the averages and a Gantt chart.

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "report.hpp"
#include "scheduler_factory.hpp"
#include "simulator.hpp"
#include "workload.hpp"

namespace {

void usage(std::ostream& out) {
  out << "Usage: cpu_sched --algo fcfs|rr|sjf [--quantum N] (--file PATH | --random N)\n"
         "                 [--seed S] [--max-arrival A] [--max-burst B] [--no-gantt]\n\n"
         "  --algo       scheduling algorithm (default: fcfs)\n"
         "  --quantum    time slice for rr (default: 4)\n"
         "  --file       workload file, lines of '<id> <arrival> <burst>'\n"
         "  --random     generate N random processes instead of reading a file\n"
         "  --seed       random seed (default: 1)\n"
         "  --max-arrival, --max-burst   random workload ranges (default: 50, 10)\n"
         "  --no-gantt   skip the Gantt chart\n";
}

// Reads the value after a flag, or exits with a message if it is missing.
std::string next_arg(int& i, int argc, char** argv) {
  if (i + 1 >= argc) {
    std::cerr << "error: " << argv[i] << " needs a value\n";
    std::exit(2);
  }
  return argv[++i];
}

}  // namespace

int main(int argc, char** argv) {
  std::string algo = "fcfs";
  std::string file;
  int quantum = 4;
  bool gantt = true;
  bool random = false;
  cpusim::GeneratorOptions gen;

  try {
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--help" || arg == "-h") {
        usage(std::cout);
        return 0;
      } else if (arg == "--algo") {
        algo = next_arg(i, argc, argv);
      } else if (arg == "--quantum") {
        quantum = std::stoi(next_arg(i, argc, argv));
      } else if (arg == "--file") {
        file = next_arg(i, argc, argv);
      } else if (arg == "--random") {
        random = true;
        gen.count = std::stoi(next_arg(i, argc, argv));
      } else if (arg == "--seed") {
        gen.seed = static_cast<unsigned>(std::stoul(next_arg(i, argc, argv)));
      } else if (arg == "--max-arrival") {
        gen.max_arrival = std::stoi(next_arg(i, argc, argv));
      } else if (arg == "--max-burst") {
        gen.max_burst = std::stoi(next_arg(i, argc, argv));
      } else if (arg == "--no-gantt") {
        gantt = false;
      } else {
        std::cerr << "error: unknown option '" << arg << "'\n\n";
        usage(std::cerr);
        return 2;
      }
    }

    if (file.empty() == !random) {  // need exactly one of --file / --random
      std::cerr << "error: give exactly one of --file or --random\n\n";
      usage(std::cerr);
      return 2;
    }

    auto scheduler = cpusim::make_scheduler(algo, quantum);
    auto procs = random ? cpusim::generate_workload(gen) : cpusim::load_workload(file);
    cpusim::SimResult result = cpusim::simulate(*scheduler, std::move(procs));

    cpusim::print_report(std::cout, result);
    if (gantt) std::cout << "\nGantt chart:\n" << cpusim::format_gantt(result.timeline);
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
