#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "scheduler.hpp"
#include "simulator.hpp"

namespace cpusim {

// Text Gantt chart, two lines of boxes plus a line of time marks, e.g.
//
//   |  P1  | P2  |  idle  |
//   0      5     8        11
//
std::string format_gantt(const std::vector<Slice>& timeline);

// Per-process table (arrival, burst, start, finish, waiting, turnaround,
// response) followed by the three averages.
void print_report(std::ostream& out, const SimResult& result);

}  // namespace cpusim
