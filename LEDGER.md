# Decision Ledger

Dated, append-only. Newest first.

## 2026-09-26 (BLE RSC)

- **Partition table `no_ota.csv`** (framework-shipped: one 2 MB app slot, 1.9 MB
  `spiffs`, NVS unchanged) supersedes the stock table below. The firmware was 940 KB
  in the 1.25 MB slot before NimBLE; OTA was never used. The LittleFS partition moved,
  so it was formatted once (session files lost, NVS profiles kept).
- **NimBLE-Arduino 2.5.1**, pinned exactly like the platform, peripheral role only,
  one connection, logging off. Bluedroid rejected: about twice the flash and 100 KB
  more RAM.

## 2026-09-26 (field-run fixes)

- **Session log on LittleFS as CSV text**, one file per seq space (boot or reset),
  created lazily on the first flush, `#` metadata header (session id, firmware
  hash, profile geometry, `t_us` wrap note) before the usual CSV header. Binary
  records were rejected: one column definition, and the download is a plain file.
- **Stock partition table**: LittleFS uses the 1.4 MB `spiffs` partition; no custom
  table. About 115 KB/h at one mark and 10 km/h.
- **Flushes from `loop()` through a 4 KB batcher**, every 16 records or 5 s;
  `onPulse` never touches flash. Oldest closed sessions are evicted at 32 files or
  when less than 96 KB headroom remains; the active session is never evicted.
- **`/api/pulses.csv` is kept** as the flash-independent fallback (RAM ring only).
- **A profile switch appends a `# profile … at_uptime_s` line**; it does not start a
  new session.
- **Closed sessions report bytes only**; an exact record count would need a full
  file read.
- **Stop timeout is derived**: longest mark distance / 0.5 m/s, clamped 3–15 s. The
  fixed 3 s floor made walking speeds invisible on a 1-mark belt (4.3 km/h floor).
- **Max-speed filter 8 m/s**, global constant, rejects (not flags) pulses that imply
  more than that over the shortest mark distance; counted as `rejected_implausible`.
- **Profile v3 (220 B): slats store `slat_count` and `slat_pitch_m`.** Slats are
  geometry only: the sensor still reads painted marks; per-slat sensing (2026-09-18
  note) stays out of scope. `mark_distance_m` stays the engine's only truth and must
  sum to count × pitch within 0.5 %, so calibration keeps working. Stored v2 blobs
  are upgraded on load (memcpy plus zeroed slat fields) instead of being dropped.
- **Slat belts are never tape-measured**: the calibration length is the chain's
  neutral line; a tape or string on the slat tops over-measures by 2π·h.

## 2026-09-24

- Backup/restore is browser-side JSON over the existing form routes; no parser on the device.

## 2026-09-19 (profiles)

- **Profile v2: 212-byte blob (name 48, notes 128); v1 blobs are treated as empty
  slots, no migration (pre-release data only). MAX_PROFILES raised to 20.**

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
