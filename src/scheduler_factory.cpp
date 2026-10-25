#include "scheduler_factory.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "fcfs.hpp"
#include "round_robin.hpp"
#include "sjf.hpp"

namespace cpusim {

std::unique_ptr<Scheduler> make_scheduler(const std::string& name, int quantum) {
  std::string key = name;
  std::transform(key.begin(), key.end(), key.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (key == "fcfs") return std::make_unique<FCFS>();
  if (key == "rr") return std::make_unique<RoundRobin>(quantum);
  if (key == "sjf") return std::make_unique<SJF>();
  throw std::invalid_argument("unknown algorithm '" + name + "' (use fcfs, rr or sjf)");
}

}  // namespace cpusim
