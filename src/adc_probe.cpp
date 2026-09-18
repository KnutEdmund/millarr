#include "adc_probe.h"

#include <Arduino.h>

namespace adc_probe {
namespace {
uint8_t pin_ = 34;
}

void begin(uint8_t pin) {
  pin_ = pin;
  analogReadResolution(12);
  analogSetPinAttenuation(pin_, ADC_11db);  // full 0..3.3 V range
  pinMode(pin_, INPUT);
}

void capture(Capture& out) {
  const uint32_t period_us = 1000000UL / kSampleRateHz;
  out.min = 4095;
  out.max = 0;
  uint32_t next = micros();
  for (uint16_t i = 0; i < kSamples; ++i) {
    while ((int32_t)(micros() - next) < 0) {
    }
    next += period_us;
    uint16_t v = analogRead(pin_);
    out.samples[i] = v;
    if (v < out.min) out.min = v;
    if (v > out.max) out.max = v;
  }
}

}  // namespace adc_probe
