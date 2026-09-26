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

The belt length anchors absolute distance: calibration redistributes the per-mark
distances but keeps their sum.

- **Belt treadmill:** measure the belt length once with a tape along the belt and
  enter it as equal distances first (`type=belt`).
- **Slat treadmill:** never use a tape or a string around the loop (it follows the
  slat tops and over-measures by about 2π·h). Count the slats over one revolution
  and measure 20 pitches mid-flat, then enter count and pitch with `type=slats`;
  see `docs/hardware.md`, "Slat belts". `/api/profiles` shows `slat_count`,
  `slat_pitch_m` and the resulting `belt_length_m`.

1. Create or select a profile for this treadmill on the **Settings** page.
2. Run the treadmill at its lowest speed until the diagnostic page shows "phase
   locked" and the "Use measured distances" button enables (10 steady revolutions
   with the phase locked). Click it and accept the proposed distances.
3. Record the saved per-mark distances from `/api/profiles`.
4. Repeat once more from a fresh run. The second set of distances must agree with
   the first within 1 %.

## 2b. Session log

After any run that is going to be analysed, download the session from the
Sessions table on the diagnostic page (closed sessions can be downloaded while the
belt runs; the active one is flushed first) rather than the RAM-log CSV, which only
holds the last 2048 records.

1. `head -5` of the file shows the session id, firmware hash and the profile
   geometry in effect. Note both in the results document.
2. `seq` must be contiguous from 1: `awk -F, '!/^#/ && NR>1 && $1!=n+1 {print}; {n=$1}'`
   prints nothing.
3. `isr_dropped` in `/api/status` is still 0 after the download.
4. `rejected_implausible` should be 0 or a single start-up pulse; more than that
   during a run means a mark is double-triggering.

## 3. Treadmill, no runner

Hold each of 6, 8, 10, 12, 14 km/h for 60 s, plus one step at the lowest speed the
console offers (the sensor's floor is 0.5 m/s = 1.8 km/h unless the 3 s stop
timeout applies; the current value is `config.stop_timeout_us` in `/api/status`).
For each, record:

| Console km/h | Sensor mean km/h | Std dev % | Dropped | Rejected | Mark share deviation |
|---|---|---|---|---|---|

Sensor mean: average `rev_speed_mps` from the CSV over the last 40 s of each step.

## 4. Treadmill, with runner

Repeat step 3 at two speeds with a runner on the belt. Expect a higher std dev
(belt slip under foot strike) and a slightly lower mean than without a runner.
Both are real effects, not sensor error.

## Acceptance for Milestone 2

- Std dev of revolution speed < 1.5 % at constant speed, no runner.
- Zero dropped pulses across all steps. A dropped or missed pulse shifts the
  run-relative mark index permanently, so every later revolution boundary lands
  one mark late and the affected revolution reads roughly one mark-distance short.
- Mark-share deviations documented (they will be used for per-mark calibration).
- Sensor mean within 3 % of console at every step, or the offset is explained.
