# Treadmill Belt Speed Sensor

A self-powered ESP32 sensor that measures **actual belt movement** on a treadmill
with an IR reflective sensor and marks on the belt, and (in a later milestone)
broadcasts Bluetooth LE Running Speed and Cadence to watches and apps.

The device timestamps every mark pulse to the microsecond, computes revolution-based
speed with per-mark distance correction, hosts its own Wi-Fi access point, and serves
a diagnostic web page with live speed, per-mark statistics, sensor signal capture and
CSV export. Per-treadmill profiles (name, mark count, measured mark distances) and
the Wi-Fi access point credentials are configured from a **Settings** page and
persisted in NVS across power cycles.

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
2. Connect a phone or laptop to that network and open `http://192.168.4.1/` (or
   `http://treadmill.local/`) **with the `http://` prefix typed explicitly** — phone
   browsers otherwise try to upgrade the address to HTTPS, which the sensor does not
   serve.
3. Open **Settings** to create a profile per treadmill — up to 20 — with a name,
   optional notes (location, model, quirks), marks, and the measured distance
   between marks; a "+ New profile" button on both pages jumps straight to a blank
   profile form. Also change the sensor's Wi-Fi name and password here. The active
   profile survives power cycles. Settings also lets you download all profiles as
   a JSON backup and restore them onto the same or another sensor.
4. Follow `docs/test-protocol.md` to verify the sensor before trusting the numbers.

## Layout

| Path | What |
|------|------|
| `lib/speed_core/` | Pure C++ speed engine, log, profile model and formatters (unit-tested) |
| `src/` | Firmware: ISR, Wi-Fi AP, HTTP server, ADC probe, `src/profile_store.cpp` (profiles and Wi-Fi settings in NVS) |
| `web/index.html` | Diagnostic page, embedded into firmware at build time |
| `web/settings.html` | Settings page: profile editor and Wi-Fi credentials, embedded at build time |
| `test/` | Unity tests (`pio test -e native`) |
| `docs/` | Hardware, test protocol, HTTP API, design specs and plans |

## Roadmap

- **Milestone 2:** BLE RSC (foot pod) for Garmin watches, via NimBLE.
- **Milestone 3 (profiles and web configuration):** done.

## License

MIT
