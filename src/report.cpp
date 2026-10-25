#include "report.hpp"

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <sstream>

namespace cpusim {

namespace {

std::string label_for(int pid) { return pid == kIdle ? "idle" : "P" + std::to_string(pid); }

}  // namespace

std::string format_gantt(const std::vector<Slice>& timeline) {
  if (timeline.empty()) return "(empty timeline)\n";

  std::string boxes = "|";
  std::string marks;
  marks += std::to_string(timeline.front().start);

  for (const Slice& s : timeline) {
    // Box is wide enough for its label and grows with the slice length, but is
    // capped so very long slices do not make the chart unreadable.
    const std::string label = label_for(s.pid);
    const std::size_t width = std::max<std::size_t>(label.size() + 2,
                                                    std::min<std::size_t>(s.end - s.start, 12));
    const std::size_t left = (width - label.size()) / 2;
    boxes += std::string(left, ' ') + label + std::string(width - label.size() - left, ' ') + "|";

    // Put the end-time under the closing '|' of this box.
    const std::size_t column = boxes.size() - 1;
    const std::string t = std::to_string(s.end);
    if (marks.size() < column) marks += std::string(column - marks.size(), ' ');
    marks += t;
  }
  return boxes + "\n" + marks + "\n";
}

void print_report(std::ostream& out, const SimResult& r) {
  out << "Algorithm: " << r.algorithm << "\n\n";
  out << std::setw(5) << "PID" << std::setw(9) << "Arrival" << std::setw(7) << "Burst"
      << std::setw(7) << "Start" << std::setw(8) << "Finish" << std::setw(9) << "Waiting"
      << std::setw(12) << "Turnaround" << std::setw(10) << "Response" << "\n";
  for (const Process& p : r.processes) {
    out << std::setw(5) << p.id << std::setw(9) << p.arrival << std::setw(7) << p.burst
        << std::setw(7) << p.start << std::setw(8) << p.finish << std::setw(9) << p.waiting
        << std::setw(12) << p.turnaround << std::setw(10) << p.response << "\n";
  }
  out << std::fixed << std::setprecision(2);
  out << "\nAverage waiting time:    " << r.avg_waiting << "\n";
  out << "Average turnaround time: " << r.avg_turnaround << "\n";
  out << "Average response time:   " << r.avg_response << "\n";
}

}  // namespace cpusim
