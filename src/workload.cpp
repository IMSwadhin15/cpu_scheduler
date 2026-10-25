#include "workload.hpp"

#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>

namespace cpusim {

std::vector<Process> parse_workload(std::istream& in) {
  std::vector<Process> procs;
  std::string line;
  int line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    const std::size_t hash = line.find('#');
    if (hash != std::string::npos) line.erase(hash);

    std::istringstream fields(line);
    int id, arrival, burst;
    if (!(fields >> id)) continue;  // blank or comment-only line
    if (!(fields >> arrival >> burst)) {
      throw std::runtime_error("workload line " + std::to_string(line_no) +
                               ": expected '<id> <arrival> <burst>'");
    }
    std::string extra;
    if (fields >> extra) {
      throw std::runtime_error("workload line " + std::to_string(line_no) +
                               ": unexpected extra text '" + extra + "'");
    }
    procs.emplace_back(id, arrival, burst);
  }
  return procs;
}

std::vector<Process> load_workload(const std::string& path) {
  std::ifstream file(path);
  if (!file) throw std::runtime_error("cannot open workload file '" + path + "'");
  return parse_workload(file);
}

std::vector<Process> generate_workload(const GeneratorOptions& o) {
  if (o.count <= 0) throw std::invalid_argument("count must be > 0");
  if (o.max_arrival < 0) throw std::invalid_argument("max_arrival must be >= 0");
  if (o.max_burst <= 0) throw std::invalid_argument("max_burst must be > 0");

  std::mt19937 rng(o.seed);
  // The tiny modulo bias is irrelevant here; std::uniform_int_distribution is
  // avoided because its output differs between standard libraries.
  auto pick = [&rng](int lo, int hi) {
    return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1));
  };

  std::vector<Process> procs;
  procs.reserve(static_cast<std::size_t>(o.count));
  for (int i = 1; i <= o.count; ++i) {
    const int arrival = pick(0, o.max_arrival);
    const int burst = pick(1, o.max_burst);
    procs.emplace_back(i, arrival, burst);
  }
  return procs;
}

}  // namespace cpusim
