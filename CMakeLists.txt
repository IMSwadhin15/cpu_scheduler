cmake_minimum_required(VERSION 3.13)
project(CpuScheduler VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to an optimised build so the timing numbers mean something.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -pthread)

# Optional sanitizer build:  cmake -B build-tsan -DSANITIZE=thread
set(SANITIZE "" CACHE STRING "Sanitizer to enable (thread, address, undefined)")
if(SANITIZE)
  add_compile_options(-fsanitize=${SANITIZE} -g -fno-omit-frame-pointer)
  add_link_options(-fsanitize=${SANITIZE})
endif()

find_package(Threads REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include)

enable_testing()

# Core library: Process, schedulers, simulator, reports, workloads.
add_library(sched_core STATIC
  src/scheduler.cpp
  src/fcfs.cpp
  src/sjf.cpp
  src/round_robin.cpp
  src/scheduler_factory.cpp
  src/simulator.cpp
  src/report.cpp
  src/workload.cpp)
target_link_libraries(sched_core PUBLIC Threads::Threads)

# Command-line simulator.
add_executable(cpu_sched src/main.cpp)
target_link_libraries(cpu_sched PRIVATE sched_core)
