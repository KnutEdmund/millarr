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
  float belt_length_m;      // e.g. 2.82
  uint8_t marks_count;      // 1..MAX_MARKS, e.g. 3
  uint32_t debounce_us;     // reject pulses closer than this to the previous one
  uint32_t stop_timeout_us; // no pulse for this long => stopped
};

struct PulseRecord {
  uint32_t seq;          // 1-based, monotonically increasing, survives log wrap
  uint32_t t_us;         // timestamp (micros), wraps every ~71.6 min
  uint32_t interval_us;  // since previous accepted pulse; 0 for the first pulse of a run
  uint8_t mark_index;    // pulse index since (re)start modulo marks_count
  float mark_speed_mps;  // (belt_length / marks) / interval; 0 when interval is 0
  float rev_speed_mps;   // belt_length / time over the last marks_count intervals; 0 until available
};

struct EngineStatus {
  float speed_mps;             // rev speed of the newest pulse, or 0 when stopped
  bool running;
  float distance_m;            // pulses with an interval * belt_length / marks
  uint32_t pulse_count;        // accepted pulses since reset
  uint32_t revolution_count;   // completed revolutions since reset
  uint32_t rejected_debounce;  // pulses rejected by the engine's debounce rule
  float mark_share[MAX_MARKS]; // mean share of revolution time per mark over the stats window
  float rev_speed_stddev;      // std dev of rev speed over the stats window (m/s)
  uint8_t stats_revolutions;   // revolutions currently in the stats window (0..STATS_REVOLUTIONS)
  uint32_t log_size;
  uint32_t log_overwritten;
  uint32_t last_seq;           // 0 when the log is empty
};

}  // namespace speed_core
