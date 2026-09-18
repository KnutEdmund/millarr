#include "pulse_isr.h"

#include <Arduino.h>

namespace pulse_isr {
namespace {

// Single-producer/single-consumer ring: attachInterrupt() installs the ISR on the core
// that calls it (setup() on core 1), and loop() runs on core 1 too, so the ISR preempts
// the consumer rather than racing it. 64 slots hold 63 usable entries.
constexpr uint8_t kRing = 64;
volatile uint32_t ring[kRing];
volatile uint8_t head = 0;  // written by ISR
volatile uint8_t tail = 0;  // written by loop()
volatile uint32_t dropped_count = 0;
volatile uint32_t rejected_count = 0;
volatile uint32_t last_us = 0;
uint32_t debounce_us_ = 0;

void IRAM_ATTR onEdge() {
  uint32_t now = micros();
  if (now - last_us < debounce_us_) {
    ++rejected_count;
    return;
  }
  last_us = now;
  uint8_t next = (head + 1) % kRing;
  if (next == tail) {
    ++dropped_count;
    return;
  }
  ring[head] = now;
  head = next;
}

}  // namespace

void begin(uint8_t pin, uint32_t debounce_us) {
  debounce_us_ = debounce_us;
  pinMode(pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin), onEdge, FALLING);
}

bool pop(uint32_t& t_us) {
  if (tail == head) return false;
  t_us = ring[tail];
  tail = (tail + 1) % kRing;
  return true;
}

uint32_t dropped() { return dropped_count; }
uint32_t rejected() { return rejected_count; }

void resetCounters() {
  noInterrupts();
  dropped_count = 0;
  rejected_count = 0;
  interrupts();
}

}  // namespace pulse_isr
