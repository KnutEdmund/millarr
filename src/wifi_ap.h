// src/wifi_ap.h
#pragma once

namespace wifi_ap {
// Start a WPA2 soft-AP and advertise <mdns_host>.local. Prints the IP to Serial.
void begin(const char* ssid, const char* password, const char* mdns_host);

// Tear the AP down and bring it up again with new credentials.
void restart(const char* ssid, const char* password, const char* mdns_host);
}  // namespace wifi_ap
