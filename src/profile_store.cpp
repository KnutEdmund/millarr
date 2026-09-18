#include "profile_store.h"

#include <Preferences.h>

#include "config.h"

using speed_core::MAX_PROFILES;
using speed_core::Profile;

namespace profile_store {
namespace {

Preferences prefs;

const char* slotKey(uint8_t slot) {
  static char key[4];
  snprintf(key, sizeof key, "p%u", (unsigned)slot);
  return key;
}

}  // namespace

bool load(uint8_t slot, Profile& out) {
  if (slot >= MAX_PROFILES) return false;
  const char* key = slotKey(slot);
  if (prefs.getBytesLength(key) != sizeof(Profile)) return false;
  prefs.getBytes(key, &out, sizeof out);
  return speed_core::validate(out) == nullptr;
}

bool save(uint8_t slot, const Profile& p) {
  if (slot >= MAX_PROFILES || speed_core::validate(p) != nullptr) return false;
  return prefs.putBytes(slotKey(slot), &p, sizeof p) == sizeof p;
}

bool erase(uint8_t slot) {
  if (slot >= MAX_PROFILES) return false;
  return prefs.remove(slotKey(slot));
}

uint8_t count() {
  Profile p;
  uint8_t n = 0;
  for (uint8_t s = 0; s < MAX_PROFILES; ++s) if (load(s, p)) ++n;
  return n;
}

uint8_t activeSlot() { return prefs.getUChar("active", 0); }

bool setActiveSlot(uint8_t slot) {
  Profile p;
  if (!load(slot, p)) return false;
  return prefs.putUChar("active", slot) == 1;
}

void begin(float default_belt_length_m, uint8_t default_marks) {
  prefs.begin("millarr", false);
  if (count() == 0) {
    save(0, speed_core::makeDefaultProfile("Home treadmill", default_belt_length_m, default_marks));
    prefs.putUChar("active", 0);
    Serial.println("profile store: seeded default profile in slot 0");
  }
  Profile p;
  if (!load(activeSlot(), p)) {
    for (uint8_t s = 0; s < MAX_PROFILES; ++s) {
      if (load(s, p)) { prefs.putUChar("active", s); break; }
    }
  }
  Serial.printf("profile store: %u profile(s), active slot %u\n", (unsigned)count(), (unsigned)activeSlot());
}

void loadWifi(String& ssid, String& pass) {
  ssid = prefs.getString("ap_ssid", AP_SSID);
  pass = prefs.getString("ap_pass", AP_PASSWORD);
}

void saveWifi(const String& ssid, const String& pass) {
  prefs.putString("ap_ssid", ssid);
  prefs.putString("ap_pass", pass);
}

void resetWifi() {
  prefs.remove("ap_ssid");
  prefs.remove("ap_pass");
}

}  // namespace profile_store
