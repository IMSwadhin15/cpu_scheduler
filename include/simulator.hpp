#pragma once

#include <string>
#include <vector>

#include "process.hpp"
#include "scheduler.hpp"

namespace cpusim {

// Everything produced by one simulation run.
struct SimResult {
  std::string algorithm;
  std::vector<Process> processes;  // same order as the input, times filled in
  std::vector<Slice> timeline;     // who held the CPU when (includes idle gaps)
  double avg_waiting = 0;
  double avg_turnaround = 0;
  double avg_response = 0;
  int makespan = 0;  // time the last process finished
};

// Run `scheduler` on `procs` and compute the per-process and average times.
//
//   turnaround = finish - arrival
//   waiting    = turnaround - burst
//   response   = first CPU time - arrival
//
// Throws std::invalid_argument for bad input (duplicate id, arrival < 0,
// burst <= 0) and std::logic_error if the scheduler returns an impossible
// timeline (e.g. a process runs before it arrives or gets the wrong total
// CPU time), which catches bugs in newly written algorithms early.
// Thread-safe as long as `scheduler` is only used through its const interface.
SimResult simulate(const Scheduler& scheduler, std::vector<Process> procs);

}  // namespace cpusim
