// src/wifi_ap.cpp
#include "wifi_ap.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

namespace wifi_ap {

void begin(const char* ssid, const char* password, const char* mdns_host) {
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(ssid, password);
  Serial.printf("softAP %s: %s ip=%s\n", ssid, ok ? "up" : "FAILED",
                WiFi.softAPIP().toString().c_str());
  if (MDNS.begin(mdns_host)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS: http://%s.local\n", mdns_host);
  } else {
    Serial.println("mDNS: failed to start");
  }
}

}  // namespace wifi_ap
