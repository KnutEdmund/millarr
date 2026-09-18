#include "speed_engine.h"

#include <cmath>

namespace speed_core {

SpeedEngine::SpeedEngine(const EngineConfig& cfg) : cfg_(cfg) {
  if (cfg_.marks_count < 1) cfg_.marks_count = 1;
  if (cfg_.marks_count > MAX_MARKS) cfg_.marks_count = MAX_MARKS;
}

void SpeedEngine::startRun() {
  run_count_ = 0;
  last_rev_speed_ = 0.0f;
  for (uint8_t i = 0; i < MAX_MARKS; ++i) {
    recent_ts_[i] = 0;
    cur_rev_intervals_[i] = 0;
  }
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

  float mark_speed = 0.0f;
  float rev_speed = 0.0f;

  if (interval > 0) {
    mark_speed = markDistance() / (interval * 1e-6f);
    ++interval_count_;
    cur_rev_intervals_[slot] = interval;
  }

  if (run_count_ >= marks) {
    uint32_t rev_us = t - oldest;
    rev_speed = cfg_.belt_length_m / (rev_us * 1e-6f);
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
  r.mark_index = slot;
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
    startRun();  // zeroes last_rev_speed_ and the revolution window
  }
}

void SpeedEngine::pushRevolutionStats(float rev_speed) {
  for (uint8_t m = 0; m < MAX_MARKS; ++m) rev_intervals_[stats_head_][m] = cur_rev_intervals_[m];
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
  s.speed_mps = s.running ? last_rev_speed_ : 0.0f;
  s.distance_m = interval_count_ * markDistance();
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
  stats_head_ = 0;
  stats_count_ = 0;
  pulse_count_ = 0;
  interval_count_ = 0;
  revolution_count_ = 0;
  rejected_ = 0;
}

}  // namespace speed_core
