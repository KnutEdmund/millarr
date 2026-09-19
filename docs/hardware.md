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

- White correction fluid, three round marks ≈ 15 mm diameter.
- Spacing: belt length / 3, measured with a tape along the belt edge. Mark the
  positions before painting. Unequal spacing does not affect revolution speed but
  shows up in the mark-share bars on the web page.
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

Measure once with a tape (typical 2.7–3.0 m). Set `BELT_LENGTH_M` in
`include/config.h`. Cross-check: walk the belt by hand and count revolutions on the
web page over a marked distance. `BELT_LENGTH_M`/`MARKS_COUNT` only seed the
built-in default profile the first time the device boots; after that, per-treadmill
belt length and mark spacing live in profiles on the **Settings** page (see
`docs/api.md`), not in `config.h`.

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
