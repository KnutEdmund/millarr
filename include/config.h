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
#define STOP_TIMEOUT_US 3000000UL

// --- Wi-Fi access point ---
#define AP_SSID "treadmill-sensor"
#define AP_PASSWORD "treadmill123"   // change before use
#define MDNS_HOSTNAME "treadmill"    // reachable as treadmill.local

// --- Serial ---
#define SERIAL_BAUD 115200
#define STATUS_INTERVAL_MS 1000
