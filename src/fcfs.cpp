#include "fcfs.hpp"

namespace cpusim {

std::vector<Slice> FCFS::schedule(const std::vector<Process>& procs) const {
  std::vector<Slice> timeline;
  int now = 0;
  for (std::size_t idx : order_by_arrival(procs)) {
    const Process& p = procs[idx];
    if (now < p.arrival) {  // nobody is ready yet: CPU idles until p arrives
      append(timeline, kIdle, now, p.arrival);
      now = p.arrival;
    }
    append(timeline, p.id, now, now + p.burst);  // runs without interruption
    now += p.burst;
  }
  return timeline;
}

}  // namespace cpusim
