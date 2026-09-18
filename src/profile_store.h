// Persistence of treadmill profiles and Wi-Fi settings in NVS (Preferences).
#pragma once
#include <Arduino.h>

#include "profile.h"

namespace profile_store {

void begin(float default_belt_length_m, uint8_t default_marks);
bool load(uint8_t slot, speed_core::Profile& out);
bool save(uint8_t slot, const speed_core::Profile& p);
bool erase(uint8_t slot);
uint8_t count();
uint8_t activeSlot();
bool setActiveSlot(uint8_t slot);
void loadWifi(String& ssid, String& pass);
void saveWifi(const String& ssid, const String& pass);
void resetWifi();

}  // namespace profile_store
