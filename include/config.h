// Compile-time configuration for the treadmill sensor firmware.
#pragma once

// --- Sensor wiring ---
#define SENSOR_DO_PIN 18      // TCRT5000 digital out, FALLING edge on mark
#define SENSOR_AO_PIN 34      // TCRT5000 analog out, ADC1 (ADC2 is unusable with Wi-Fi)

// --- Belt geometry ---
#define BELT_LENGTH_M 2.82f
#define MARKS_COUNT 3

// --- Timing (microseconds) ---
#define DEBOUNCE_US 50000UL
// Stop timeout is derived per profile: longest mark distance / MIN_SPEED_MPS,
// clamped to [STOP_TIMEOUT_MIN_US, STOP_TIMEOUT_MAX_US]. MIN_SPEED_MPS is the
// slowest belt speed the sensor can still measure (0.5 m/s = 1.8 km/h).
#define MIN_SPEED_MPS 0.5f
#define STOP_TIMEOUT_MIN_US 3000000UL
#define STOP_TIMEOUT_MAX_US 15000000UL

// --- Plausibility ---
// Pulses implying a belt speed above this are rejected (8 m/s = 28.8 km/h).
#define MAX_SPEED_MPS 8.0f

// --- Wi-Fi access point ---
#define AP_SSID "treadmill-sensor"
#define AP_PASSWORD "treadmill123"   // change before use
#define MDNS_HOSTNAME "treadmill"    // reachable as treadmill.local

// --- Serial ---
#define SERIAL_BAUD 115200
#define STATUS_INTERVAL_MS 1000

// --- Bluetooth LE (RSC foot pod) ---
#define BLE_DEVICE_NAME "millarr"
#define RSC_NOTIFY_INTERVAL_MS 1000
#define RSC_RUNNING_MIN_MPS 1.667f   // running flag at and above 6 km/h
#define RSC_CADENCE_SPM 0            // cadence comes from the HRM/watch, never from the belt
