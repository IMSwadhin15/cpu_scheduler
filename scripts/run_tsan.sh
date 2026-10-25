#!/usr/bin/env bash
# Build everything with ThreadSanitizer and run the tests plus a small parallel
# batch. Exits non-zero if ThreadSanitizer reports a data race.
set -euo pipefail
cd "$(dirname "$0")/.."

cmake -B build-tsan -DSANITIZE=thread -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-tsan
ctest --test-dir build-tsan --output-on-failure
./build-tsan/compare --workloads 100 --procs 50 --repeats 1 --csv build-tsan/tsan_timings.csv
echo "ThreadSanitizer: no data races reported"
