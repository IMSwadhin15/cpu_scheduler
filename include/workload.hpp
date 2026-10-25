#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "process.hpp"

namespace cpusim {

// Workload file format: one process per line, three whole numbers
//
//     <id> <arrival> <burst>
//
// Blank lines are skipped, and everything after '#' on a line is a comment.
// Throws std::runtime_error (with the line number) on malformed input.
std::vector<Process> parse_workload(std::istream& in);

// Same, reading from a file. Throws std::runtime_error if it cannot be opened.
std::vector<Process> load_workload(const std::string& path);

struct GeneratorOptions {
  int count = 10;         // number of processes
  unsigned seed = 1;      // same seed => identical workload on every machine
  int max_arrival = 50;   // arrival times are uniform in [0, max_arrival]
  int max_burst = 10;     // burst times are uniform in [1, max_burst]
};

// Seeded random workload with ids 1..count. Uses std::mt19937 (whose output is
// fully specified by the C++ standard) and plain arithmetic to map to a range,
// so results are reproducible across compilers and platforms.
// Throws std::invalid_argument for non-positive count/max_burst or negative max_arrival.
std::vector<Process> generate_workload(const GeneratorOptions& options);

}  // namespace cpusim
