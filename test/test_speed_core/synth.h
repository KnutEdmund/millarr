// Synthetic pulse timestamps for a belt running at constant speed.
// mark_fraction[k] is the fraction of belt length between mark k and mark k+1
// (defaults to equal spacing). jitter_frac adds Gaussian noise to each interval.
#pragma once
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

struct SynthOptions {
  double belt_length_m = 2.82;
  int marks = 3;
  std::vector<double> mark_fraction;  // size == marks, sums to 1.0; empty = equal
  double jitter_frac = 0.0;
  uint32_t start_us = 0;
  unsigned seed = 42;
};

inline std::vector<uint32_t> synthConstant(double speed_mps, double duration_s,
                                           const SynthOptions& o = SynthOptions{}) {
  std::vector<double> frac = o.mark_fraction;
  if (frac.empty()) frac.assign(o.marks, 1.0 / o.marks);
  std::mt19937 rng(o.seed);
  std::normal_distribution<double> noise(0.0, 1.0);
  std::vector<uint32_t> out;
  double t = 0.0;
  int mark = 0;  // the mark that just passed
  out.push_back(o.start_us);
  while (true) {
    double gap = frac[mark] * o.belt_length_m / speed_mps;
    if (o.jitter_frac > 0) gap *= 1.0 + o.jitter_frac * noise(rng);
    t += gap;
    if (t > duration_s) break;
    mark = (mark + 1) % o.marks;
    out.push_back(o.start_us + static_cast<uint32_t>(std::llround(t * 1e6)));  // wraps on purpose
  }
  return out;
}
