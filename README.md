# Treadmill Belt Speed Sensor

A self-powered ESP32 sensor that measures **actual belt movement** on a treadmill
with an IR reflective sensor and three white marks on the belt, and (in a later
milestone) broadcasts Bluetooth LE Running Speed and Cadence to watches and apps.

Milestone 1 (this repo state) is a **data-quality logger**: the device timestamps
every mark pulse to the microsecond, computes revolution-based speed, hosts its own
Wi-Fi access point, and serves a diagnostic web page with live speed, per-mark
statistics, sensor signal capture and CSV export.

## Hardware

- ESP32 dev board (38-pin, CP2102, USB-C)
- TCRT5000 IR reflective sensor module (DO → GPIO18, AO → GPIO34)
- WeMos 18650 battery shield V3 + 18650 cell
- Three white correction-fluid marks on the belt, 15 mm diameter, equidistant

See `docs/hardware.md` for wiring and belt marking.

## Build and flash

Requires [PlatformIO Core](https://platformio.org/) (`brew install platformio`).

```bash
pio test -e native            # unit tests for the speed engine, on your computer
pio run -e esp32dev           # build firmware
pio run -e esp32dev -t upload # flash (port is set in platformio.ini)
pio device monitor            # serial status line, 115200 baud
```

## Use

1. Power the sensor. It starts a Wi-Fi network `treadmill-sensor` (password in
   `include/config.h`, change it).
2. Connect a phone or laptop and open `http://192.168.4.1/` (or `http://treadmill.local/`).
3. Follow `docs/test-protocol.md` to verify the sensor before trusting the numbers.

## Layout

| Path | What |
|------|------|
| `lib/speed_core/` | Pure C++ speed engine, log and formatters (unit-tested) |
| `src/` | Firmware: ISR, Wi-Fi AP, HTTP server, ADC probe |
| `web/index.html` | Diagnostic page, embedded into firmware at build time |
| `test/` | Unity tests (`pio test -e native`) |
| `docs/` | Hardware, test protocol, HTTP API, design specs and plans |

## Roadmap

- **Milestone 2:** BLE RSC (foot pod) for Garmin watches, via NimBLE.
- **Milestone 3:** captive portal with per-treadmill profiles stored in NVS.

## License

MIT
