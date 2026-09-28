# Measuring a slat belt: count × pitch

Field guide for the one measurement that fixes the belt length of a slat treadmill.
Background and why a tape around the loop is wrong: `docs/hardware.md`, "Slat belts".
Takes about 15 minutes. Two people make step 2 easier but one is enough.

## You need

- A tape measure or folding rule at least 1.5 m long, readable to 1 mm.
- Masking tape and a fine pen, or a piece of chalk.
- A short ruler (for the height check in step 3).
- A phone camera to photograph every reading before you write it down.
- The treadmill **switched off** and, if you can, unplugged. You will move the belt
  by hand and lean over it.

## Step 1: count the slats

1. Stick a piece of tape on one slat, next to the painted sensor mark if there is
   one, so you have an unambiguous start slat.
2. Push the belt slowly by hand, counting each slat as its leading edge passes a
   fixed point (the front edge of the frame, a chalk line on the side rail).
   Do not count the start slat twice: it is slat 1 when it passes the point and
   you stop when it comes back to the same point.
3. Count twice, once in each direction if the belt allows it. Both counts must
   agree exactly. One slat wrong is 1.7 % on a 60-slat belt, larger than the
   whole effect being measured.

Record: `slat_count = N` (integer, no uncertainty).

## Step 2: measure 20 pitches mid-flat

The pitch is the repeat distance between corresponding features on adjacent slats.
It is **not** the slat width and **not** the visible top surface: with bevelled or
overlapping slats those two differ from each other and neither is guaranteed to be
the pitch. Measuring 20 repeats directly settles it to 0.1 %.

1. Work in the **middle of the flat section**, at least five slats away from either
   drum. Near the drums the slats splay and the pitch at the surface is wrong.
   If the deck is visibly curved where you stand, move to the flattest part: on a
   convex curve the slat tops splay by roughly h/R, which is 0.5–1 % for a 2 cm
   slat on a 2–4 m radius.
2. Choose one feature and use the same feature on every slat: the **leading edge
   of the top face** works well. Mark it on slat 1 and on slat 21 with a pen tick on
   masking tape. Count the slats between the ticks twice: slat 1 to slat 21 is 20
   pitches, not 21.
3. Lay the tape along the belt, parallel to the direction of travel, near one edge
   where it lies flat on the slat tops. It must not bow sideways or lift off the
   surface. Read tick to tick to the nearest millimetre and photograph the reading.
4. Repeat the reading:
   - at the other belt edge (checks that the slats are square to the travel);
   - after pushing the belt half a revolution, so different slats are under the
     tape (checks that the pitch is uniform around the loop).
   Three readings within ±3 mm of each other are normal. A reading that stands out
   by more than that means a tick slipped or the tape bowed; redo it.

Record all raw readings, then: `slat_pitch_m = mean(D) / 20`, in metres with four
decimals (0.0600, not 0.06).

## Step 3: check the string measurement against h

If a string or tape has ever been run around the outside of the loop, it read
`N × pitch + 2π·h`, where h is the height from the slat top down to the neutral
line of the chain or belt the slats are bolted to. Measure h once with a ruler at a
drum end, from the top face of a slat to the centre of its fixing to the drive
belt or chain (look from the side, or from underneath if the guard allows).

Then `string − N × pitch` should be about `2π·h`. For the slat treadmill on 2026-09-28: the
string read about 3.74 m, so `3.74 − N × pitch` should come out near 6.28 × h.
If it does, the two measurements describe the same belt and you are done. If it is
off by several centimetres, one of the three numbers is wrong; the pitch reading is
the one to repeat first.

## Step 4: compute and compare

`belt_length_m = slat_count × slat_pitch_m`

For the slat treadmill, the numbers on the table before this measurement:

| L | Meaning |
|---|---|
| 3.504 m | console honest (`docs/results/2026-09-28-session-012.md`) |
| 3.603 m | 60 × 6.0 cm, the value in use on 2026-09-26 |
| 3.740 m | string around the outside, 2026-09-28; not a calibration value |

Any L is fine as a result. The point is that L is now measured, not argued.

## Step 5: enter it on the device

1. Open the Settings page, select the profile (profile 2 "slat treadmill").
2. Set **Type** to `Slats (count × pitch)`.
3. Enter **Slat count** and **Slat pitch (m, mid-flat)**. The page shows the
   resulting belt length as `count × pitch`; check it against your arithmetic.
4. **Save.** Mark distances are seeded as an equal split of count × pitch; a later
   calibration run ("Use measured distances") still works on top.
5. Verify with `GET /api/profiles`: `slat_count`, `slat_pitch_m` and
   `belt_length_m` for the profile.

## Step 6: write it down

- `LEDGER.md`: date, N, the raw 20-pitch readings, h, the resulting L, and which
  profile now carries it.
- The current results report: the measured L, and the sensor-vs-console figure
  rescaled by `L_new / L_old`. No session needs recapturing; pulses are timestamps.

## Mistakes that cost more than the effect you are measuring

- Leading edge on one tick, trailing edge on the other.
- Counting 21 slats as 21 pitches (it is 20).
- Measuring near a drum, or on a curved part of the deck.
- Tape bowed sideways or lifted over a bevel.
- Entering the pitch in centimetres in a field that takes metres.
