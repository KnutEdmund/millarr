// Bluetooth Running Speed and Cadence (RSC) measurement encoding.
// Pure C++: the firmware's BLE module only ships the bytes produced here.
#pragma once
#include <cstddef>
#include <cstdint>

namespace speed_core {

constexpr size_t RSC_MEASUREMENT_LEN = 4;
constexpr uint8_t RSC_FLAG_RUNNING = 0x04;                // bit0 stride, bit1 total distance stay 0
constexpr uint16_t RSC_FEATURE_WALK_RUN_STATUS = 0x0004;  // RSC Feature: walking/running status only
constexpr uint8_t RSC_LOCATION_OTHER = 0;                 // Sensor Location: Other

struct RscMeasurement {
  uint8_t flags;
  uint16_t speed_q8;    // instantaneous speed in 1/256 m/s
  uint8_t cadence_spm;  // instantaneous cadence, steps per minute
};

// Negative or NaN speed becomes 0. Speed rounds to the nearest 1/256 m/s and
// saturates at 0xFFFF. The running flag is set when speed >= running_min_mps.
RscMeasurement makeRscMeasurement(float speed_mps, uint8_t cadence_spm, float running_min_mps);

// Little-endian RSC Measurement packet: flags, speed, cadence. Returns the length (4).
size_t encodeRscMeasurement(const RscMeasurement& m, uint8_t out[RSC_MEASUREMENT_LEN]);

}  // namespace speed_core
