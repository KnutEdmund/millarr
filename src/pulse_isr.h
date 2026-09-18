// GPIO falling-edge interrupt that timestamps belt-mark pulses into a small
// single-producer/single-consumer ring. The engine never runs inside the ISR.
#pragma once
#include <cstdint>

namespace pulse_isr {

void begin(uint8_t pin, uint32_t debounce_us);
bool pop(uint32_t& t_us);
uint32_t dropped();
uint32_t rejected();
void resetCounters();

}  // namespace pulse_isr
