// The profile currently driving the engine.
#pragma once
#include "profile.h"
#include "speed_engine.h"

namespace active_profile {
void apply(speed_core::SpeedEngine& engine);
const speed_core::Profile& current();
uint8_t slot();
}  // namespace active_profile
