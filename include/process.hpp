#pragma once

// One job for the CPU. The first three fields are the input; the rest are
// filled in by the Simulator after a schedule has been computed.

namespace cpusim {

class Process {
 public:
  Process() = default;
  Process(int id_, int arrival_, int burst_)
      : id(id_), arrival(arrival_), burst(burst_) {}

  // ---- input ----
  int id = 0;       // unique label shown in reports and the Gantt chart
  int arrival = 0;  // time the process becomes ready
  int burst = 0;    // total CPU time needed (must be > 0)

  // ---- computed by the Simulator ----
  int start = -1;      // first time the process got the CPU
  int finish = -1;     // time the process completed
  int waiting = 0;     // time spent ready but not running = turnaround - burst
  int turnaround = 0;  // finish - arrival
  int response = 0;    // start - arrival (how long until first CPU time)
};

}  // namespace cpusim
