#pragma once

#include <string>
#include <vector>

#include "process.hpp"

namespace cpusim {

// pid used for a stretch of time when no process is ready.
constexpr int kIdle = -1;

// "Process `pid` held the CPU from time `start` up to (not including) `end`."
struct Slice {
  int pid;
  int start;
  int end;
};

// Base class for every scheduling algorithm.
//
// To add a new algorithm: derive from Scheduler, implement name() and
// schedule(), and (optionally) register it in scheduler_factory.cpp.
// Nothing else in the project needs to change: the Simulator turns the
// timeline you return into waiting/turnaround/response times, and the
// Gantt chart and reports work off the same timeline.
//
// schedule() is const and keeps no state between calls, so one scheduler
// object can safely be used by many threads at once.
class Scheduler {
 public:
  virtual ~Scheduler() = default;

  // Human-readable name, e.g. "FCFS" or "RR (q=4)".
  virtual std::string name() const = 0;

  // Decide who runs when. Input processes are already validated
  // (unique ids, arrival >= 0, burst > 0). Return the timeline in time order;
  // use kIdle slices for gaps where the CPU has nothing to do.
  virtual std::vector<Slice> schedule(const std::vector<Process>& procs) const = 0;

 protected:
  // Add [start, end) to the timeline, merging with the previous slice when it
  // belongs to the same pid and touches it. Zero-length slices are ignored.
  static void append(std::vector<Slice>& timeline, int pid, int start, int end);

  // Indices of `procs` sorted by (arrival, id).
  static std::vector<std::size_t> order_by_arrival(const std::vector<Process>& procs);
};

}  // namespace cpusim
