// src/wifi_ap.cpp
#include "wifi_ap.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "dns_reply.h"

namespace wifi_ap {

namespace {
// Own responder instead of the framework's DNSServer, which garbles replies to
// EDNS queries and answers AAAA questions with an A record.
WiFiUDP dns;
constexpr uint16_t kDnsPort = 53;
uint8_t dns_in[dns_reply::kMaxPacket];
uint8_t dns_out[dns_reply::kMaxPacket];
}  // namespace

void begin(const char* ssid, const char* password, const char* mdns_host) {
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(ssid, password);
  Serial.printf("softAP %s: %s ip=%s\n", ssid, ok ? "up" : "FAILED",
                WiFi.softAPIP().toString().c_str());
  if (dns.begin(kDnsPort)) {
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

void service() {
  const int size = dns.parsePacket();
  if (size <= 0) return;
  const int n = dns.read(dns_in, sizeof dns_in);
  if (n <= 0 || size > static_cast<int>(sizeof dns_in)) return;  // oversized: drop
  const IPAddress ip = WiFi.softAPIP();
  const uint8_t ipv4[4] = {ip[0], ip[1], ip[2], ip[3]};
  const size_t len = dns_reply::build(dns_in, static_cast<size_t>(n), ipv4, dns_out, sizeof dns_out);
  if (len == 0) return;
  dns.beginPacket(dns.remoteIP(), dns.remotePort());
  dns.write(dns_out, len);
  dns.endPacket();
}

}  // namespace wifi_ap
