#include <Arduino.h>
#include "config.h"

static uint32_t last_status_ms = 0;
static uint32_t ticks = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);
  Serial.println();
  Serial.println("treadmill-sensor boot");
}

void loop() {
  uint32_t now = millis();
  if (now - last_status_ms >= STATUS_INTERVAL_MS) {
    last_status_ms = now;
    Serial.printf("tick=%lu uptime_s=%lu\n", (unsigned long)ticks++, (unsigned long)(now / 1000));
  }
}
