# Hardware

## Wiring

| TCRT5000 module | ESP32 | Note |
|-----------------|-------|------|
| VCC | 5V (or 3V3) | 5 V gives stronger IR emission; DO is open-collector so 3.3 V logic is safe |
| GND | GND | |
| DO | GPIO18 | Falling edge on mark. Firmware enables the internal pull-up |
| AO | GPIO34 | ADC1, input-only. Used only for signal capture |

Power: WeMos 18650 shield → ESP32 `VIN`/5V and `GND`. During development the USB-C
cable powers the board; do not feed 5 V from both at once.

## Belt marks

- A strip of white tape across the direction of travel. All field runs so far
  (sessions 1–38) use a single tape mark. White correction fluid should work the
  same way but has not been tried.
- One mark is enough for speed and distance; more marks give faster stop
  detection and per-mark statistics. With several marks, space them at belt
  length / count, measured with a tape along the belt edge (belt treadmills; for
  slat belts see below), and mark the positions before applying. Unequal spacing
  does not affect revolution speed but shows up in the mark-share bars on the web
  page. The firmware's first-boot default profile assumes three marks.
- Location: a few centimetres from the belt edge, on the stretch that slides over
  the deck near the front motor cover, where the belt does not flap.

## Sensor placement

- Sensor face 2–8 mm from the belt, perpendicular. TCRT5000 is most sensitive at
  about 2.5 mm.
- Fix it so it cannot vibrate: a small bracket screwed to the motor cover works.
- Use the web page's "Capture signal" to see belt vs. mark contrast, then turn the
  module's potentiometer until DO switches cleanly inside that margin and the
  onboard LED blinks once per mark.
- With nothing connected, the analog input reads 0; 3.3 V on GPIO34 reads 4095,
  which is a quick way to confirm the ADC path is alive.

## Belt length

The engine needs the distance the running surface travels per revolution. For a
conventional belt, measure once with a tape along the belt (typical 2.7–3.0 m) and
enter it as the profile's mark distances on the **Settings** page (`type=belt`).
Cross-check: walk the belt by hand and count revolutions on the web page over a
marked distance. `BELT_LENGTH_M`/`MARKS_COUNT` in `include/config.h` only seed the
built-in default profile the first time the device boots; after that, per-treadmill
geometry lives in profiles (see `docs/api.md`), not in `config.h`.

## Slat belts

On a slat treadmill the calibration length is the loop length at the **neutral
line** of the chain or belt the slats are bolted to: on the flat every slat
translates exactly with the chain. A tape on top of the deck, or a string around
the outside of the loop, follows the slat **tops**, which swing around the end drums
on a larger radius than the neutral line. That path is longer by 2π·h, where h is
the height from the neutral line to the slat surface: about 10 cm for h ≈ 16 mm, a
+2.8 % speed error on a 3.6 m belt (seen on the first field run, 2026-09-26).
**Do not measure a slat belt with a tape or a string around the loop.**

Measure it on the top side only, with no assumptions about the ends:

1. **Count the slats** over one full revolution. Jog the belt slowly by hand or at
   the lowest speed and use the sensor mark as the start/stop reference.
   This is an integer with zero error.
2. **Measure the pitch over a long baseline** in the middle of the flat section,
   where the slats sit at exactly the chain pitch (they only splay near the drums):
   leading edge of slat 1 to leading edge of slat 21 is 20 pitches. ±2 mm over
   about 2 m is 0.1 %.
3. Belt length = slat count × pitch. Enter both on the Settings page with
   `type=slats`; the page computes the belt length and seeds the mark distances.
   Calibration ("Use measured distances") still works on top of it.

The sensor still reads the tape mark on a slat belt; the slat count and pitch only
replace the tape measurement.

Step-by-step field guide with the checks that catch the usual mistakes:
`docs/slat-pitch-measurement.md`.

## Session log

Every accepted pulse is also written to a session file on the ESP32's flash
(LittleFS on the 1.9 MB `spiffs` partition of the `no_ota.csv` table), so a full training session
survives without a browser connected. A new session starts at boot and at each
"Reset log" on the diagnostic page; files are listed, downloaded and deleted from
the Sessions table there. Capacity is about 115 KB per hour at one mark and
10 km/h (roughly 16 hours of the newest sessions; a 3-mark belt at 12 km/h uses
about 500 KB per hour), and the oldest closed sessions are evicted automatically.
Writes happen from the main loop in batches every 16 records or 5 s, so at most 5 s
of data is lost on power loss. `pio run -t erase` wipes the session log together
with the profiles.

## Garmin watch

The sensor advertises as a Bluetooth LE foot pod; pairing and watch settings are
in `docs/garmin.md`.

## Recovering Wi-Fi settings

If the access point name or password is forgotten: Reset or power-cycle the sensor,
wait for the boot log (about a second), then press and hold **BOOT** for 3 seconds
within the first 10 seconds after boot. The serial log prints `wifi settings reset
(BOOT held)` and the access point restarts with the defaults from
`include/config.h`. Do **not** hold BOOT while pressing EN: that puts the ESP32 into
flash-download mode and the firmware does not start. Stored profiles are not
affected.

Note when capturing serial output on a Mac without power-cycling the board first: a
capture started mid-run (no BOOT/EN reset) can open with a burst of garbled old
status lines before settling into clean output — a `pio device monitor` or
`scripts/serial_capture.py` artifact of attaching to an already-running UART, not a
firmware fault. Reset the board (or start the capture before powering it) if a clean
log from boot is needed.

Boards without a BOOT button: GPIO0 is exposed on the pin header (labelled `0`, `D0` or
`IO0`). Touching a jumper wire from **GND** to that pin for 3 seconds within 10 seconds
after a reset does exactly the same thing.

Over USB, without touching the board: `scripts/wifi_reset.py` resets the board through
the RTS line and pulls GPIO0 low through the DTR line, then confirms the log message.
This is the same mechanism the flasher uses.
