# Decision Ledger

Dated, append-only. Newest first.

## 2026-09-18

- **Log capacity 2048 records** (not 4096 as in the spec). A record is 24 bytes; 4096
  would put ≈ 96 KB in `.bss` next to the Wi-Fi stack. 2048 ≈ 11 min at 10 km/h and
  CSV can be downloaded mid-run.
- **`mark_index` is run-relative** (pulse index since last restart mod marks), because
  belt phase after a stop is unknown and per-mark stats must be consistent within a run.
- **Distance counts intervals**, not pulses: the first pulse after a stop has no known
  travel.
- **Platform pinned** to the `espressif32` version resolved on first build.

## 2026-09-07

- **PlatformIO CLI** as the only toolchain.
- **Revolution speed over one full belt loop** as the primary speed: mark spacing
  error cancels exactly; no moving average on top.
- **Synchronous `WebServer` + JSON polling**, not ESPAsyncWebServer, for Milestone 1.
- **AO on GPIO34 (ADC1)** because ADC2 is unusable while Wi-Fi is on.
- **BLE deferred to Milestone 2**, to be implemented with NimBLE.
