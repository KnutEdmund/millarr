#include <unity.h>
#include <vector>

#include "pulse_log.h"
#include "speed_engine.h"
#include "speed_types.h"
#include "synth.h"

using namespace speed_core;

static PulseRecord rec(uint32_t seq) {
  PulseRecord r{};
  r.seq = seq;
  r.t_us = seq * 1000u;
  return r;
}

void setUp() {}
void tearDown() {}

// --- PulseLog ---

void test_log_starts_empty() {
  PulseLog log;
  TEST_ASSERT_EQUAL_UINT32(0, log.size());
  TEST_ASSERT_EQUAL_UINT32(0, log.lastSeq());
  TEST_ASSERT_EQUAL_UINT32(0, log.overwritten());
  TEST_ASSERT_EQUAL_UINT32(LOG_CAPACITY, log.capacity());
}

void test_log_push_and_at_oldest_first() {
  PulseLog log;
  log.push(rec(1));
  log.push(rec(2));
  log.push(rec(3));
  TEST_ASSERT_EQUAL_UINT32(3, log.size());
  TEST_ASSERT_EQUAL_UINT32(1, log.at(0).seq);
  TEST_ASSERT_EQUAL_UINT32(3, log.at(2).seq);
  TEST_ASSERT_EQUAL_UINT32(3, log.lastSeq());
}

void test_log_wraps_and_counts_overwritten() {
  PulseLog log;
  for (uint32_t i = 1; i <= LOG_CAPACITY + 10; ++i) log.push(rec(i));
  TEST_ASSERT_EQUAL_UINT32(LOG_CAPACITY, log.size());
  TEST_ASSERT_EQUAL_UINT32(10, log.overwritten());
  TEST_ASSERT_EQUAL_UINT32(11, log.at(0).seq);                  // oldest surviving
  TEST_ASSERT_EQUAL_UINT32(LOG_CAPACITY + 10, log.lastSeq());
}

void test_log_since_returns_tail_in_order_capped() {
  PulseLog log;
  for (uint32_t i = 1; i <= LOG_CAPACITY + 10; ++i) log.push(rec(i));
  std::vector<uint32_t> seen;
  size_t n = log.since(LOG_CAPACITY + 5, 100, [&](const PulseRecord& r) { seen.push_back(r.seq); });
  TEST_ASSERT_EQUAL_UINT32(5, n);
  TEST_ASSERT_EQUAL_UINT32(LOG_CAPACITY + 6, seen.front());
  TEST_ASSERT_EQUAL_UINT32(LOG_CAPACITY + 10, seen.back());

  seen.clear();
  n = log.since(0, 3, [&](const PulseRecord& r) { seen.push_back(r.seq); });
  TEST_ASSERT_EQUAL_UINT32(3, n);
  TEST_ASSERT_EQUAL_UINT32(11, seen.front());  // cap applies from the oldest
}

void test_log_since_older_than_retained_returns_everything() {
  PulseLog log;
  for (uint32_t i = 1; i <= LOG_CAPACITY + 10; ++i) log.push(rec(i));
  size_t n = log.since(3, 100000, [](const PulseRecord&) {});
  TEST_ASSERT_EQUAL_UINT32(LOG_CAPACITY, n);
}

void test_log_clear() {
  PulseLog log;
  log.push(rec(1));
  log.clear();
  TEST_ASSERT_EQUAL_UINT32(0, log.size());
  TEST_ASSERT_EQUAL_UINT32(0, log.lastSeq());
  TEST_ASSERT_EQUAL_UINT32(0, log.overwritten());
}

static EngineConfig defaultConfig() {
  return EngineConfig{2.82f, 3, 50000u, 3000000u};
}

static void feed(SpeedEngine& e, const std::vector<uint32_t>& ts) {
  for (uint32_t t : ts) e.onPulse(t);
}

// --- SpeedEngine ---

void test_engine_constant_speed_perfect_marks() {
  SpeedEngine e(defaultConfig());
  const double v = 10.0 / 3.6;  // 10 km/h
  auto ts = synthConstant(v, 30.0);
  feed(e, ts);
  const PulseRecord& last = e.log().at(e.log().size() - 1);
  TEST_ASSERT_FLOAT_WITHIN(v * 0.001, v, last.mark_speed_mps);
  TEST_ASSERT_FLOAT_WITHIN(v * 0.001, v, last.rev_speed_mps);
  TEST_ASSERT_FLOAT_WITHIN(v * 0.001, v, e.speedAt(ts.back() + 100000));
  TEST_ASSERT_TRUE(e.running(ts.back() + 100000));
}

void test_engine_first_pulses_have_no_rev_speed() {
  SpeedEngine e(defaultConfig());
  auto ts = synthConstant(2.0, 10.0);
  e.onPulse(ts[0]);
  TEST_ASSERT_EQUAL_UINT32(0, e.log().at(0).interval_us);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, e.log().at(0).mark_speed_mps);
  e.onPulse(ts[1]);
  e.onPulse(ts[2]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, e.log().at(2).rev_speed_mps);  // only 2 intervals so far
  TEST_ASSERT_EQUAL_FLOAT(0.0f, e.speedAt(ts[2] + 1));
  e.onPulse(ts[3]);
  TEST_ASSERT_FLOAT_WITHIN(0.002f, 2.0f, e.log().at(3).rev_speed_mps);  // 3 intervals = 1 rev
}

void test_engine_mark_index_is_run_relative() {
  SpeedEngine e(defaultConfig());
  auto ts = synthConstant(2.0, 10.0);
  feed(e, ts);
  for (size_t i = 0; i < e.log().size(); ++i) {
    TEST_ASSERT_EQUAL_UINT8(i % 3, e.log().at(i).mark_index);
  }
}

void test_engine_debounce_rejects_bounce() {
  SpeedEngine e(defaultConfig());
  e.onPulse(1000000);
  e.onPulse(1001000);  // 1 ms later: bounce
  e.onPulse(1400000);  // 400 ms later: real
  TEST_ASSERT_EQUAL_UINT32(2, e.log().size());
  TEST_ASSERT_EQUAL_UINT32(400000, e.log().at(1).interval_us);
  TEST_ASSERT_EQUAL_UINT32(1, e.status(1400001).rejected_debounce);
}

void test_engine_stop_timeout_and_restart() {
  SpeedEngine e(defaultConfig());
  auto ts = synthConstant(2.0, 5.0);
  feed(e, ts);
  uint32_t last = ts.back();
  TEST_ASSERT_TRUE(e.running(last + 2999999));
  TEST_ASSERT_FALSE(e.running(last + 3000001));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, e.speedAt(last + 3000001));
  TEST_ASSERT_FALSE(e.status(last + 3000001).running);

  uint32_t restart = last + 5000000;
  e.onPulse(restart);
  const PulseRecord& r = e.log().at(e.log().size() - 1);
  TEST_ASSERT_EQUAL_UINT32(0, r.interval_us);
  TEST_ASSERT_EQUAL_UINT8(0, r.mark_index);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, r.rev_speed_mps);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, e.speedAt(restart + 1));  // rev window restarted
}

void test_engine_distance_counts_intervals() {
  SpeedEngine e(defaultConfig());
  auto ts = synthConstant(2.0, 10.0);
  feed(e, ts);
  float expected = (ts.size() - 1) * (2.82f / 3.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, e.status(ts.back()).distance_m);
  TEST_ASSERT_EQUAL_UINT32(ts.size(), e.status(ts.back()).pulse_count);
  TEST_ASSERT_EQUAL_UINT32((ts.size() - 1) / 3, e.status(ts.back()).revolution_count);
}

void test_engine_micros_wrap() {
  SpeedEngine e(defaultConfig());
  SynthOptions o;
  o.start_us = 0xFFFFFFFFu - 500000u;  // wraps during the run
  const double v = 10.0 / 3.6;
  auto ts = synthConstant(v, 5.0, o);
  feed(e, ts);
  TEST_ASSERT_TRUE(ts.back() < ts.front());  // proves the wrap happened
  const PulseRecord& last = e.log().at(e.log().size() - 1);
  TEST_ASSERT_FLOAT_WITHIN(v * 0.001, v, last.rev_speed_mps);
  TEST_ASSERT_TRUE(e.running(ts.back() + 1000));
}

void test_engine_reset_clears_everything() {
  SpeedEngine e(defaultConfig());
  feed(e, synthConstant(2.0, 5.0));
  e.reset();
  EngineStatus s = e.status(100);
  TEST_ASSERT_EQUAL_UINT32(0, s.pulse_count);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, s.distance_m);
  TEST_ASSERT_EQUAL_UINT32(0, e.log().size());
  TEST_ASSERT_FALSE(s.running);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_log_starts_empty);
  RUN_TEST(test_log_push_and_at_oldest_first);
  RUN_TEST(test_log_wraps_and_counts_overwritten);
  RUN_TEST(test_log_since_returns_tail_in_order_capped);
  RUN_TEST(test_log_since_older_than_retained_returns_everything);
  RUN_TEST(test_log_clear);
  RUN_TEST(test_engine_constant_speed_perfect_marks);
  RUN_TEST(test_engine_first_pulses_have_no_rev_speed);
  RUN_TEST(test_engine_mark_index_is_run_relative);
  RUN_TEST(test_engine_debounce_rejects_bounce);
  RUN_TEST(test_engine_stop_timeout_and_restart);
  RUN_TEST(test_engine_distance_counts_intervals);
  RUN_TEST(test_engine_micros_wrap);
  RUN_TEST(test_engine_reset_clears_everything);
  return UNITY_END();
}
