#pragma once

#include <memory>
#include <string>

#include "scheduler.hpp"

namespace cpusim {

// Build a scheduler from a command-line name: "fcfs", "rr" or "sjf"
// (case-insensitive). `quantum` is only used by "rr".
// Throws std::invalid_argument for an unknown name.
// Adding a new algorithm = write the subclass + add one line in the .cpp.
std::unique_ptr<Scheduler> make_scheduler(const std::string& name, int quantum);

}  // namespace cpusim
