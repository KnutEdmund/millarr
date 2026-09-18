# Verification Protocol

Run these before trusting the sensor, and again after any change to marks, sensor
placement or firmware timing. Record results in `docs/results/` (create it) as
`YYYY-MM-DD-<test>.md` with the CSV export attached.

## 1. Bench

1. Flash firmware, open `http://192.168.4.1/`.
2. Move a white card past the sensor by hand ten times. Expect ten pulses, zero
   dropped, and rejected only from bounces.
3. Capture signal with the card held still on white and on black. Expect a margin of
   several hundred ADC counts.

## 2. Belt length and mark spacing

1. Measure belt length with a tape; set `BELT_LENGTH_M`.
2. Run the treadmill at the lowest speed for 2 minutes. Read the mark-share bars.
   Record each mark's deviation from 33.3 %.

## 3. Treadmill, no runner

Hold each of 6, 8, 10, 12, 14 km/h for 60 s. For each, record:

| Console km/h | Sensor mean km/h | Std dev % | Dropped | Rejected | Mark share deviation |
|---|---|---|---|---|---|

Sensor mean: average `rev_speed_mps` from the CSV over the last 40 s of each step.

## 4. Treadmill, with runner

Repeat step 3 at two speeds with a runner on the belt. Expect a higher std dev
(belt slip under foot strike) and a slightly lower mean than without a runner.
Both are real effects, not sensor error.

## Acceptance for Milestone 2

- Std dev of revolution speed < 1.5 % at constant speed, no runner.
- Zero dropped pulses across all steps.
- Mark-share deviations documented (they will be used for per-mark calibration).
- Sensor mean within 3 % of console at every step, or the offset is explained.
