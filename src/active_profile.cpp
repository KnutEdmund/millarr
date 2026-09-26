#include "active_profile.h"

#include <Arduino.h>

#include "config.h"
#include "profile_store.h"

namespace active_profile {
namespace {
speed_core::Profile current_ = speed_core::makeDefaultProfile("Home treadmill", BELT_LENGTH_M, MARKS_COUNT);
uint8_t slot_ = 0;
}  // namespace

void apply(speed_core::SpeedEngine& engine) {
  slot_ = profile_store::activeSlot();
  if (!profile_store::load(slot_, current_)) {
    current_ = speed_core::makeDefaultProfile("Home treadmill", BELT_LENGTH_M, MARKS_COUNT);
    Serial.println("active profile: slot unreadable, using built-in default");
  }
  const uint32_t stop_us = speed_core::stopTimeoutFor(current_, MIN_SPEED_MPS, STOP_TIMEOUT_MIN_US,
                                                      STOP_TIMEOUT_MAX_US);
  engine.reconfigure(speed_core::makeEngineConfig(current_, DEBOUNCE_US, stop_us, MAX_SPEED_MPS));
  Serial.printf("profile %u '%s' type=%s marks=%u belt=%.3fm stop=%lums\n", (unsigned)slot_,
                current_.name, speed_core::typeName(current_.type), (unsigned)current_.marks,
                (double)speed_core::beltLength(current_), (unsigned long)(stop_us / 1000));
}

const speed_core::Profile& current() { return current_; }
uint8_t slot() { return slot_; }

}  // namespace active_profile
