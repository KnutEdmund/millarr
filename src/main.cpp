#include <Arduino.h>

#include "adc_probe.h"
#include "config.h"
#include "pulse_isr.h"
#include "speed_engine.h"
#include "web_server.h"
#include "wifi_ap.h"

using speed_core::EngineConfig;
using speed_core::EngineStatus;
using speed_core::SpeedEngine;

static const EngineConfig kConfig =
    speed_core::makeEqualConfig(BELT_LENGTH_M, MARKS_COUNT, DEBOUNCE_US, STOP_TIMEOUT_US);
SpeedEngine engine(kConfig);

static uint32_t last_status_ms = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);
  Serial.println();
  Serial.println("treadmill-sensor boot");
  Serial.printf("belt=%.3fm marks=%u debounce=%luus stop=%luus\n", (double)BELT_LENGTH_M,
                (unsigned)MARKS_COUNT, (unsigned long)DEBOUNCE_US, (unsigned long)STOP_TIMEOUT_US);
  pulse_isr::begin(SENSOR_DO_PIN, DEBOUNCE_US);
  adc_probe::begin(SENSOR_AO_PIN);

  wifi_ap::begin(AP_SSID, AP_PASSWORD, MDNS_HOSTNAME);
  web_server::begin(engine);
}

void loop() {
  uint32_t t;
  while (pulse_isr::pop(t)) engine.onPulse(t);

  engine.tick(micros());

  web_server::handle();

  uint32_t now_ms = millis();
  if (now_ms - last_status_ms >= STATUS_INTERVAL_MS) {
    last_status_ms = now_ms;
    EngineStatus s = engine.status(micros());
    Serial.printf("speed=%.2fkm/h dist=%.1fm pulses=%lu rev=%lu dropped=%lu rejected=%lu\n",
                  (double)s.speed_mps * 3.6, (double)s.distance_m, (unsigned long)s.pulse_count,
                  (unsigned long)s.revolution_count, (unsigned long)pulse_isr::dropped(),
                  (unsigned long)(s.rejected_debounce + pulse_isr::rejected()));
  }
}
