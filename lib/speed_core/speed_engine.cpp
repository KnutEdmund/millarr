#include "speed_engine.h"

#include <cmath>

namespace speed_core {

SpeedEngine::SpeedEngine(const EngineConfig& cfg) : cfg_(cfg) { clampConfig(); }

void SpeedEngine::clampConfig() {
  if (cfg_.marks_count < 1) cfg_.marks_count = 1;
  if (cfg_.marks_count > MAX_MARKS) cfg_.marks_count = MAX_MARKS;
}

void SpeedEngine::reconfigure(const EngineConfig& cfg) {
  cfg_ = cfg;
  clampConfig();
  startRun();
  ever_locked_ = false;
  stats_head_ = 0;
  stats_count_ = 0;
}

void SpeedEngine::startRun() {
  run_count_ = 0;
  last_rev_speed_ = 0.0f;
  phase_locked_ = false;
  phase_shift_ = 0;
  for (uint8_t i = 0; i < MAX_MARKS; ++i) {
    recent_ts_[i] = 0;
    cur_rev_intervals_[i] = 0;
  }
}

float SpeedEngine::distanceForSlot(uint8_t slot) const {
  if (!phase_locked_) return meanMarkDistance();
  const uint8_t marks = cfg_.marks_count;
  const uint8_t phys = static_cast<uint8_t>((slot + phase_shift_) % marks);
  return cfg_.mark_distance_m[(phys + marks - 1) % marks];
}

// Choose the rotation of the configured distances that makes the intervals of
// the first revolution imply the most uniform speed. Ties keep the smallest shift,
// so equal distances always lock to shift 0 (run-relative == physical).
void SpeedEngine::lockPhase() {
  const uint8_t marks = cfg_.marks_count;
  uint8_t best = 0;
  float best_spread = -1.0f;
  for (uint8_t sh = 0; sh < marks; ++sh) {
    float lo = 1e30f, hi = 0.0f;
    for (uint8_t s = 0; s < marks; ++s) {
      const uint32_t iv = cur_rev_intervals_[s];
      if (iv == 0) continue;
      const uint8_t phys = static_cast<uint8_t>((s + sh) % marks);
      const float speed = cfg_.mark_distance_m[(phys + marks - 1) % marks] / (iv * 1e-6f);
      if (speed < lo) lo = speed;
      if (speed > hi) hi = speed;
    }
    const float spread = hi - lo;
    if (best_spread < 0.0f || spread < best_spread) {
      best_spread = spread;
      best = sh;
    }
  }
  phase_shift_ = best;
  phase_locked_ = true;
  if (ever_locked_ && best != last_locked_shift_) {
    stats_head_ = 0;
    stats_count_ = 0;
  }
  last_locked_shift_ = best;
  ever_locked_ = true;
}

void SpeedEngine::onPulse(uint32_t t) {
  if (has_last_) {
    uint32_t gap = t - last_t_;  // unsigned: correct across a single micros() wrap
    if (gap < cfg_.debounce_us) {
      ++rejected_;
      return;
    }
    if (gap > cfg_.stop_timeout_us) startRun();
  }

  const uint8_t marks = cfg_.marks_count;
  const uint8_t slot = static_cast<uint8_t>(run_count_ % marks);
  const uint32_t interval = (has_last_ && run_count_ > 0) ? (t - last_t_) : 0;
  const uint32_t oldest = recent_ts_[slot];  // timestamp `marks` pulses ago, if run_count_ >= marks
  recent_ts_[slot] = t;
  if (interval > 0) cur_rev_intervals_[slot] = interval;

  // The pulse completing the first revolution of a run fixes the phase before
  // its own mark values are computed.
  const bool locked_now = !phase_locked_ && interval > 0 && run_count_ >= marks;
  if (locked_now) lockPhase();

  float mark_speed = 0.0f;
  float rev_speed = 0.0f;

  if (interval > 0) {
    const float d = distanceForSlot(slot);
    mark_speed = d / (interval * 1e-6f);
    distance_m_ += locked_now ? meanMarkDistance() : d;
    ++interval_count_;
  }

  if (run_count_ >= marks) {
    uint32_t rev_us = t - oldest;
    rev_speed = beltLength(cfg_) / (rev_us * 1e-6f);
    last_rev_speed_ = rev_speed;
    if (slot == 0) {
      ++revolution_count_;
      pushRevolutionStats(rev_speed);
    }
  }

  PulseRecord r{};
  r.seq = next_seq_++;
  r.t_us = t;
  r.interval_us = interval;
  r.mark_index = phase_locked_ ? static_cast<uint8_t>((slot + phase_shift_) % marks) : slot;
  r.mark_speed_mps = mark_speed;
  r.rev_speed_mps = rev_speed;
  log_.push(r);

  last_t_ = t;
  has_last_ = true;
  ++run_count_;
  ++pulse_count_;
}

void SpeedEngine::tick(uint32_t now_us) {
  if (has_last_ && (now_us - last_t_) > cfg_.stop_timeout_us) {
    has_last_ = false;
    startRun();  // zeroes last_rev_speed_, the revolution window and the phase
  }
}

void SpeedEngine::pushRevolutionStats(float rev_speed) {
  const uint8_t marks = cfg_.marks_count;
  for (uint8_t m = 0; m < MAX_MARKS; ++m) rev_intervals_[stats_head_][m] = 0;
  for (uint8_t s = 0; s < marks; ++s) {
    const uint8_t phys = static_cast<uint8_t>((s + phase_shift_) % marks);
    rev_intervals_[stats_head_][phys] = cur_rev_intervals_[s];
  }
  rev_speeds_[stats_head_] = rev_speed;
  stats_head_ = static_cast<uint8_t>((stats_head_ + 1) % STATS_REVOLUTIONS);
  if (stats_count_ < STATS_REVOLUTIONS) ++stats_count_;
}

bool SpeedEngine::running(uint32_t now_us) const {
  return has_last_ && (now_us - last_t_) <= cfg_.stop_timeout_us;
}

float SpeedEngine::speedAt(uint32_t now_us) const {
  return running(now_us) ? last_rev_speed_ : 0.0f;
}

EngineStatus SpeedEngine::status(uint32_t now_us) const {
  EngineStatus s{};
  s.running = running(now_us);
  s.phase_locked = phase_locked_;
  s.speed_mps = s.running ? last_rev_speed_ : 0.0f;
  s.distance_m = distance_m_;
  s.pulse_count = pulse_count_;
  s.revolution_count = revolution_count_;
  s.rejected_debounce = rejected_;
  s.stats_revolutions = stats_count_;
  s.log_size = static_cast<uint32_t>(log_.size());
  s.log_capacity = static_cast<uint32_t>(log_.capacity());
  s.log_overwritten = log_.overwritten();
  s.last_seq = log_.lastSeq();

  uint64_t per_mark[MAX_MARKS] = {};
  uint64_t total = 0;
  double sum = 0.0, sum_sq = 0.0;
  for (uint8_t i = 0; i < stats_count_; ++i) {
    for (uint8_t m = 0; m < cfg_.marks_count; ++m) {
      per_mark[m] += rev_intervals_[i][m];
      total += rev_intervals_[i][m];
    }
    sum += rev_speeds_[i];
    sum_sq += static_cast<double>(rev_speeds_[i]) * rev_speeds_[i];
  }
  for (uint8_t m = 0; m < MAX_MARKS; ++m) {
    s.mark_share[m] = total ? static_cast<float>(static_cast<double>(per_mark[m]) / total) : 0.0f;
  }
  if (stats_count_ >= 2) {
    double mean = sum / stats_count_;
    double var = sum_sq / stats_count_ - mean * mean;
    s.rev_speed_stddev = var > 0 ? static_cast<float>(std::sqrt(var)) : 0.0f;
  }
  return s;
}

void SpeedEngine::reset() {
  log_.clear();
  has_last_ = false;
  last_t_ = 0;
  next_seq_ = 1;
  startRun();
  ever_locked_ = false;
  stats_head_ = 0;
  stats_count_ = 0;
  pulse_count_ = 0;
  interval_count_ = 0;
  revolution_count_ = 0;
  rejected_ = 0;
  distance_m_ = 0.0f;
}

}  // namespace speed_core
