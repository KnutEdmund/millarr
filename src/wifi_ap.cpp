// src/wifi_ap.cpp
#include "wifi_ap.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>

namespace wifi_ap {

namespace {
DNSServer dns;
constexpr uint16_t kDnsPort = 53;
}  // namespace

void begin(const char* ssid, const char* password, const char* mdns_host) {
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(ssid, password);
  Serial.printf("softAP %s: %s ip=%s\n", ssid, ok ? "up" : "FAILED",
                WiFi.softAPIP().toString().c_str());
  if (dns.start(kDnsPort, "*", WiFi.softAPIP())) {
    Serial.println("dns: captive portal on");
  } else {
    Serial.println("dns: failed to start");
  }
  if (MDNS.begin(mdns_host)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS: http://%s.local\n", mdns_host);
  } else {
    Serial.println("mDNS: failed to start");
  }
}

void restart(const char* ssid, const char* password, const char* mdns_host) {
  dns.stop();
  MDNS.end();
  WiFi.softAPdisconnect(true);
  delay(100);
  begin(ssid, password, mdns_host);
}

void service() { dns.processNextRequest(); }

}  // namespace wifi_ap
