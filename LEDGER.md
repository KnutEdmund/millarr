# Decision Ledger

Dated, append-only. Newest first.

## 2026-09-18 (profiles)

- **Per-mark distances replace equal spacing**; belt length is always the sum.
- **Phase is locked after the first revolution of each run** by choosing the
  distance rotation with the most uniform implied speed; ties pick shift 0.
- **Writes to the API are form-encoded** so no JSON parser is needed on the device.
- **Profiles are 60-byte blobs** in NVS namespace `millarr` (one padding byte,
  always zero-initialised before save); a wrong size or version counts as an empty
  slot.
- **BOOT button (GPIO0) held 3 s within 10 s after boot resets the AP credentials.**
  A `loop()`-based watcher samples GPIO0 only after `setup()` has started, since
  holding it low across an EN reset would put the ESP32 into UART download mode
  and `setup()` would never run.
- **Slat treadmills are representable** (`type=slats`) but not implemented.
- Stats window is cleared when a restart locks a different phase than the previous
  run, so calibration never blends alignments.

## 2026-09-18

- **Log capacity 2048 records** (not 4096 as in the spec). A record is 24 bytes; 4096
  would put ≈ 96 KB in `.bss` next to the Wi-Fi stack. 2048 ≈ 11 min at 10 km/h and
  CSV can be downloaded mid-run.
- **`mark_index` is run-relative** (pulse index since last restart mod marks), because
  belt phase after a stop is unknown and per-mark stats must be consistent within a run.
- **Distance counts intervals**, not pulses: the first pulse after a stop has no known
  travel.
- **Platform pinned** to `espressif32@7.1.3`, the version resolved on first build.
- **Stop is latched by `SpeedEngine::tick()` from `loop()`**, so a `micros()` wrap
  during idle cannot resurrect a stale running state.
- **`mark_share` is a time-weighted mean** (sum of per-mark intervals over sum of
  revolution time across the window), not the mean of per-revolution ratios the
  spec describes; identical at constant speed and the better estimator under
  acceleration.

## 2026-09-07

- **PlatformIO CLI** as the only toolchain.
- **Revolution speed over one full belt loop** as the primary speed: mark spacing
  error cancels exactly; no moving average on top.
- **Synchronous `WebServer` + JSON polling**, not ESPAsyncWebServer, for Milestone 1.
- **AO on GPIO34 (ADC1)** because ADC2 is unusable while Wi-Fi is on.
- **BLE deferred to Milestone 2**, to be implemented with NimBLE.
