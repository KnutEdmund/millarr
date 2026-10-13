// src/wifi_ap.h
#pragma once

namespace wifi_ap {
// Start a WPA2 soft-AP, advertise <mdns_host>.local and answer every DNS query
// with the AP's own address, so phones detect a captive portal and open the
// dashboard themselves. Prints the IP to Serial.
void begin(const char* ssid, const char* password, const char* mdns_host);

// Tear the AP down and bring it up again with new credentials.
void restart(const char* ssid, const char* password, const char* mdns_host);

// Answer pending DNS queries. Call from loop().
void service();
}  // namespace wifi_ap
