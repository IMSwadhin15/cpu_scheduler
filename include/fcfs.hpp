#pragma once

#include "scheduler.hpp"

namespace cpusim {

// First-Come First-Served: run processes to completion in arrival order.
class FCFS : public Scheduler {
 public:
  std::string name() const override { return "FCFS"; }
  std::vector<Slice> schedule(const std::vector<Process>& procs) const override;
};

}  // namespace cpusim
