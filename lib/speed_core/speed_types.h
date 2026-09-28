// Plain data types shared by the speed engine, log and formatters.
// No Arduino dependencies: this header compiles on the host.
#pragma once
#include <cstddef>
#include <cstdint>

namespace speed_core {

constexpr uint8_t MAX_MARKS = 8;           // upper bound for marks_count
constexpr uint8_t STATS_REVOLUTIONS = 10;  // window for mark_share and jitter

#ifndef SPEED_CORE_LOG_CAPACITY
#define SPEED_CORE_LOG_CAPACITY 2048
#endif
constexpr size_t LOG_CAPACITY = SPEED_CORE_LOG_CAPACITY;

struct EngineConfig {
  uint8_t marks_count;                // 1..MAX_MARKS
  float mark_distance_m[MAX_MARKS];   // [k] = distance from mark k to mark k+1 (mod marks_count)
  uint32_t debounce_us;               // reject pulses closer than this to the previous one
  uint32_t stop_timeout_us;           // no pulse for this long => stopped
  float max_speed_mps;                // reject pulses implying a higher speed; 0 = disabled
};

// Sum of the first marks_count distances.
inline float beltLength(const EngineConfig& c) {
  const uint8_t n = c.marks_count > MAX_MARKS ? MAX_MARKS : c.marks_count;
  float sum = 0.0f;
  for (uint8_t k = 0; k < n; ++k) sum += c.mark_distance_m[k];
  return sum;
}

// Equal spacing: belt_length_m / marks_count for every mark.
inline EngineConfig makeEqualConfig(float belt_length_m, uint8_t marks_count, uint32_t debounce_us,
                                    uint32_t stop_timeout_us, float max_speed_mps = 0.0f) {
  EngineConfig c{};
  c.marks_count = marks_count < 1 ? 1 : (marks_count > MAX_MARKS ? MAX_MARKS : marks_count);
  for (uint8_t k = 0; k < c.marks_count; ++k) c.mark_distance_m[k] = belt_length_m / c.marks_count;
  c.debounce_us = debounce_us;
  c.stop_timeout_us = stop_timeout_us;
  c.max_speed_mps = max_speed_mps;
  return c;
}

// Stop timeout that still sees one pulse per mark at min_speed_mps: the longest
// mark distance divided by the minimum speed, clamped to [min_us, max_us].
// The minimum measurable speed therefore equals min_speed_mps unless the clamp
// applies. A non-positive speed yields min_us.
inline uint32_t stopTimeoutUs(float longest_mark_distance_m, float min_speed_mps, uint32_t min_us,
                              uint32_t max_us) {
  if (!(min_speed_mps > 0.0f) || !(longest_mark_distance_m > 0.0f)) return min_us;
  const double us = static_cast<double>(longest_mark_distance_m) / min_speed_mps * 1e6;
  if (us <= min_us) return min_us;
  if (us >= max_us) return max_us;
  return static_cast<uint32_t>(us + 0.5);
}

struct PulseRecord {
  uint32_t seq;          // 1-based, monotonically increasing, survives log wrap
  uint32_t t_us;         // timestamp (micros), wraps every ~71.6 min
  uint32_t interval_us;  // since previous accepted pulse; 0 for the first pulse of a run
  uint8_t mark_index;    // physical mark once the phase is locked; pulse index since (re)start mod marks before
  float mark_speed_mps;  // distance of the interval ending at this mark / interval; 0 when interval is 0
  float rev_speed_mps;   // belt_length / time over the last marks_count intervals; 0 until available
};

struct EngineStatus {
  float speed_mps;             // rev speed of the newest pulse, or 0 when stopped
  bool running;
  bool phase_locked;           // true once the current run has completed a revolution and chosen its mark phase
  float distance_m;            // accumulated distance: per-mark distance once the phase is locked, mean before
  uint32_t pulse_count;        // accepted pulses since reset
  uint32_t revolution_count;   // completed revolutions since reset
  uint32_t rejected_debounce;  // pulses rejected by the engine's debounce rule
  uint32_t rejected_implausible;  // pulses rejected because they implied more than max_speed_mps
  float mark_share[MAX_MARKS]; // mean share of revolution time per mark over the stats window
  float rev_speed_stddev;      // std dev of rev speed over the stats window (m/s)
  uint8_t stats_revolutions;   // revolutions currently in the stats window (0..STATS_REVOLUTIONS)
  uint32_t log_size;
  uint32_t log_capacity;
  uint32_t log_overwritten;
  uint32_t last_seq;           // 0 when the log is empty
};

// State of the flash session log, reported in /api/status.
struct FlashStatus {
  bool ok;                   // filesystem mounted; false => nothing is written
  bool full;                 // no room even after evicting closed sessions
  uint32_t total_bytes;
  uint32_t used_bytes;
  uint32_t session_id;       // current session (file created lazily on the first flush)
  uint32_t session_records;  // records written or pending for the current session
  uint32_t session_bytes;    // bytes on flash for the current session
  uint32_t write_errors;
  uint8_t sessions;          // files on flash, including the current one if created
};

// State of the BLE RSC foot pod, reported in /api/status. Counters are since boot.
struct BleStatus {
  bool advertising;
  bool connected;
  uint32_t connects;        // connections accepted since boot
  uint32_t notifications;   // RSC measurements sent since boot
  float last_speed_mps;     // speed in the last notification
};

}  // namespace speed_core
