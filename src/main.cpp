#include <Arduino.h>

#include "active_profile.h"
#include "adc_probe.h"
#include "ble_rsc.h"
#include "config.h"
#include "flash_log.h"
#include "fw_version.h"
#include "profile_store.h"
#include "pulse_isr.h"
#include "speed_engine.h"
#include "web_server.h"
#include "wifi_ap.h"

using speed_core::EngineConfig;
using speed_core::EngineStatus;
using speed_core::SpeedEngine;

static const EngineConfig kConfig = speed_core::makeEqualConfig(
    BELT_LENGTH_M, MARKS_COUNT, DEBOUNCE_US,
    speed_core::stopTimeoutUs(BELT_LENGTH_M / MARKS_COUNT, MIN_SPEED_MPS, STOP_TIMEOUT_MIN_US, STOP_TIMEOUT_MAX_US),
    MAX_SPEED_MPS);
SpeedEngine engine(kConfig);

static uint32_t last_status_ms = 0;

static uint32_t boot_ms = 0;
static uint32_t boot_low_since_ms = 0;
static bool boot_low = false;
static bool boot_watch_done = false;

// For the first 10 s after boot: holding BOOT (GPIO0) low for 3 s resets the
// AP credentials to the config.h defaults and restarts the AP. The button is
// only sampled after boot, so it never interferes with the ROM's strapping.
static void watchBootButton() {
  if (boot_watch_done) return;
  const uint32_t now = millis();
  if (now - boot_ms > 10000) {
    boot_watch_done = true;
    return;
  }
  const bool low = digitalRead(0) == LOW;
  if (low && !boot_low) {
    boot_low = true;
    boot_low_since_ms = now;
  }
  if (!low) boot_low = false;
  if (low && now - boot_low_since_ms >= 3000) {
    boot_watch_done = true;
    profile_store::resetWifi();
    Serial.println("wifi settings reset (BOOT held)");
    String ssid, pass;
    profile_store::loadWifi(ssid, pass);
    wifi_ap::restart(ssid.c_str(), pass.c_str(), MDNS_HOSTNAME);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);
  Serial.println();
  Serial.println("treadmill-sensor boot");
  Serial.printf("defaults belt=%.3fm marks=%u debounce=%luus min_speed=%.2fm/s max_speed=%.1fm/s\n",
                (double)BELT_LENGTH_M, (unsigned)MARKS_COUNT, (unsigned long)DEBOUNCE_US,
                (double)MIN_SPEED_MPS, (double)MAX_SPEED_MPS);

  profile_store::begin(BELT_LENGTH_M, MARKS_COUNT);
  pinMode(0, INPUT_PULLUP);
  boot_ms = millis();
  active_profile::apply(engine);
  flash_log::begin(FW_GIT_HASH);

  pulse_isr::begin(SENSOR_DO_PIN, DEBOUNCE_US);
  adc_probe::begin(SENSOR_AO_PIN);

  String ssid, pass;
  profile_store::loadWifi(ssid, pass);
  wifi_ap::begin(ssid.c_str(), pass.c_str(), MDNS_HOSTNAME);
  web_server::begin(engine);
  ble_rsc::begin(BLE_DEVICE_NAME, FW_GIT_HASH);
}

void loop() {
  uint32_t t;
  while (pulse_isr::pop(t)) {
    engine.onPulse(t);
    flash_log::observe(engine.log());
  }
  flash_log::service(micros());

  engine.tick(micros());
  flash_log::observe(engine.log());  // tick() may commit a held-back pulse
  watchBootButton();

  web_server::handle();
  ble_rsc::service(millis(), engine.speedAt(micros()));

  uint32_t now_ms = millis();
  if (now_ms - last_status_ms >= STATUS_INTERVAL_MS) {
    last_status_ms = now_ms;
    EngineStatus s = engine.status(micros());
    const speed_core::FlashStatus f = flash_log::status();
    const speed_core::BleStatus b = ble_rsc::status();
    Serial.printf("speed=%.2fkm/h dist=%.1fm pulses=%lu rev=%lu dropped=%lu rejected=%lu implausible=%lu"
                  " flash=%s session=%lu records=%lu used=%luB ble=%s n=%lu\n",
                  (double)s.speed_mps * 3.6, (double)s.distance_m, (unsigned long)s.pulse_count,
                  (unsigned long)s.revolution_count, (unsigned long)pulse_isr::dropped(),
                  (unsigned long)(s.rejected_debounce + pulse_isr::rejected()),
                  (unsigned long)s.rejected_implausible, f.ok ? (f.full ? "full" : "ok") : "off",
                  (unsigned long)f.session_id, (unsigned long)f.session_records, (unsigned long)f.used_bytes,
                  b.connected ? "conn" : (b.advertising ? "adv" : "off"), (unsigned long)b.notifications);
  }
}
