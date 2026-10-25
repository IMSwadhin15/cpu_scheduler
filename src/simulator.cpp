#include "simulator.hpp"

#include <stdexcept>
#include <unordered_map>

namespace cpusim {

namespace {

void validate_input(const std::vector<Process>& procs) {
  std::unordered_map<int, bool> seen;
  for (const Process& p : procs) {
    if (p.id < 0) throw std::invalid_argument("process id must be >= 0");
    if (p.arrival < 0) throw std::invalid_argument("process arrival must be >= 0");
    if (p.burst <= 0) throw std::invalid_argument("process burst must be > 0");
    if (!seen.emplace(p.id, true).second) {
      throw std::invalid_argument("duplicate process id " + std::to_string(p.id));
    }
  }
}

}  // namespace

SimResult simulate(const Scheduler& scheduler, std::vector<Process> procs) {
  validate_input(procs);

  SimResult result;
  result.algorithm = scheduler.name();
  result.timeline = scheduler.schedule(procs);

  std::unordered_map<int, std::size_t> index;  // process id -> position in procs
  for (std::size_t i = 0; i < procs.size(); ++i) index[procs[i].id] = i;

  std::vector<int> cpu_time(procs.size(), 0);
  int prev_end = 0;
  for (const Slice& s : result.timeline) {
    if (s.start < prev_end || s.end <= s.start) {
      throw std::logic_error("scheduler returned an overlapping or empty slice");
    }
    prev_end = s.end;
    if (s.pid == kIdle) continue;

    auto it = index.find(s.pid);
    if (it == index.end()) throw std::logic_error("timeline mentions an unknown process");
    Process& p = procs[it->second];
    if (s.start < p.arrival) throw std::logic_error("process ran before it arrived");

    if (p.start < 0) p.start = s.start;  // first time on the CPU
    p.finish = s.end;                    // keeps being overwritten until the last slice
    cpu_time[it->second] += s.end - s.start;
  }

  double sum_wait = 0, sum_turn = 0, sum_resp = 0;
  for (std::size_t i = 0; i < procs.size(); ++i) {
    Process& p = procs[i];
    if (cpu_time[i] != p.burst) throw std::logic_error("a process did not get exactly its burst time");
    p.turnaround = p.finish - p.arrival;
    p.waiting = p.turnaround - p.burst;
    p.response = p.start - p.arrival;
    sum_wait += p.waiting;
    sum_turn += p.turnaround;
    sum_resp += p.response;
    if (p.finish > result.makespan) result.makespan = p.finish;
  }

  if (!procs.empty()) {
    const double n = static_cast<double>(procs.size());
    result.avg_waiting = sum_wait / n;
    result.avg_turnaround = sum_turn / n;
    result.avg_response = sum_resp / n;
  }
  result.processes = std::move(procs);
  return result;
}

}  // namespace cpusim
