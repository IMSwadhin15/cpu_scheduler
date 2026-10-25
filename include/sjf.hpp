#pragma once

#include "scheduler.hpp"

namespace cpusim {

// Shortest Job First (non-preemptive): whenever the CPU becomes free, pick the
// ready process with the smallest burst. Ties go to the earlier arrival, then
// the smaller id. A running process is never interrupted.
class SJF : public Scheduler {
 public:
  std::string name() const override { return "SJF"; }
  std::vector<Slice> schedule(const std::vector<Process>& procs) const override;
};

}  // namespace cpusim
