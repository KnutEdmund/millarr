// On-demand capture of the TCRT5000 analog output, to measure the contrast
// between belt and mark and tune the module's potentiometer.
#pragma once
#include <cstdint>

namespace adc_probe {

constexpr uint16_t kSamples = 400;
constexpr uint32_t kSampleRateHz = 2000;

struct Capture {
  uint16_t samples[kSamples];
  uint16_t min;
  uint16_t max;
};

void begin(uint8_t pin);
void capture(Capture& out);

}  // namespace adc_probe
