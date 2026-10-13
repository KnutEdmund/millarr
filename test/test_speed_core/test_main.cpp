#include <unity.h>
#include <vector>
#include <string>
#include <cmath>
#include <cstring>

#include "pulse_log.h"
#include "speed_engine.h"
#include "speed_types.h"
#include "synth.h"
#include "format.h"
#include "profile.h"
#include "csv_batcher.h"
#include "session.h"
#include "rsc.h"

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
  return makeEqualConfig(2.82f, 3, 50000u, 3000000u);
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

void test_engine_stop_latched_across_wrap() {
  SpeedEngine e(defaultConfig());
  auto ts = synthConstant(2.0, 5.0);
  feed(e, ts);
  uint32_t last = ts.back();
  e.tick(last + 4000000u);                       // 4 s idle: stop latched
  TEST_ASSERT_FALSE(e.running(last + 4000000u));
  // 71.6 min later micros() has wrapped: to the engine this timestamp is
  // indistinguishable from "1 s after the last pulse".
  uint32_t wrapped = last + 1000000u;
  TEST_ASSERT_FALSE(e.running(wrapped));         // fails without the latch
  TEST_ASSERT_EQUAL_FLOAT(0.0f, e.speedAt(wrapped));
  e.onPulse(wrapped);                            // belt restarts inside the window
  const PulseRecord& r = e.log().at(e.log().size() - 1);
  TEST_ASSERT_EQUAL_UINT32(0, r.interval_us);
  TEST_ASSERT_EQUAL_UINT8(0, r.mark_index);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, r.rev_speed_mps);
}

// --- Statistics ---

void test_stats_uneven_marks_rev_speed_unaffected() {
  SpeedEngine e(defaultConfig());
  SynthOptions o;
  o.mark_fraction = {0.30, 0.35, 0.35};
  const double v = 12.0 / 3.6;
  auto ts = synthConstant(v, 60.0, o);
  feed(e, ts);
  EngineStatus s = e.status(ts.back());

  // Revolution speed cancels the spacing error.
  TEST_ASSERT_FLOAT_WITHIN(v * 0.001, v, s.speed_mps);
  TEST_ASSERT_FLOAT_WITHIN(v * 0.001, v, e.log().at(e.log().size() - 1).rev_speed_mps);

  // Mark speed does not: the interval ending at mark_index 1 covers fraction[0] = 0.30.
  bool saw_low = false, saw_high = false;
  for (size_t i = e.log().size() - 6; i < e.log().size(); ++i) {
    const PulseRecord& r = e.log().at(i);
    if (r.mark_index == 1) saw_low |= std::fabs(r.mark_speed_mps - v / 0.90) < v * 0.002;
    if (r.mark_index == 2) saw_high |= std::fabs(r.mark_speed_mps - v / 1.05) < v * 0.002;
  }
  TEST_ASSERT_TRUE(saw_low);
  TEST_ASSERT_TRUE(saw_high);

  // mark_share[m] is the share of the interval ending at mark m.
  TEST_ASSERT_EQUAL_UINT8(STATS_REVOLUTIONS, s.stats_revolutions);
  TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.35f, s.mark_share[0]);  // fraction[2]
  TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.30f, s.mark_share[1]);  // fraction[0]
  TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.35f, s.mark_share[2]);  // fraction[1]
  TEST_ASSERT_EQUAL_FLOAT(0.0f, s.mark_share[3]);            // unused slots stay 0
}

void test_stats_even_marks_share_is_one_third() {
  SpeedEngine e(defaultConfig());
  auto ts = synthConstant(3.0, 60.0);
  feed(e, ts);
  EngineStatus s = e.status(ts.back());
  for (int m = 0; m < 3; ++m) TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f / 3.0f, s.mark_share[m]);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.0f, s.rev_speed_stddev);
}

void test_stats_jitter_reflected_in_stddev() {
  SpeedEngine e(defaultConfig());
  SynthOptions o;
  o.jitter_frac = 0.01;  // 1 % Gaussian per interval
  const double v = 10.0 / 3.6;
  auto ts = synthConstant(v, 120.0, o);
  feed(e, ts);
  EngineStatus s = e.status(ts.back());
  // Three independent 1 % interval errors give ~0.58 % on the revolution; allow a wide band
  // because the window is only 10 revolutions.
  TEST_ASSERT_TRUE(s.rev_speed_stddev > v * 0.003);
  TEST_ASSERT_TRUE(s.rev_speed_stddev < v * 0.012);
}

void test_stats_window_survives_stop() {
  SpeedEngine e(defaultConfig());
  auto ts = synthConstant(3.0, 30.0);
  feed(e, ts);
  uint32_t restart = ts.back() + 10000000;  // 10 s gap: stop
  SynthOptions o;
  o.start_us = restart;
  auto ts2 = synthConstant(3.0, 2.0, o);  // ~6 pulses: about 2 revolutions
  feed(e, ts2);
  EngineStatus s = e.status(ts2.back());
  // Old revolutions stay in the window (stats are per reset, not per run) and the
  // restart pulse contributed no interval.
  TEST_ASSERT_EQUAL_UINT8(STATS_REVOLUTIONS, s.stats_revolutions);
  TEST_ASSERT_EQUAL_UINT32(ts.size() - 1 + ts2.size() - 1, static_cast<uint32_t>(s.distance_m / e.meanMarkDistance() + 0.5f));
}

// --- Formatting ---

void test_csv_header() {
  TEST_ASSERT_EQUAL_STRING("seq,t_us,interval_us,mark,mark_speed_mps,rev_speed_mps\n", kCsvHeader);
}

void test_csv_record_line() {
  PulseRecord r{};
  r.seq = 42;
  r.t_us = 123456789u;
  r.interval_us = 338400u;
  r.mark_index = 2;
  r.mark_speed_mps = 2.77778f;
  r.rev_speed_mps = 2.7f;
  char buf[128];
  int n = formatRecordCsv(r, buf, sizeof buf);
  TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof buf);
  TEST_ASSERT_EQUAL_STRING("42,123456789,338400,2,2.778,2.700\n", buf);
}

void test_json_record() {
  PulseRecord r{};
  r.seq = 7;
  r.t_us = 1000;
  r.interval_us = 0;
  r.mark_index = 0;
  char buf[192];
  int n = formatRecordJson(r, buf, sizeof buf);
  TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof buf);
  TEST_ASSERT_EQUAL_STRING(
      "{\"seq\":7,\"t_us\":1000,\"interval_us\":0,\"mark\":0,\"mark_speed_mps\":0.000,\"rev_speed_mps\":0.000}",
      buf);
}

void test_json_status_contains_all_fields() {
  EngineStatus s{};
  s.speed_mps = 2.5f;
  s.running = true;
  s.phase_locked = true;
  s.distance_m = 12.34f;
  s.pulse_count = 13;
  s.revolution_count = 4;
  s.rejected_debounce = 1;
  s.rejected_implausible = 0;
  s.mark_share[0] = 0.33f;
  s.mark_share[1] = 0.33f;
  s.mark_share[2] = 0.34f;
  s.rev_speed_stddev = 0.012f;
  s.stats_revolutions = 4;
  s.log_size = 13;
  s.log_capacity = 2048;
  s.log_overwritten = 0;
  s.last_seq = 13;
  EngineConfig c = makeEqualConfig(2.82f, 3, 50000u, 3000000u);
  StatusExtras x{2, 99, 1, "Gym A", {}, {}};
  x.flash.ok = true; x.flash.full = false; x.flash.total_bytes = 1441792; x.flash.used_bytes = 301056;
  x.flash.session_id = 12; x.flash.session_records = 1726; x.flash.session_bytes = 69040;
  x.flash.write_errors = 0; x.flash.sessions = 3;
  x.ble.advertising = false; x.ble.connected = true; x.ble.connects = 2;
  x.ble.notifications = 1234; x.ble.last_speed_mps = 2.5f;
  char buf[1024];
  int n = formatStatusJson(s, c, x, buf, sizeof buf);
  TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof buf);
  TEST_ASSERT_EQUAL_STRING(
      "{\"speed_mps\":2.500,\"speed_kmh\":9.000,\"running\":true,\"phase_locked\":true,\"distance_m\":12.340,"
      "\"pulse_count\":13,\"revolution_count\":4,\"rejected_debounce\":1,\"rejected_implausible\":0,\"isr_dropped\":2,"
      "\"mark_share\":[0.330,0.330,0.340],\"rev_speed_stddev\":0.0120,\"stats_revolutions\":4,"
      "\"log_size\":13,\"log_capacity\":2048,\"log_overwritten\":0,\"last_seq\":13,\"uptime_s\":99,"
      "\"flash\":{\"ok\":true,\"full\":false,\"total\":1441792,\"used\":301056,\"session_id\":12,"
      "\"session_records\":1726,\"session_bytes\":69040,\"write_errors\":0,\"sessions\":3},"
      "\"ble\":{\"advertising\":false,\"connected\":true,\"connects\":2,\"notifications\":1234,"
      "\"last_speed_mps\":2.500},"
      "\"profile\":{\"id\":1,\"name\":\"Gym A\"},"
      "\"config\":{\"marks_count\":3,\"mark_distance_m\":[0.940,0.940,0.940],\"belt_length_m\":2.820,"
      "\"debounce_us\":50000,\"stop_timeout_us\":3000000,\"max_speed_mps\":0.000}}",
      buf);
}

void test_json_profile() {
  Profile p = makeDefaultProfile("Home treadmill", 2.82f, 3);
  char buf[256];
  int n = formatProfileJson(p, 0, buf, sizeof buf);
  TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof buf);
  TEST_ASSERT_EQUAL_STRING(
      "{\"id\":0,\"name\":\"Home treadmill\",\"notes\":\"\",\"type\":\"belt\",\"marks\":3,"
      "\"mark_distance_m\":[0.940,0.940,0.940],\"slat_count\":0,\"slat_pitch_m\":0.00000,"
      "\"belt_length_m\":2.820}",
      buf);
}

void test_json_profile_notes_newline_escaped() {
  Profile p = makeDefaultProfile("Home treadmill", 2.82f, 3);
  std::strcpy(p.notes, "first\nsecond");
  char buf[256];
  int n = formatProfileJson(p, 0, buf, sizeof buf);
  TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof buf);
  TEST_ASSERT_EQUAL_STRING(
      "{\"id\":0,\"name\":\"Home treadmill\",\"notes\":\"first\\nsecond\",\"type\":\"belt\",\"marks\":3,"
      "\"mark_distance_m\":[0.940,0.940,0.940],\"slat_count\":0,\"slat_pitch_m\":0.00000,"
      "\"belt_length_m\":2.820}",
      buf);
}

void test_json_status_truncation_is_reported() {
  EngineStatus s{};
  EngineConfig c = makeEqualConfig(2.82f, 3, 50000u, 3000000u);
  StatusExtras x{0, 0, 0, "x", {}, {}};
  char buf[64];
  int n = formatStatusJson(s, c, x, buf, sizeof buf);
  TEST_ASSERT_TRUE(n >= (int)sizeof buf);   // caller must treat this as truncated
}

// --- Per-mark distances and phase lock ---

void test_config_belt_length_and_equal_config() {
  EngineConfig c = makeEqualConfig(2.82f, 3, 1, 2);
  TEST_ASSERT_EQUAL_UINT8(3, c.marks_count);
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.94f, c.mark_distance_m[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 2.82f, beltLength(c));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, c.mark_distance_m[3]);  // unused slots stay zero
  EngineConfig z = makeEqualConfig(2.82f, 0, 1, 2);
  TEST_ASSERT_EQUAL_UINT8(1, z.marks_count);
  EngineConfig big = makeEqualConfig(2.82f, 20, 1, 2);
  TEST_ASSERT_EQUAL_UINT8(MAX_MARKS, big.marks_count);
}

void test_engine_phase_alignment_uneven_marks() {
  const float d[3] = {0.90f, 1.00f, 0.92f};  // sums to 2.82
  const double v = 10.0 / 3.6;
  for (int start = 0; start < 3; ++start) {
    EngineConfig c = defaultConfig();
    for (int k = 0; k < 3; ++k) c.mark_distance_m[k] = d[k];
    SpeedEngine e(c);
    SynthOptions o;
    // The synthetic belt starts with physical mark `start` passing the sensor.
    o.mark_fraction = {d[start % 3] / 2.82, d[(start + 1) % 3] / 2.82, d[(start + 2) % 3] / 2.82};
    auto ts = synthConstant(v, 30.0, o);
    feed(e, ts);
    EngineStatus s = e.status(ts.back());
    TEST_ASSERT_TRUE(s.phase_locked);
    TEST_ASSERT_FLOAT_WITHIN(v * 0.001, v, s.speed_mps);
    // From the pulse that completes the first revolution on, indices are physical
    // and mark speed uses the physical distance.
    for (size_t i = 3; i < e.log().size(); ++i) {
      const PulseRecord& r = e.log().at(i);
      TEST_ASSERT_EQUAL_UINT8((i + start) % 3, r.mark_index);
      TEST_ASSERT_FLOAT_WITHIN(v * 0.001, v, r.mark_speed_mps);
    }
    // mark_share[p] is the share of the interval ending at physical mark p = d[p-1] / belt.
    for (int p = 0; p < 3; ++p) {
      TEST_ASSERT_FLOAT_WITHIN(0.002f, d[(p + 2) % 3] / 2.82f, s.mark_share[p]);
    }
  }
}

void test_engine_first_revolution_distance_is_belt_length() {
  const float d[3] = {0.90f, 1.00f, 0.92f};
  EngineConfig c = defaultConfig();
  for (int k = 0; k < 3; ++k) c.mark_distance_m[k] = d[k];
  SpeedEngine e(c);
  SynthOptions o;
  o.mark_fraction = {d[0] / 2.82, d[1] / 2.82, d[2] / 2.82};
  auto ts = synthConstant(2.0, 10.0, o);
  for (size_t i = 0; i < 4; ++i) e.onPulse(ts[i]);   // exactly one revolution: 3 intervals
  TEST_ASSERT_TRUE(e.status(ts[3]).phase_locked);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.82f, e.status(ts[3]).distance_m);
}

void test_engine_phase_not_locked_before_first_revolution() {
  SpeedEngine e(defaultConfig());
  auto ts = synthConstant(2.0, 10.0);
  e.onPulse(ts[0]);
  e.onPulse(ts[1]);
  e.onPulse(ts[2]);
  TEST_ASSERT_FALSE(e.status(ts[2]).phase_locked);
  e.onPulse(ts[3]);
  TEST_ASSERT_TRUE(e.status(ts[3]).phase_locked);
}

void test_engine_one_and_two_marks() {
  for (uint8_t marks = 1; marks <= 2; ++marks) {
    SpeedEngine e(makeEqualConfig(2.82f, marks, 50000u, 3000000u));
    SynthOptions o;
    o.marks = marks;
    const double v = 8.0 / 3.6;
    auto ts = synthConstant(v, 30.0, o);
    feed(e, ts);
    EngineStatus s = e.status(ts.back());
    TEST_ASSERT_TRUE(s.phase_locked);
    TEST_ASSERT_FLOAT_WITHIN(v * 0.001, v, s.speed_mps);
    TEST_ASSERT_EQUAL_UINT32((ts.size() - 1) / marks, s.revolution_count);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, (ts.size() - 1) * (2.82f / marks), s.distance_m);
  }
}

void test_engine_reconfigure_keeps_log_and_distance() {
  SpeedEngine e(defaultConfig());
  auto ts = synthConstant(2.0, 20.0);
  feed(e, ts);
  EngineStatus before = e.status(ts.back());
  TEST_ASSERT_TRUE(before.stats_revolutions > 0);

  e.reconfigure(makeEqualConfig(3.0f, 2, 50000u, 3000000u));
  EngineStatus s = e.status(ts.back());
  TEST_ASSERT_EQUAL_UINT8(2, e.config().marks_count);
  TEST_ASSERT_EQUAL_UINT8(0, s.stats_revolutions);
  TEST_ASSERT_FALSE(s.phase_locked);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, s.speed_mps);
  TEST_ASSERT_EQUAL_UINT32(before.pulse_count, s.pulse_count);
  TEST_ASSERT_EQUAL_UINT32(before.log_size, s.log_size);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, before.distance_m, s.distance_m);

  e.onPulse(ts.back() + 500000u);
  const PulseRecord& r = e.log().at(e.log().size() - 1);
  TEST_ASSERT_EQUAL_UINT32(0, r.interval_us);
  TEST_ASSERT_EQUAL_UINT8(0, r.mark_index);
}

void test_stats_window_cleared_when_phase_changes_after_restart() {
  const float d[3] = {0.90f, 1.00f, 0.92f};
  EngineConfig c = defaultConfig();
  for (int k = 0; k < 3; ++k) c.mark_distance_m[k] = d[k];
  SpeedEngine e(c);
  SynthOptions o;
  o.mark_fraction = {d[0] / 2.82, d[1] / 2.82, d[2] / 2.82};  // run 1 starts at physical mark 0
  auto ts = synthConstant(2.0, 60.0, o);
  feed(e, ts);
  TEST_ASSERT_EQUAL_UINT8(STATS_REVOLUTIONS, e.status(ts.back()).stats_revolutions);

  SynthOptions o2;
  o2.start_us = ts.back() + 10000000u;                          // 10 s pause: stop
  o2.mark_fraction = {d[1] / 2.82, d[2] / 2.82, d[0] / 2.82};  // run 2 starts at physical mark 1
  auto ts2 = synthConstant(2.0, 6.0, o2);                      // about 4 revolutions
  feed(e, ts2);
  EngineStatus s = e.status(ts2.back());
  TEST_ASSERT_TRUE(s.phase_locked);
  TEST_ASSERT_TRUE(s.stats_revolutions < STATS_REVOLUTIONS);   // window restarted with the new phase
  for (int p = 0; p < 3; ++p) {
    TEST_ASSERT_FLOAT_WITHIN(0.002f, d[(p + 2) % 3] / 2.82f, s.mark_share[p]);  // per physical mark, not blended
  }
}

// --- Profile ---

void test_profile_default_is_valid_and_equal() {
  Profile p = makeDefaultProfile("Home treadmill", 2.82f, 3);
  TEST_ASSERT_NULL(validate(p));
  TEST_ASSERT_EQUAL_UINT8(PROFILE_VERSION, p.version);
  TEST_ASSERT_EQUAL_STRING("Home treadmill", p.name);
  TEST_ASSERT_EQUAL_UINT8(3, p.marks);
  TEST_ASSERT_TRUE(p.type == TreadmillType::Belt);
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.94f, p.mark_distance_m[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 2.82f, beltLength(p));
}

void test_profile_validate_rejects_bad_values() {
  Profile p = makeDefaultProfile("X", 2.82f, 3);
  Profile q = p; q.marks = 0;              TEST_ASSERT_EQUAL_STRING("marks must be 1..8", validate(q));
  q = p; q.marks = 9;                      TEST_ASSERT_EQUAL_STRING("marks must be 1..8", validate(q));
  q = p; q.mark_distance_m[1] = 0.01f;     TEST_ASSERT_EQUAL_STRING("distance out of range", validate(q));
  q = p; q.mark_distance_m[2] = 11.0f;     TEST_ASSERT_EQUAL_STRING("distance out of range", validate(q));
  q = p; q.name[0] = '\0';                 TEST_ASSERT_EQUAL_STRING("name empty", validate(q));
  q = p; q.version = 1;                    TEST_ASSERT_EQUAL_STRING("bad version", validate(q));
  q = p; q.name[0] = '"';                  TEST_ASSERT_EQUAL_STRING("name has invalid characters", validate(q));
  q = p; q.type = static_cast<TreadmillType>(7); TEST_ASSERT_EQUAL_STRING("bad type", validate(q));
  q = p; memset(q.name, 'a', PROFILE_NAME_LEN);  TEST_ASSERT_EQUAL_STRING("name too long", validate(q));
  // Distances beyond `marks` are ignored by validation.
  q = p; q.mark_distance_m[5] = 99.0f;     TEST_ASSERT_NULL(validate(q));
  q = p; memset(q.notes, 'x', PROFILE_NOTES_LEN);  TEST_ASSERT_EQUAL_STRING("notes too long", validate(q));
  q = p; std::strcpy(q.notes, "a\"b");     TEST_ASSERT_EQUAL_STRING("notes has invalid characters", validate(q));
  q = p; std::strcpy(q.notes, "line1\nline2"); TEST_ASSERT_NULL(validate(q));
  q = p; memset(q.name, 'a', 47); q.name[47] = '\0'; TEST_ASSERT_NULL(validate(q));
}

void test_profile_parse_slot() {
  uint8_t out;
  TEST_ASSERT_TRUE(parseSlot("0", out));  TEST_ASSERT_EQUAL_UINT8(0, out);
  TEST_ASSERT_TRUE(parseSlot("7", out));  TEST_ASSERT_EQUAL_UINT8(7, out);
  TEST_ASSERT_TRUE(parseSlot("19", out)); TEST_ASSERT_EQUAL_UINT8(19, out);
  TEST_ASSERT_FALSE(parseSlot("20", out));
  TEST_ASSERT_FALSE(parseSlot("", out));
  TEST_ASSERT_FALSE(parseSlot("07", out));
  TEST_ASSERT_FALSE(parseSlot("1a", out));
  TEST_ASSERT_FALSE(parseSlot("-1", out));
  TEST_ASSERT_FALSE(parseSlot("100", out));
}

void test_profile_constants() {
  TEST_ASSERT_EQUAL_UINT8(20, MAX_PROFILES);
  TEST_ASSERT_EQUAL_UINT8(3, PROFILE_VERSION);
  TEST_ASSERT_EQUAL_UINT32(220, sizeof(Profile));
  TEST_ASSERT_EQUAL_UINT32(212, PROFILE_V2_BLOB_SIZE);
}

void test_profile_belt_length_sums_only_used_marks() {
  Profile p = makeDefaultProfile("X", 3.0f, 2);
  p.mark_distance_m[2] = 50.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 3.0f, beltLength(p));
}

void test_profile_make_engine_config() {
  Profile p = makeDefaultProfile("X", 2.82f, 3);
  p.mark_distance_m[0] = 0.90f; p.mark_distance_m[1] = 1.00f; p.mark_distance_m[2] = 0.92f;
  EngineConfig c = makeEngineConfig(p, 50000u, 3000000u, 8.0f);
  TEST_ASSERT_EQUAL_UINT8(3, c.marks_count);
  TEST_ASSERT_EQUAL_FLOAT(0.90f, c.mark_distance_m[0]);
  TEST_ASSERT_EQUAL_FLOAT(1.00f, c.mark_distance_m[1]);
  TEST_ASSERT_EQUAL_FLOAT(0.92f, c.mark_distance_m[2]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, c.mark_distance_m[3]);
  TEST_ASSERT_EQUAL_UINT32(50000u, c.debounce_us);
  TEST_ASSERT_EQUAL_UINT32(3000000u, c.stop_timeout_us);
  TEST_ASSERT_EQUAL_FLOAT(8.0f, c.max_speed_mps);
}

void test_profile_type_names() {
  TEST_ASSERT_EQUAL_STRING("belt", typeName(TreadmillType::Belt));
  TEST_ASSERT_EQUAL_STRING("slats", typeName(TreadmillType::Slats));
  TreadmillType t;
  TEST_ASSERT_TRUE(parseTypeName("slats", t));
  TEST_ASSERT_TRUE(t == TreadmillType::Slats);
  TEST_ASSERT_FALSE(parseTypeName("rope", t));
  TEST_ASSERT_TRUE(validName("Gym A", 24));
  TEST_ASSERT_TRUE(validName("m\xc3\xb8lle", 24));   // UTF-8 allowed
  TEST_ASSERT_FALSE(validName("", 24));
  TEST_ASSERT_FALSE(validName("a\"b", 24));
  TEST_ASSERT_FALSE(validName("tab\there", 24));
}

// --- Max-speed plausibility filter and geometry-derived stop timeout ---

static EngineConfig oneMarkConfig(float max_speed_mps) {
  return makeEqualConfig(3.603f, 1, 50000u, 3000000u, max_speed_mps);
}

void test_engine_rejects_implausible_speed() {
  SpeedEngine e(oneMarkConfig(8.0f));
  e.onPulse(1000000);
  e.onPulse(1066400);  // 66 ms later: 54 m/s on a 3.603 m belt (first-field-run artifact)
  TEST_ASSERT_EQUAL_UINT32(0, e.log().size());  // first pulse still held back
  TEST_ASSERT_EQUAL_UINT32(1, e.status(1066401).rejected_implausible);
  TEST_ASSERT_EQUAL_UINT32(0, e.status(1066401).rejected_debounce);
  e.onPulse(2261000);  // 1.261 s after the accepted pulse: 2.86 m/s
  TEST_ASSERT_EQUAL_UINT32(1, e.log().size());
  TEST_ASSERT_EQUAL_UINT32(1000000, e.log().at(0).t_us);  // no speed yet: the first edge wins
  e.tick(2261000 + 450400);
  TEST_ASSERT_EQUAL_UINT32(2, e.log().size());
  TEST_ASSERT_EQUAL_UINT32(1261000, e.log().at(1).interval_us);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.857f, e.log().at(1).rev_speed_mps);
}

void test_engine_max_speed_zero_disables_filter() {
  SpeedEngine e(oneMarkConfig(0.0f));
  e.onPulse(1000000);
  e.onPulse(1066400);
  TEST_ASSERT_EQUAL_UINT32(2, e.log().size());
  TEST_ASSERT_EQUAL_UINT32(0, e.status(1066401).rejected_implausible);
}

void test_engine_plausible_fast_pulse_passes() {
  SpeedEngine e(oneMarkConfig(8.0f));
  e.onPulse(1000000);
  e.onPulse(1456000);  // 7.9 m/s: below the 8 m/s ceiling
  e.tick(1456000 + 450400);
  TEST_ASSERT_EQUAL_UINT32(2, e.log().size());
  TEST_ASSERT_EQUAL_UINT32(0, e.status(1456001).rejected_implausible);
}

void test_engine_implausible_uses_shortest_mark_distance() {
  EngineConfig c = makeEqualConfig(2.82f, 3, 50000u, 3000000u, 8.0f);
  c.mark_distance_m[0] = 0.5f; c.mark_distance_m[1] = 1.0f; c.mark_distance_m[2] = 1.32f;
  SpeedEngine e(c);
  e.onPulse(1000000);
  e.onPulse(1070000);  // 70 ms: 7.1 m/s over the shortest mark, plausible
  TEST_ASSERT_EQUAL_UINT32(1, e.log().size());
  e.onPulse(1130000);  // 60 ms: 8.3 m/s over the shortest mark, rejected
  e.tick(1200000);
  TEST_ASSERT_EQUAL_UINT32(2, e.log().size());
  TEST_ASSERT_EQUAL_UINT32(1, e.status(1130001).rejected_implausible);
}

void test_engine_reset_clears_implausible() {
  SpeedEngine e(oneMarkConfig(8.0f));
  e.onPulse(1000000);
  e.onPulse(1066400);
  e.reset();
  TEST_ASSERT_EQUAL_UINT32(0, e.status(1066401).rejected_implausible);
}

// --- Held-back pulses: of two edges closer than the min gap, keep the one that fits ---

// One-mark belt at 3.6 m and 14.2 km/h, as in the 2026-10-13 field run.
static std::vector<uint32_t> fieldRunPulses(double early_s, int glitch_at) {
  SynthOptions o;
  o.belt_length_m = 3.6;
  o.marks = 1;
  o.start_us = 1000000;
  std::vector<uint32_t> p = synthConstant(14.2 / 3.6, 20.0, o);
  const uint32_t spurious = p[glitch_at] - static_cast<uint32_t>(early_s * 1e6);
  p.insert(p.begin() + glitch_at, spurious);
  return p;
}

static void feedAndSettle(SpeedEngine& e, const std::vector<uint32_t>& p) {
  for (uint32_t t : p) {
    e.tick(t);  // as loop() does between pulses
    e.onPulse(t);
  }
  e.tick(p.back() + 1000000);
}

void test_engine_early_spurious_edge_replaced_by_real_mark() {
  // A spurious edge 0.27 s before the tape (session 38): before the fix one
  // revolution read 20.8 km/h and the next 10.8 km/h.
  std::vector<uint32_t> p = fieldRunPulses(0.27, 10);
  SpeedEngine e(oneMarkConfig(8.0f));
  feedAndSettle(e, p);
  TEST_ASSERT_EQUAL_UINT32(p.size() - 1, e.log().size());
  TEST_ASSERT_EQUAL_UINT32(1, e.status(p.back()).rejected_implausible);
  for (size_t i = 1; i < e.log().size(); ++i) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 14.2f / 3.6f, e.log().at(i).rev_speed_mps);
  }
  TEST_ASSERT_EQUAL_UINT32(p[11], e.log().at(10).t_us);  // the real mark, not the spurious edge
}

void test_engine_small_early_offset_also_replaced() {
  std::vector<uint32_t> p = fieldRunPulses(0.09, 12);
  SpeedEngine e(oneMarkConfig(8.0f));
  feedAndSettle(e, p);
  for (size_t i = 1; i < e.log().size(); ++i) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 14.2f / 3.6f, e.log().at(i).rev_speed_mps);
  }
}

void test_engine_late_spurious_edge_rejected() {
  SynthOptions o;
  o.belt_length_m = 3.6;
  o.marks = 1;
  o.start_us = 1000000;
  std::vector<uint32_t> p = synthConstant(14.2 / 3.6, 20.0, o);
  p.insert(p.begin() + 11, p[10] + 200000);  // 0.2 s after the real mark
  SpeedEngine e(oneMarkConfig(8.0f));
  feedAndSettle(e, p);
  TEST_ASSERT_EQUAL_UINT32(p.size() - 1, e.log().size());
  TEST_ASSERT_EQUAL_UINT32(p[10], e.log().at(10).t_us);
  for (size_t i = 1; i < e.log().size(); ++i) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 14.2f / 3.6f, e.log().at(i).rev_speed_mps);
  }
}

void test_engine_held_pulse_commits_on_tick_after_min_gap() {
  SpeedEngine e(oneMarkConfig(8.0f));  // min gap 3.603 / 8 = 450.4 ms
  e.onPulse(1000000);
  e.tick(1450000);
  TEST_ASSERT_EQUAL_UINT32(0, e.log().size());
  e.tick(1450400);
  TEST_ASSERT_EQUAL_UINT32(1, e.log().size());
  e.tick(1450400);
  TEST_ASSERT_EQUAL_UINT32(1, e.log().size());  // committed once
}

void test_engine_reset_drops_held_pulse() {
  SpeedEngine e(oneMarkConfig(8.0f));
  e.onPulse(1000000);
  e.reset();
  e.tick(2000000);
  TEST_ASSERT_EQUAL_UINT32(0, e.log().size());
}

void test_stop_timeout_from_geometry() {
  // longest mark distance / min speed, clamped to [min_us, max_us]
  TEST_ASSERT_UINT32_WITHIN(1, 7206000u, stopTimeoutUs(3.603f, 0.5f, 3000000u, 15000000u));
  TEST_ASSERT_EQUAL_UINT32(3000000u, stopTimeoutUs(0.94f, 0.5f, 3000000u, 15000000u));
  TEST_ASSERT_EQUAL_UINT32(15000000u, stopTimeoutUs(10.0f, 0.5f, 3000000u, 15000000u));
  TEST_ASSERT_EQUAL_UINT32(3000000u, stopTimeoutUs(3.603f, 0.0f, 3000000u, 15000000u));  // guard
}

void test_stop_timeout_for_profile_uses_longest_mark() {
  Profile p = makeDefaultProfile("X", 3.603f, 1);
  TEST_ASSERT_UINT32_WITHIN(1, 7206000u, stopTimeoutFor(p, 0.5f, 3000000u, 15000000u));
  Profile u = makeDefaultProfile("U", 4.5f, 3);
  u.mark_distance_m[0] = 1.0f; u.mark_distance_m[1] = 2.5f; u.mark_distance_m[2] = 1.0f;
  TEST_ASSERT_UINT32_WITHIN(1, 5000000u, stopTimeoutFor(u, 0.5f, 3000000u, 15000000u));
}

// --- Slat treadmills: belt length = slat count x pitch ---

static Profile slatProfile(const char* name, uint16_t count, float pitch, uint8_t marks) {
  Profile p = makeDefaultProfile(name, 1.0f, marks);
  p.type = TreadmillType::Slats;
  p.slat_count = count;
  p.slat_pitch_m = pitch;
  seedEqualDistances(p, slatBeltLength(p));
  return p;
}

void test_profile_slats_valid_and_belt_length() {
  Profile p = slatProfile("Slat mill", 47, 0.0746f, 1);
  TEST_ASSERT_NULL(validate(p));
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 3.5062f, slatBeltLength(p));
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 3.5062f, beltLength(p));
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 3.5062f, p.mark_distance_m[0]);
  Profile two = slatProfile("Two", 47, 0.0746f, 2);
  TEST_ASSERT_NULL(validate(two));
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 1.7531f, two.mark_distance_m[1]);
}

void test_profile_slats_validation() {
  Profile p = slatProfile("S", 47, 0.0746f, 1);
  Profile q = p; q.slat_count = 1;             TEST_ASSERT_EQUAL_STRING("slat count must be 2..1000", validate(q));
  q = p; q.slat_count = 1001;                  TEST_ASSERT_EQUAL_STRING("slat count must be 2..1000", validate(q));
  q = p; q.slat_pitch_m = 0.001f;              TEST_ASSERT_EQUAL_STRING("slat pitch out of range", validate(q));
  q = p; q.slat_pitch_m = 0.6f;                TEST_ASSERT_EQUAL_STRING("slat pitch out of range", validate(q));
  q = p; q.mark_distance_m[0] = 3.4f;          TEST_ASSERT_EQUAL_STRING("distances do not match slats", validate(q));
  q = p; q.mark_distance_m[0] = 3.52f;         TEST_ASSERT_NULL(validate(q));  // within 0.5 %
  // Belt profiles ignore the slat fields entirely.
  Profile b = makeDefaultProfile("B", 2.82f, 3);
  b.slat_count = 1; b.slat_pitch_m = 9.0f;
  TEST_ASSERT_NULL(validate(b));
  // Distance range is still checked first for slats.
  q = p; q.mark_distance_m[0] = 0.01f; q.slat_count = 2; q.slat_pitch_m = 0.005f;
  TEST_ASSERT_EQUAL_STRING("distance out of range", validate(q));
}

void test_profile_slats_calibrated_distances_pass() {
  Profile p = slatProfile("S", 47, 0.0746f, 2);
  p.mark_distance_m[0] = 1.70f;
  p.mark_distance_m[1] = 1.8062f;  // sum 3.5062: calibration moved the split but kept the sum
  TEST_ASSERT_NULL(validate(p));
}

void test_profile_v2_blob_upgrades_to_v3() {
  Profile v3 = makeDefaultProfile("Old", 3.6f, 1);
  std::strcpy(v3.notes, "kept");
  unsigned char blob[PROFILE_V2_BLOB_SIZE];
  std::memcpy(blob, &v3, PROFILE_V2_BLOB_SIZE);
  blob[0] = 2;  // version byte of the stored v2 layout
  Profile out{};
  out.slat_count = 99; out.slat_pitch_m = 1.0f;  // must be cleared by the upgrade
  TEST_ASSERT_TRUE(profileFromBlob(blob, sizeof blob, out));
  TEST_ASSERT_EQUAL_UINT8(PROFILE_VERSION, out.version);
  TEST_ASSERT_EQUAL_STRING("Old", out.name);
  TEST_ASSERT_EQUAL_STRING("kept", out.notes);
  TEST_ASSERT_EQUAL_UINT8(1, out.marks);
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 3.6f, out.mark_distance_m[0]);
  TEST_ASSERT_EQUAL_UINT16(0, out.slat_count);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, out.slat_pitch_m);
  TEST_ASSERT_NULL(validate(out));
  // Current-size blobs pass through unchanged; other sizes are rejected.
  TEST_ASSERT_TRUE(profileFromBlob(&v3, sizeof v3, out));
  TEST_ASSERT_EQUAL_STRING("Old", out.name);
  TEST_ASSERT_FALSE(profileFromBlob(blob, 60, out));
  blob[0] = 1;
  TEST_ASSERT_FALSE(profileFromBlob(blob, sizeof blob, out));  // wrong version for that size
}

void test_json_profile_slats() {
  Profile p = slatProfile("Slat mill", 47, 0.0746f, 1);
  char buf[256];
  int n = formatProfileJson(p, 4, buf, sizeof buf);
  TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof buf);
  TEST_ASSERT_EQUAL_STRING(
      "{\"id\":4,\"name\":\"Slat mill\",\"notes\":\"\",\"type\":\"slats\",\"marks\":1,"
      "\"mark_distance_m\":[3.506],\"slat_count\":47,\"slat_pitch_m\":0.07460,\"belt_length_m\":3.506}",
      buf);
}

// --- Flash session log: batcher, header, session metadata ---

void test_log_newest() {
  PulseLog log;
  log.push(rec(1));
  log.push(rec(2));
  TEST_ASSERT_EQUAL_UINT32(2, log.newest().seq);
  for (uint32_t i = 3; i <= LOG_CAPACITY + 5; ++i) log.push(rec(i));
  TEST_ASSERT_EQUAL_UINT32(LOG_CAPACITY + 5, log.newest().seq);
}

static PulseRecord csvRec(uint32_t seq, uint32_t t_us) {
  PulseRecord r{};
  r.seq = seq; r.t_us = t_us; r.interval_us = 338400u; r.mark_index = 2;
  r.mark_speed_mps = 2.77778f; r.rev_speed_mps = 2.7f;
  return r;
}

void test_batcher_formats_lines_and_counts() {
  CsvBatcher b;
  TEST_ASSERT_EQUAL_UINT32(0, b.size());
  TEST_ASSERT_EQUAL_UINT32(0, b.pending());
  TEST_ASSERT_TRUE(b.addRecord(csvRec(1, 1000), 1000));
  TEST_ASSERT_TRUE(b.addRecord(csvRec(2, 2000), 2000));
  char line1[64], line2[64];
  formatRecordCsv(csvRec(1, 1000), line1, sizeof line1);
  formatRecordCsv(csvRec(2, 2000), line2, sizeof line2);
  std::string expect = std::string(line1) + line2;
  TEST_ASSERT_EQUAL_UINT32(expect.size(), b.size());
  TEST_ASSERT_EQUAL_MEMORY(expect.data(), b.data(), expect.size());
  TEST_ASSERT_EQUAL_UINT32(2, b.pending());
}

void test_batcher_flush_by_count() {
  CsvBatcher b;
  for (uint32_t i = 1; i < CsvBatcher::kFlushRecords; ++i) {
    b.addRecord(csvRec(i, i * 1000), i * 1000);
    TEST_ASSERT_FALSE(b.shouldFlush(i * 1000));
  }
  b.addRecord(csvRec(CsvBatcher::kFlushRecords, 99000), 99000);
  TEST_ASSERT_TRUE(b.shouldFlush(99000));
}

void test_batcher_flush_by_age() {
  CsvBatcher b;
  TEST_ASSERT_FALSE(b.shouldFlush(10000000));  // empty never flushes
  b.addRecord(csvRec(1, 1000000), 1000000);
  TEST_ASSERT_FALSE(b.shouldFlush(1000000 + CsvBatcher::kFlushAgeUs - 1));
  TEST_ASSERT_TRUE(b.shouldFlush(1000000 + CsvBatcher::kFlushAgeUs));
  // Age is measured from the oldest pending record and survives a micros() wrap.
  CsvBatcher w;
  w.addRecord(csvRec(1, 0xFFFFFF00u), 0xFFFFFF00u);
  TEST_ASSERT_TRUE(w.shouldFlush(CsvBatcher::kFlushAgeUs));
}

void test_batcher_never_overflows() {
  CsvBatcher b;
  uint32_t n = 0;
  while (b.addRecord(csvRec(1000000 + n, 4000000000u), 0)) ++n;
  TEST_ASSERT_TRUE(n > 50);
  TEST_ASSERT_TRUE(b.size() <= CsvBatcher::kCapacity);
  TEST_ASSERT_TRUE(b.shouldFlush(0));  // nearly full counts as a flush reason
  TEST_ASSERT_EQUAL_UINT32(n, b.pending());
  b.clear();
  TEST_ASSERT_EQUAL_UINT32(0, b.size());
  TEST_ASSERT_EQUAL_UINT32(0, b.pending());
  TEST_ASSERT_FALSE(b.shouldFlush(0));
}

void test_batcher_add_line_comment() {
  CsvBatcher b;
  TEST_ASSERT_TRUE(b.addLine("# hello\n"));
  TEST_ASSERT_EQUAL_UINT32(8, b.size());
  TEST_ASSERT_EQUAL_UINT32(0, b.pending());     // comment lines are not records
  TEST_ASSERT_FALSE(b.shouldFlush(100000000));  // and do not age
  b.addRecord(csvRec(1, 5), 5);
  TEST_ASSERT_EQUAL_UINT32(1, b.pending());
  TEST_ASSERT_EQUAL_MEMORY("# hello\n1,5,", b.data(), 12);
}

void test_session_header_exact() {
  Profile p = makeDefaultProfile("Home treadmill", 2.82f, 3);
  SessionHeaderInfo h{123, "a1b2c3d", 12, &p, 0};
  char buf[512];
  int n = formatSessionHeader(h, buf, sizeof buf);
  TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof buf);
  TEST_ASSERT_EQUAL_STRING(
      "# millarr session 123\n"
      "# firmware a1b2c3d boot_uptime_s 12\n"
      "# profile 0 \"Home treadmill\" type=belt marks=3 belt_length_m=2.820 mark_distance_m=[0.940,0.940,0.940]\n"
      "# t_us wraps every 4294967296 us: add 2^32 whenever it decreases. interval_us=0 marks a run restart.\n",
      buf);
  Profile s = slatProfile("Slat mill", 47, 0.0746f, 1);
  SessionHeaderInfo hs{7, "unknown", 0, &s, 4};
  n = formatSessionHeader(hs, buf, sizeof buf);
  TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof buf);
  TEST_ASSERT_EQUAL_STRING(
      "# millarr session 7\n"
      "# firmware unknown boot_uptime_s 0\n"
      "# profile 4 \"Slat mill\" type=slats marks=1 belt_length_m=3.506 mark_distance_m=[3.506] slat_count=47 slat_pitch_m=0.07460\n"
      "# t_us wraps every 4294967296 us: add 2^32 whenever it decreases. interval_us=0 marks a run restart.\n",
      buf);
}

void test_profile_comment_line() {
  Profile p = makeDefaultProfile("Home treadmill", 2.82f, 3);
  char buf[256];
  int n = formatProfileComment(p, 0, 77, buf, sizeof buf);
  TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof buf);
  TEST_ASSERT_EQUAL_STRING(
      "# profile 0 \"Home treadmill\" type=belt marks=3 belt_length_m=2.820 mark_distance_m=[0.940,0.940,0.940] at_uptime_s=77\n",
      buf);
}

void test_session_json_exact() {
  char buf[128];
  SessionInfo a{12, 69040, 1726, true, true};
  int n = formatSessionJson(a, buf, sizeof buf);
  TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof buf);
  TEST_ASSERT_EQUAL_STRING("{\"id\":12,\"bytes\":69040,\"active\":true,\"records\":1726}", buf);
  SessionInfo c{11, 230400, 0, false, false};
  n = formatSessionJson(c, buf, sizeof buf);
  TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof buf);
  TEST_ASSERT_EQUAL_STRING("{\"id\":11,\"bytes\":230400,\"active\":false}", buf);
}

void test_parse_session_id() {
  uint32_t id = 0;
  TEST_ASSERT_TRUE(parseSessionId("1", id));       TEST_ASSERT_EQUAL_UINT32(1, id);
  TEST_ASSERT_TRUE(parseSessionId("123456", id));  TEST_ASSERT_EQUAL_UINT32(123456, id);
  TEST_ASSERT_TRUE(parseSessionId("999999999", id));
  TEST_ASSERT_FALSE(parseSessionId("0", id));
  TEST_ASSERT_FALSE(parseSessionId("", id));
  TEST_ASSERT_FALSE(parseSessionId("012", id));
  TEST_ASSERT_FALSE(parseSessionId("1a", id));
  TEST_ASSERT_FALSE(parseSessionId("1234567890", id));
  TEST_ASSERT_FALSE(parseSessionId("-1", id));
}

// --- RSC measurement encoder ---

static void encodeRsc(float speed_mps, uint8_t cadence, uint8_t out[RSC_MEASUREMENT_LEN], size_t& n) {
  n = encodeRscMeasurement(makeRscMeasurement(speed_mps, cadence, 1.667f), out);
}

void test_rsc_encode_zero_speed() {
  uint8_t out[RSC_MEASUREMENT_LEN];
  size_t n;
  encodeRsc(0.0f, 0, out, n);
  TEST_ASSERT_EQUAL_size_t(4, n);
  const uint8_t expect[4] = {0x00, 0x00, 0x00, 0x00};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expect, out, 4);
}

void test_rsc_encode_running_speed_exact() {
  uint8_t out[RSC_MEASUREMENT_LEN];
  size_t n;
  encodeRsc(2.778f, 0, out, n);   // 711.168 -> 711 = 0x02C7, running flag set
  const uint8_t expect[4] = {0x04, 0xC7, 0x02, 0x00};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expect, out, 4);
}

void test_rsc_encode_walking_below_threshold() {
  uint8_t out[RSC_MEASUREMENT_LEN];
  size_t n;
  encodeRsc(1.0f, 0, out, n);
  const uint8_t expect[4] = {0x00, 0x00, 0x01, 0x00};   // 256 = 0x0100
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expect, out, 4);
  encodeRsc(1.666f, 0, out, n);
  TEST_ASSERT_EQUAL_UINT8(0x00, out[0]);
  encodeRsc(1.667f, 0, out, n);
  TEST_ASSERT_EQUAL_UINT8(RSC_FLAG_RUNNING, out[0]);
}

void test_rsc_encode_rounds_to_nearest() {
  RscMeasurement m = makeRscMeasurement(0.01f, 0, 1.667f);   // 2.56 -> 3
  TEST_ASSERT_EQUAL_UINT16(3, m.speed_q8);
  m = makeRscMeasurement(0.001f, 0, 1.667f);                 // 0.256 -> 0
  TEST_ASSERT_EQUAL_UINT16(0, m.speed_q8);
}

void test_rsc_encode_saturates_and_clamps() {
  RscMeasurement m = makeRscMeasurement(300.0f, 0, 1.667f);  // 76800 > 65535
  TEST_ASSERT_EQUAL_UINT16(0xFFFF, m.speed_q8);
  TEST_ASSERT_EQUAL_UINT8(RSC_FLAG_RUNNING, m.flags);
  m = makeRscMeasurement(-1.0f, 0, 1.667f);
  TEST_ASSERT_EQUAL_UINT16(0, m.speed_q8);
  TEST_ASSERT_EQUAL_UINT8(0, m.flags);
  m = makeRscMeasurement(std::nanf(""), 0, 1.667f);
  TEST_ASSERT_EQUAL_UINT16(0, m.speed_q8);
  TEST_ASSERT_EQUAL_UINT8(0, m.flags);
}

void test_rsc_encode_cadence_passthrough() {
  uint8_t out[RSC_MEASUREMENT_LEN];
  size_t n;
  encodeRsc(2.0f, 170, out, n);
  TEST_ASSERT_EQUAL_UINT8(0xAA, out[3]);
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
  RUN_TEST(test_engine_stop_latched_across_wrap);
  RUN_TEST(test_stats_uneven_marks_rev_speed_unaffected);
  RUN_TEST(test_stats_even_marks_share_is_one_third);
  RUN_TEST(test_stats_jitter_reflected_in_stddev);
  RUN_TEST(test_stats_window_survives_stop);
  RUN_TEST(test_config_belt_length_and_equal_config);
  RUN_TEST(test_engine_phase_alignment_uneven_marks);
  RUN_TEST(test_engine_first_revolution_distance_is_belt_length);
  RUN_TEST(test_engine_phase_not_locked_before_first_revolution);
  RUN_TEST(test_engine_one_and_two_marks);
  RUN_TEST(test_engine_reconfigure_keeps_log_and_distance);
  RUN_TEST(test_stats_window_cleared_when_phase_changes_after_restart);
  RUN_TEST(test_profile_default_is_valid_and_equal);
  RUN_TEST(test_profile_validate_rejects_bad_values);
  RUN_TEST(test_profile_parse_slot);
  RUN_TEST(test_profile_constants);
  RUN_TEST(test_profile_belt_length_sums_only_used_marks);
  RUN_TEST(test_profile_make_engine_config);
  RUN_TEST(test_profile_type_names);
  RUN_TEST(test_csv_header);
  RUN_TEST(test_csv_record_line);
  RUN_TEST(test_json_record);
  RUN_TEST(test_json_status_contains_all_fields);
  RUN_TEST(test_json_profile);
  RUN_TEST(test_json_profile_notes_newline_escaped);
  RUN_TEST(test_json_status_truncation_is_reported);
  RUN_TEST(test_engine_rejects_implausible_speed);
  RUN_TEST(test_engine_max_speed_zero_disables_filter);
  RUN_TEST(test_engine_plausible_fast_pulse_passes);
  RUN_TEST(test_engine_implausible_uses_shortest_mark_distance);
  RUN_TEST(test_engine_reset_clears_implausible);
  RUN_TEST(test_engine_early_spurious_edge_replaced_by_real_mark);
  RUN_TEST(test_engine_small_early_offset_also_replaced);
  RUN_TEST(test_engine_late_spurious_edge_rejected);
  RUN_TEST(test_engine_held_pulse_commits_on_tick_after_min_gap);
  RUN_TEST(test_engine_reset_drops_held_pulse);
  RUN_TEST(test_stop_timeout_from_geometry);
  RUN_TEST(test_stop_timeout_for_profile_uses_longest_mark);
  RUN_TEST(test_profile_slats_valid_and_belt_length);
  RUN_TEST(test_profile_slats_validation);
  RUN_TEST(test_profile_slats_calibrated_distances_pass);
  RUN_TEST(test_profile_v2_blob_upgrades_to_v3);
  RUN_TEST(test_json_profile_slats);
  RUN_TEST(test_log_newest);
  RUN_TEST(test_batcher_formats_lines_and_counts);
  RUN_TEST(test_batcher_flush_by_count);
  RUN_TEST(test_batcher_flush_by_age);
  RUN_TEST(test_batcher_never_overflows);
  RUN_TEST(test_batcher_add_line_comment);
  RUN_TEST(test_session_header_exact);
  RUN_TEST(test_profile_comment_line);
  RUN_TEST(test_session_json_exact);
  RUN_TEST(test_parse_session_id);
  RUN_TEST(test_rsc_encode_zero_speed);
  RUN_TEST(test_rsc_encode_running_speed_exact);
  RUN_TEST(test_rsc_encode_walking_below_threshold);
  RUN_TEST(test_rsc_encode_rounds_to_nearest);
  RUN_TEST(test_rsc_encode_saturates_and_clamps);
  RUN_TEST(test_rsc_encode_cadence_passthrough);
  return UNITY_END();
}
