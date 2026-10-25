#pragma once

// Tests use plain assert(). Release builds define NDEBUG, which would turn
// every assert into a no-op and make the tests pass vacuously, so undo that
// here. Include this header BEFORE anything else in a test file.
#undef NDEBUG
#include <cassert>

#include <cmath>
#include <iostream>

// Run one named test function and print a line for it.
#define RUN_TEST(fn)                                  \
  do {                                                \
    std::cout << "  " #fn " ... " << std::flush;      \
    fn();                                             \
    std::cout << "ok\n";                              \
  } while (0)

inline bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }
