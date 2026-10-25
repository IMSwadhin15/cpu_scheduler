#pragma once

#include "scheduler.hpp"

namespace cpusim {

// Round Robin: ready processes take turns, each for at most `quantum` time
// units. A process that is not finished goes to the back of the queue.
//
// Tie-break convention: a process that arrives at the same instant a quantum
// expires is queued *before* the process that was just preempted.
class RoundRobin : public Scheduler {
 public:
  // Throws std::invalid_argument if quantum <= 0.
  explicit RoundRobin(int quantum);

  std::string name() const override;
  std::vector<Slice> schedule(const std::vector<Process>& procs) const override;

  int quantum() const { return quantum_; }

 private:
  int quantum_;
};

}  // namespace cpusim
