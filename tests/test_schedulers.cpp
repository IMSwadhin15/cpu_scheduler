#include "test_common.hpp"

#include <sstream>
#include <stdexcept>
#include <vector>

#include "fcfs.hpp"
#include "report.hpp"
#include "round_robin.hpp"
#include "scheduler_factory.hpp"
#include "simulator.hpp"
#include "sjf.hpp"
#include "workload.hpp"

using namespace cpusim;

// The hand-calculated example used throughout the docs:
//   P1 arrives 0 burst 5, P2 arrives 1 burst 3, P3 arrives 2 burst 8, P4 arrives 3 burst 6
static std::vector<Process> classic() {
  return {Process(1, 0, 5), Process(2, 1, 3), Process(3, 2, 8), Process(4, 3, 6)};
}

static const Process& find(const SimResult& r, int id) {
  for (const Process& p : r.processes) {
    if (p.id == id) return p;
  }
  assert(false && "process not found");
  return r.processes.front();
}

static void check_times(const SimResult& r, int id, int start, int finish, int waiting,
                        int turnaround, int response) {
  const Process& p = find(r, id);
  assert(p.start == start);
  assert(p.finish == finish);
  assert(p.waiting == waiting);
  assert(p.turnaround == turnaround);
  assert(p.response == response);
}

// FCFS: P1 0-5, P2 5-8, P3 8-16, P4 16-22
// waiting 0,4,6,13 (avg 5.75); turnaround 5,7,14,19 (avg 11.25); response = waiting
static void fcfs_hand_calculated() {
  SimResult r = simulate(FCFS(), classic());
  check_times(r, 1, 0, 5, 0, 5, 0);
  check_times(r, 2, 5, 8, 4, 7, 4);
  check_times(r, 3, 8, 16, 6, 14, 6);
  check_times(r, 4, 16, 22, 13, 19, 13);
  assert(near(r.avg_waiting, 5.75));
  assert(near(r.avg_turnaround, 11.25));
  assert(near(r.avg_response, 5.75));
  assert(r.makespan == 22);
}

// SJF: P1 0-5, then of {P2:3, P3:8, P4:6} -> P2 5-8, P4 8-14, P3 14-22
// waiting 0,4,12,5 (avg 5.25); turnaround 5,7,20,11 (avg 10.75)
static void sjf_hand_calculated() {
  SimResult r = simulate(SJF(), classic());
  check_times(r, 1, 0, 5, 0, 5, 0);
  check_times(r, 2, 5, 8, 4, 7, 4);
  check_times(r, 3, 14, 22, 12, 20, 12);
  check_times(r, 4, 8, 14, 5, 11, 5);
  assert(near(r.avg_waiting, 5.25));
  assert(near(r.avg_turnaround, 10.75));
  assert(near(r.avg_response, 5.25));
}

// RR, quantum 2. Timeline:
//   P1 0-2, P2 2-4, P3 4-6, P1 6-8, P4 8-10, P2 10-11, P3 11-13, P1 13-14,
//   P4 14-16, P3 16-18, P4 18-20, P3 20-22
// finish: P1 14, P2 11, P3 22, P4 20
// waiting 9,7,12,11 (avg 9.75); turnaround 14,10,20,17 (avg 15.25);
// response 0,1,2,5 (avg 2.0)
static void round_robin_hand_calculated() {
  SimResult r = simulate(RoundRobin(2), classic());
  check_times(r, 1, 0, 14, 9, 14, 0);
  check_times(r, 2, 2, 11, 7, 10, 1);
  check_times(r, 3, 4, 22, 12, 20, 2);
  check_times(r, 4, 8, 20, 11, 17, 5);
  assert(near(r.avg_waiting, 9.75));
  assert(near(r.avg_turnaround, 15.25));
  assert(near(r.avg_response, 2.0));

  const std::vector<int> expected_pids = {1, 2, 3, 1, 4, 2, 3, 1, 4, 3, 4, 3};
  assert(r.timeline.size() == expected_pids.size());
  for (std::size_t i = 0; i < expected_pids.size(); ++i) {
    assert(r.timeline[i].pid == expected_pids[i]);
  }
}

// A huge quantum means nobody is ever preempted: RR degenerates into FCFS.
static void round_robin_with_big_quantum_equals_fcfs() {
  SimResult rr = simulate(RoundRobin(1000), classic());
  SimResult fcfs = simulate(FCFS(), classic());
  assert(rr.timeline.size() == fcfs.timeline.size());
  for (std::size_t i = 0; i < rr.timeline.size(); ++i) {
    assert(rr.timeline[i].pid == fcfs.timeline[i].pid);
    assert(rr.timeline[i].start == fcfs.timeline[i].start);
    assert(rr.timeline[i].end == fcfs.timeline[i].end);
  }
}

// P1 arrives at 2 (CPU idle 0-2), runs 2-5. P2 arrives at 10 (idle 5-10), runs 10-12.
static void idle_gaps_are_handled_by_every_algorithm() {
  const std::vector<Process> procs = {Process(1, 2, 3), Process(2, 10, 2)};
  FCFS fcfs;
  SJF sjf;
  RoundRobin rr(2);
  const Scheduler* all[] = {&fcfs, &sjf, &rr};
  for (const Scheduler* s : all) {
    SimResult r = simulate(*s, procs);
    check_times(r, 1, 2, 5, 0, 3, 0);
    check_times(r, 2, 10, 12, 0, 2, 0);
    assert(r.timeline.size() == 4);
    assert(r.timeline[0].pid == kIdle && r.timeline[0].start == 0 && r.timeline[0].end == 2);
    assert(r.timeline[2].pid == kIdle && r.timeline[2].start == 5 && r.timeline[2].end == 10);
  }
}

// Both ready at once: SJF takes the shorter job even though it has the larger id;
// equal bursts fall back to earlier arrival, then smaller id.
static void sjf_picks_shortest_and_breaks_ties() {
  {
    SimResult r = simulate(SJF(), {Process(1, 0, 4), Process(2, 1, 9), Process(3, 1, 2)});
    // P1 0-4, then P3 (2) before P2 (9)
    assert(find(r, 3).start == 4);
    assert(find(r, 2).start == 6);
  }
  {
    SimResult r = simulate(SJF(), {Process(1, 0, 4), Process(7, 2, 3), Process(5, 2, 3)});
    // P1 0-4; P5 and P7 tie on burst and arrival -> smaller id (5) first
    assert(find(r, 5).start == 4);
    assert(find(r, 7).start == 7);
  }
}

// A process arriving exactly when a quantum ends is queued before the one that was preempted.
static void round_robin_arrival_at_quantum_boundary() {
  // P1 (burst 4) runs 0-2; P2 arrives at 2 and must go before P1's second turn.
  SimResult r = simulate(RoundRobin(2), {Process(1, 0, 4), Process(2, 2, 2)});
  assert(r.timeline.size() == 3);
  assert(r.timeline[0].pid == 1);
  assert(r.timeline[1].pid == 2);  // P2 ahead of P1's remainder
  assert(r.timeline[2].pid == 1);
}

// SJF has the lowest average waiting time of the non-preemptive algorithms on
// this example, and RR the best response time: sanity-check the orderings.
static void expected_algorithm_orderings() {
  SimResult f = simulate(FCFS(), classic());
  SimResult s = simulate(SJF(), classic());
  SimResult r = simulate(RoundRobin(2), classic());
  assert(s.avg_waiting < f.avg_waiting);
  assert(r.avg_response < f.avg_response);
  assert(r.avg_response < s.avg_response);
}

static void empty_workload_is_fine() {
  SimResult r = simulate(FCFS(), {});
  assert(r.processes.empty());
  assert(r.timeline.empty());
  assert(near(r.avg_waiting, 0));
}

static void bad_input_is_rejected() {
  auto throws_invalid = [](std::vector<Process> procs) {
    try {
      simulate(FCFS(), std::move(procs));
    } catch (const std::invalid_argument&) {
      return true;
    }
    return false;
  };
  assert(throws_invalid({Process(1, 0, 0)}));                   // zero burst
  assert(throws_invalid({Process(1, -1, 3)}));                  // negative arrival
  assert(throws_invalid({Process(1, 0, 3), Process(1, 1, 2)})); // duplicate id

  bool threw = false;
  try {
    RoundRobin bad(0);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

// Shows the "new algorithm = new subclass" claim: a made-up LIFO-ish scheduler
// that only implements name() and schedule(), and still gets full metrics.
class LastArrivedFirst : public Scheduler {
 public:
  std::string name() const override { return "LAF"; }
  std::vector<Slice> schedule(const std::vector<Process>& procs) const override {
    std::vector<Slice> t;
    int now = 0;
    std::vector<bool> done(procs.size(), false);
    for (std::size_t n = 0; n < procs.size(); ++n) {
      std::size_t pick = procs.size();
      int next_arrival = 1 << 30;
      for (std::size_t i = 0; i < procs.size(); ++i) {
        if (done[i]) continue;
        if (procs[i].arrival <= now) {
          if (pick == procs.size() || procs[i].arrival > procs[pick].arrival) pick = i;
        } else if (procs[i].arrival < next_arrival) {
          next_arrival = procs[i].arrival;
        }
      }
      if (pick == procs.size()) {
        append(t, kIdle, now, next_arrival);
        now = next_arrival;
        --n;
        continue;
      }
      append(t, procs[pick].id, now, now + procs[pick].burst);
      now += procs[pick].burst;
      done[pick] = true;
    }
    return t;
  }
};

static void new_algorithm_needs_only_a_subclass() {
  SimResult r = simulate(LastArrivedFirst(), classic());
  // P1 0-5, then newest arrival first: P4 5-11, P3 11-19, P2 19-22
  check_times(r, 1, 0, 5, 0, 5, 0);
  check_times(r, 4, 5, 11, 2, 8, 2);
  check_times(r, 3, 11, 19, 9, 17, 9);
  check_times(r, 2, 19, 22, 18, 21, 18);
}

static void gantt_chart_text() {
  SimResult r = simulate(FCFS(), {Process(1, 0, 5), Process(2, 7, 3)});
  const std::string g = format_gantt(r.timeline);
  // Boxes: P1 (0-5), idle (5-7), P2 (7-10)
  assert(g.find("P1") != std::string::npos);
  assert(g.find("idle") != std::string::npos);
  assert(g.find("P2") != std::string::npos);
  const std::size_t nl = g.find('\n');
  const std::string marks = g.substr(nl + 1);
  assert(marks.find('0') == 0);
  assert(marks.find("10") != std::string::npos);
  assert(format_gantt({}) == "(empty timeline)\n");
}

static void workload_file_parsing() {
  std::istringstream good("# comment\n1 0 5\n\n2 1 3   # trailing comment\n");
  std::vector<Process> p = parse_workload(good);
  assert(p.size() == 2);
  assert(p[0].id == 1 && p[0].arrival == 0 && p[0].burst == 5);
  assert(p[1].id == 2 && p[1].arrival == 1 && p[1].burst == 3);

  auto parse_fails = [](const char* text) {
    std::istringstream in(text);
    try {
      parse_workload(in);
    } catch (const std::runtime_error&) {
      return true;
    }
    return false;
  };
  assert(parse_fails("1 0\n"));        // missing burst
  assert(parse_fails("1 0 x\n"));      // not a number
  assert(parse_fails("1 0 5 9\n"));    // extra field

  bool threw = false;
  try {
    load_workload("this/file/does/not/exist.txt");
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
}

static void generator_is_seeded_and_in_range() {
  GeneratorOptions o;
  o.count = 50;
  o.seed = 42;
  o.max_arrival = 30;
  o.max_burst = 8;
  std::vector<Process> a = generate_workload(o);
  std::vector<Process> b = generate_workload(o);
  assert(a.size() == 50);
  for (std::size_t i = 0; i < a.size(); ++i) {
    assert(a[i].id == static_cast<int>(i) + 1);
    assert(a[i].arrival == b[i].arrival && a[i].burst == b[i].burst);  // same seed, same data
    assert(a[i].arrival >= 0 && a[i].arrival <= 30);
    assert(a[i].burst >= 1 && a[i].burst <= 8);
  }
  o.seed = 43;
  std::vector<Process> c = generate_workload(o);
  bool differs = false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].arrival != c[i].arrival || a[i].burst != c[i].burst) differs = true;
  }
  assert(differs);
}

static void factory_builds_schedulers() {
  assert(make_scheduler("FCFS", 4)->name() == "FCFS");
  assert(make_scheduler("sjf", 4)->name() == "SJF");
  assert(make_scheduler("rr", 3)->name() == "RR (q=3)");
  bool threw = false;
  try {
    make_scheduler("lottery", 4);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

// On random workloads every algorithm must give each process exactly its burst
// (simulate() throws logic_error otherwise), and SJF should never have a higher
// average waiting time than FCFS when all processes arrive together.
static void random_workloads_are_consistent() {
  for (unsigned seed = 1; seed <= 200; ++seed) {
    GeneratorOptions o;
    o.count = 30;
    o.seed = seed;
    o.max_arrival = 0;  // everyone arrives at 0 => SJF is provably optimal for waiting time
    o.max_burst = 12;
    std::vector<Process> procs = generate_workload(o);
    SimResult f = simulate(FCFS(), procs);
    SimResult s = simulate(SJF(), procs);
    SimResult r = simulate(RoundRobin(3), procs);
    assert(s.avg_waiting <= f.avg_waiting + 1e-9);
    assert(s.avg_waiting <= r.avg_waiting + 1e-9);
    assert(f.makespan == s.makespan && s.makespan == r.makespan);
  }
}

int main() {
  std::cout << "scheduler tests\n";
  RUN_TEST(fcfs_hand_calculated);
  RUN_TEST(sjf_hand_calculated);
  RUN_TEST(round_robin_hand_calculated);
  RUN_TEST(round_robin_with_big_quantum_equals_fcfs);
  RUN_TEST(idle_gaps_are_handled_by_every_algorithm);
  RUN_TEST(sjf_picks_shortest_and_breaks_ties);
  RUN_TEST(round_robin_arrival_at_quantum_boundary);
  RUN_TEST(expected_algorithm_orderings);
  RUN_TEST(empty_workload_is_fine);
  RUN_TEST(bad_input_is_rejected);
  RUN_TEST(new_algorithm_needs_only_a_subclass);
  RUN_TEST(gantt_chart_text);
  RUN_TEST(workload_file_parsing);
  RUN_TEST(generator_is_seeded_and_in_range);
  RUN_TEST(factory_builds_schedulers);
  RUN_TEST(random_workloads_are_consistent);
  std::cout << "all passed\n";
  return 0;
}
