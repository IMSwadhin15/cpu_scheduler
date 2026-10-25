#include "round_robin.hpp"

#include <algorithm>
#include <queue>
#include <stdexcept>

namespace cpusim {

RoundRobin::RoundRobin(int quantum) : quantum_(quantum) {
  if (quantum <= 0) throw std::invalid_argument("Round Robin quantum must be > 0");
}

std::string RoundRobin::name() const { return "RR (q=" + std::to_string(quantum_) + ")"; }

std::vector<Slice> RoundRobin::schedule(const std::vector<Process>& procs) const {
  std::vector<Slice> timeline;
  const std::vector<std::size_t> order = order_by_arrival(procs);

  std::vector<int> remaining(procs.size());
  for (std::size_t i = 0; i < procs.size(); ++i) remaining[i] = procs[i].burst;

  std::queue<std::size_t> ready;  // indices into procs
  std::size_t next = 0;           // next not-yet-admitted process in `order`
  std::size_t finished = 0;
  int now = 0;

  // Move every process that has arrived by time `t` into the ready queue.
  auto admit = [&](int t) {
    while (next < order.size() && procs[order[next]].arrival <= t) ready.push(order[next++]);
  };

  while (finished < procs.size()) {
    admit(now);
    if (ready.empty()) {  // idle until the next process shows up
      const int t = procs[order[next]].arrival;
      append(timeline, kIdle, now, t);
      now = t;
      continue;
    }

    const std::size_t i = ready.front();
    ready.pop();
    const int run = std::min(quantum_, remaining[i]);
    append(timeline, procs[i].id, now, now + run);
    now += run;
    remaining[i] -= run;

    admit(now);  // arrivals during this slice queue up *before* the preempted process
    if (remaining[i] > 0) {
      ready.push(i);
    } else {
      ++finished;
    }
  }
  return timeline;
}

}  // namespace cpusim
