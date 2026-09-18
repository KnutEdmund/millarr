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
web page over a marked distance.
