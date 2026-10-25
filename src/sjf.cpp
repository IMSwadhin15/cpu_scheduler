#include "sjf.hpp"

#include <algorithm>
#include <limits>

namespace cpusim {

std::vector<Slice> SJF::schedule(const std::vector<Process>& procs) const {
  std::vector<Slice> timeline;
  std::vector<bool> done(procs.size(), false);
  std::size_t completed = 0;
  int now = 0;

  while (completed < procs.size()) {
    // Among processes that have already arrived, find the shortest burst.
    const Process* best = nullptr;
    std::size_t best_idx = 0;
    int next_arrival = std::numeric_limits<int>::max();
    for (std::size_t i = 0; i < procs.size(); ++i) {
      if (done[i]) continue;
      const Process& p = procs[i];
      if (p.arrival > now) {
        next_arrival = std::min(next_arrival, p.arrival);
        continue;
      }
      const bool better =
          best == nullptr || p.burst < best->burst ||
          (p.burst == best->burst &&
           (p.arrival < best->arrival || (p.arrival == best->arrival && p.id < best->id)));
      if (better) {
        best = &p;
        best_idx = i;
      }
    }

    if (best == nullptr) {  // nothing ready: idle until the next arrival
      append(timeline, kIdle, now, next_arrival);
      now = next_arrival;
      continue;
    }

    append(timeline, best->id, now, now + best->burst);  // run to completion
    now += best->burst;
    done[best_idx] = true;
    ++completed;
  }
  return timeline;
}

}  // namespace cpusim
