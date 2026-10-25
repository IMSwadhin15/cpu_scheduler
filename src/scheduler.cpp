#include "scheduler.hpp"

#include <algorithm>

namespace cpusim {

void Scheduler::append(std::vector<Slice>& timeline, int pid, int start, int end) {
  if (end <= start) return;
  if (!timeline.empty() && timeline.back().pid == pid && timeline.back().end == start) {
    timeline.back().end = end;  // extend instead of adding a new box
  } else {
    timeline.push_back(Slice{pid, start, end});
  }
}

std::vector<std::size_t> Scheduler::order_by_arrival(const std::vector<Process>& procs) {
  std::vector<std::size_t> order(procs.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&procs](std::size_t a, std::size_t b) {
    if (procs[a].arrival != procs[b].arrival) return procs[a].arrival < procs[b].arrival;
    return procs[a].id < procs[b].id;
  });
  return order;
}

}  // namespace cpusim
