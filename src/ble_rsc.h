// BLE Running Speed and Cadence foot pod (NimBLE glue). One central at a time.
#pragma once
#include <cstdint>

#include "speed_types.h"

namespace ble_rsc {

// Start the BLE stack, register the RSC and Device Information services and
// advertise. Call after wifi_ap::begin (Wi-Fi first, then BLE).
void begin(const char* name, const char* fw_hash);

// Send one RSC measurement every RSC_NOTIFY_INTERVAL_MS while a central is
// connected, and report connection transitions on Serial. Call from loop() only.
void service(uint32_t now_ms, float speed_mps);

// Snapshot for /api/status. Call from loop() only.
speed_core::BleStatus status();

}  // namespace ble_rsc
