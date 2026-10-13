# Decision Ledger

Dated, append-only. Newest first.

## 2026-10-13 (spurious edges)

- **Of two edges closer than the minimum gap, the engine keeps the one that fits the
  current speed**, not always the first. A reflection about 1.1 m ahead of the tape
  produced early edges at 14+ km/h (six in one session); the first-wins rule kept the
  spurious edge and dropped the real mark, giving one revolution up to 45 % fast and the
  next 25 % slow. Without a speed yet (start of a run) the first edge still wins.
- **Every pulse is held back for the minimum gap before it is logged** (about 0.45 s
  on a 3.6 m one-mark belt), committed by the next pulse or by `tick()`. The added
  latency is below one revolution and well inside the watch's update rate; it keeps
  the log, flash file, distance and BLE speed free of edges that are later replaced,
  so nothing has to be rolled back.

## 2026-09-29 (data field route, advertising order)

- **The belt speed goes into the activity as a Connect IQ developer field, not as the
  watch's pace.** The HRM 600's Pace & Distance feature competes with a foot pod for
  the native pace slot, and the strap's running power and dynamics are worth more
  than replacing an estimated pace. The pod stays a standard RSC peripheral; a data
  field (ExtraSensor from the Connect IQ store, closed source, or a project field
  later) connects to it and records speed to FIT. The native foot-pod pairing remains
  supported and documented for anyone without such a strap.
- **Advertising payload order is fixed: flags, 16-bit service UUID list, appearance,
  name.** Garmin's Connect IQ scanner drops the service UUIDs when the UUID list is
  the last AD element (WERETECH-9123, open since 2020) and never reads scan
  responses, so a data field could not find the sensor with NimBLE's default order
  (name first, UUIDs last). The payload is printed on boot (`ble: adv payload=`).
- **The sensor must be removed from the watch's own sensor list while a data field
  uses it**: one connection at a time, and the native stack reconnects within a
  minute of every sensor boot if the pod is still stored.

## 2026-09-28 (Garmin pairing)

- **No security configuration for the foot pod.** The Instinct 2 never requests pairing
  or encryption (`enc=0 bonded=0` on every connection); NimBLE defaults stay.
- **RSC layout unchanged**: Feature 0x0004, Sensor Location 0 (other), cadence 0, no
  total distance. **Cadence 0 is accepted by the Instinct 2**: it shows pace from the
  speed field alone, so the synthesized-cadence fallback from 2026-09-26 is retired. The treadlink-style layout (0x0006, location 1, cadence 170, total
  distance) was tested while the watch was stuck and made no difference; the watch was
  the fault (bench report of 2026-09-28).
- **Connection trace on Serial is kept**: peer address, interval, MTU, disconnect
  reason with enc/bonded flags, RSC subscribe events. Printed from the NimBLE host task,
  Serial only.

## 2026-09-28 (belt length provenance)

- **Profile 2 `belt_length_m = 3.740` was a string around the outside of the loop**
  (measured 2026-09-28, about 374 cm), the method the 2026-09-26 rule forbids. Kept
  here as provenance only; it is not a calibration value. **Profile 2 carries an
  estimated pitch meanwhile: `type=slats`, 60 × 0.0600 m = 3.600 m**, flagged as
  estimated in the profile notes until the 20-pitch measurement replaces it.
- **Slat count is 60** (same day). Slats are about 6 cm wide with a bevelled edge
  and roughly 0.5 cm overlap, so the visible top of each slat is about 5.5 cm.
  60 × 6.0 = 3.60 m reproduces the 3.603 m previously in use; 60 × 5.5 = 3.30 m
  does not. The two measurements are not averaged: their difference is 2π·h, and it
  discriminates. 374 − 360 = 14 cm gives h ≈ 2.2 cm (a slat); 374 − 330 = 44 cm
  gives h ≈ 7 cm (not a slat). The string therefore favours pitch 6.0 over 5.5.
- **Pitch is measured, not inferred from slat width and overlap**: leading edge of
  slat 1 to leading edge of slat 21 mid-flat, divided by 20 (`docs/hardware.md`,
  "Slat belts"), plus h with a ruler at the drum as the check on the string. An
  honest console needs 3.504 m, i.e. pitch 5.84 cm, which "about 6" and "about 0.5"
  cannot exclude. Nothing is recaptured: session 12 rescales by L_new / 3.740.
- **Interim pitch estimate 0.0600 m (±0.001)** from geometry only, never from the
  console: the string minus 2π·h for any plausible slat height (h 1.5–3.0 cm) gives
  5.92–6.08 cm, the slat width by eye gives about 6.0, and both agree with the
  3.603 m in use on 2026-09-26. The console is what the sensor is meant to check,
  so it does not enter the estimate. At L = 3.600 the sensor reads +2.7 % vs
  console on all five session-12 plateaus.
## 2026-09-26 (BLE RSC)

- **Partition table `no_ota.csv`** (framework-shipped: one 2 MB app slot, 1.9 MB
  `spiffs`, NVS unchanged) supersedes the stock table below. The firmware was 940 KB
  in the 1.25 MB slot before NimBLE; OTA was never used. The LittleFS partition moved,
  so it was formatted once (session files lost, NVS profiles kept).
- **NimBLE-Arduino 2.5.1**, pinned exactly like the platform, peripheral role only,
  one connection, logging off. Bluedroid rejected: about twice the flash and 100 KB
  more RAM. Measured: firmware.bin 940 KB → 1,183 KB, static RAM +9 KB, heap
  102 KB → 82 KB after `NimBLEDevice::init` and advertising.
- **RSC only, minimal**: Feature 0x0004 (walking/running status), Sensor Location
  Other, no Total Distance and therefore no SC Control Point (the watch integrates
  speed and applies its own calibration factor; the sensor's distance is in the
  session log). Device Information carries the firmware git hash.
- **Cadence 0 on purpose**: the runner's cadence comes from the HRM 600 or the watch
  and a belt sensor cannot measure it. Agreed fallback if the watch rejects cadence
  0: synthesize it from speed (one constant, `RSC_CADENCE_SPM`), accepting that
  the pod then overrides the HRM cadence.
- **One packet per second from `loop()`**, also at 0 m/s, speed = `speedAt()` (newest
  revolution speed, held until the derived stop timeout). Running flag at 6 km/h.
- **Encoder in `speed_core` (`rsc.{h,cpp}`), NimBLE glue in `src/ble_rsc`.** NimBLE
  callbacks run on the host task and touch atomics only; the engine, Serial,
  LittleFS and NVS stay on `loop()`.
- **Soft-AP and BLE always on**, Wi-Fi started first. `WiFi.setSleep(false)` is
  forbidden: with both radios up it aborts in coexistence init.

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
