// Turns a stream of pulse timestamps into speed, distance and diagnostics.
// Pure C++: no hardware, no clock. The caller supplies every timestamp.
#pragma once
#include <cstdint>

#include "pulse_log.h"
#include "speed_types.h"

namespace speed_core {

class SpeedEngine {
 public:
  explicit SpeedEngine(const EngineConfig& cfg);

  // Feed one falling-edge timestamp (micros). Applies debounce and stop detection.
  void onPulse(uint32_t t_us);

  // Called periodically (e.g. from loop()) so a long idle period is latched as
  // stopped before a later micros() wrap could make running() resurrect.
  void tick(uint32_t now_us);

  bool running(uint32_t now_us) const;
  // Revolution speed of the newest pulse, or 0 when stopped / not yet available.
  float speedAt(uint32_t now_us) const;
  EngineStatus status(uint32_t now_us) const;
  void reset();

  const PulseLog& log() const { return log_; }
  const EngineConfig& config() const { return cfg_; }
  float markDistance() const { return cfg_.belt_length_m / cfg_.marks_count; }

 private:
  void startRun();
  void pushRevolutionStats(float rev_speed);

  EngineConfig cfg_;
  PulseLog log_;

  bool has_last_ = false;
  uint32_t last_t_ = 0;
  uint32_t next_seq_ = 1;
  uint32_t run_count_ = 0;                 // accepted pulses since (re)start
  uint32_t recent_ts_[MAX_MARKS] = {};     // timestamp per mark slot, for rev speed
  float last_rev_speed_ = 0.0f;
  uint32_t cur_rev_intervals_[MAX_MARKS] = {};

  // Stats window over the last STATS_REVOLUTIONS completed revolutions.
  uint32_t rev_intervals_[STATS_REVOLUTIONS][MAX_MARKS] = {};
  float rev_speeds_[STATS_REVOLUTIONS] = {};
  uint8_t stats_head_ = 0;
  uint8_t stats_count_ = 0;

  uint32_t pulse_count_ = 0;
  uint32_t interval_count_ = 0;
  uint32_t revolution_count_ = 0;
  uint32_t rejected_ = 0;
};

}  // namespace speed_core
