# HTTP API

Base URL: `http://192.168.4.1` (or `http://treadmill.local`) on the sensor's access point.

| Method | Path | Response |
|--------|------|----------|
| GET | `/` | Diagnostic page |
| GET | `/settings` | Settings page |
| GET | `/api/status` | JSON status (below) |
| GET | `/api/pulses.json?since=SEQ` | JSON array of records with `seq > SEQ`, max 500 |
| GET | `/api/pulses.csv` | Whole log as CSV download |
| GET | `/api/signal` | 200 ms ADC capture of the sensor's analog output |
| POST | `/api/reset` | Clears log, distance and counters; `{"ok":true}` |
| GET | `/api/profiles` | All stored profiles and the active id |
| POST | `/api/profiles/{id}` | Create or replace profile `id` (0–7); form fields `name`, `type` (`belt`/`slats`), `marks` (1–8), `d0`…`d{marks-1}` (metres) |
| POST | `/api/profiles/{id}/activate` | Make `id` the active profile; the engine is reconfigured immediately |
| POST | `/api/profiles/{id}/delete` | Delete `id`; refused for the active profile and for the last remaining one |
| GET | `/api/settings` | `{"ap_ssid":"…","mdns":"treadmill"}` |
| POST | `/api/settings/wifi` | Form fields `ssid` (1–32), `password` (8–63); the access point restarts about 1 s after the response |

Note on `/api/pulses.csv`: sending the whole log blocks the sensor's main loop for up
to about a second; the interrupt ring absorbs roughly 60 pulses meanwhile, so
download while the belt is stopped, or check `isr_dropped` in `/api/status` afterwards.

## Writes

`POST` bodies are `application/x-www-form-urlencoded`, not JSON — the firmware has no
JSON parser. On success the response is `{"ok":true}` (`/api/settings/wifi` adds
`"restarting_ap":true`). On failure the response is `400` or `500` with
`{"error":"reason"}`; the reason strings are listed under each endpoint below.

`POST /api/profiles/{id}` reconfigures the running engine immediately only when `{id}`
is the currently active profile; saving a different slot just writes it to NVS.

Errors from `/api/profiles/{id}`, in the order they are checked: `bad profile id`
(id isn't a single digit 0–7), `name too long`, `bad type` (not `belt` or `slats`),
`marks must be 1..8`, `missing distance` (a `d0`…`d{marks-1}` field is absent), then
any `validate()` reason below. `/api/profiles/{id}/activate`: `bad profile id`,
`empty profile` (no profile stored in that slot). `/api/profiles/{id}/delete`:
`bad profile id`, `cannot delete active profile`, `empty profile`,
`at least one profile` (refuses to delete the last remaining profile).
`/api/settings/wifi`: `ssid must be 1-32 printable characters`,
`password must be 8-63 printable characters`.

## `/api/status`

```json
{"speed_mps":2.500,"speed_kmh":9.000,"running":true,"phase_locked":true,"distance_m":12.340,
 "pulse_count":13,"revolution_count":4,"rejected_debounce":1,"isr_dropped":2,
 "mark_share":[0.330,0.330,0.340],"rev_speed_stddev":0.0120,"stats_revolutions":4,
 "log_size":13,"log_capacity":2048,"log_overwritten":0,"last_seq":13,"uptime_s":99,
 "profile":{"id":1,"name":"Gym A"},
 "config":{"marks_count":3,"mark_distance_m":[0.940,0.940,0.940],"belt_length_m":2.820,
 "debounce_us":50000,"stop_timeout_us":3000000}}
```

- `speed_mps` is the revolution speed of the newest pulse, 0 when stopped.
- `phase_locked` is false during the first revolution of every run (after boot, reset
  or a stop); until it is true, `mark` in pulse records is run-relative (pulse index
  since the run started, modulo `marks_count`) and mark speed uses the mean mark
  distance rather than the per-mark one. It becomes true once the engine has picked
  which physical mark the run started on.
- `mark_share[m]` is the fraction of revolution time in the interval ending at mark
  `m`, averaged over the last 10 revolutions. Ideal is `1/marks_count`. Once
  `phase_locked` is true, index `m` names a physical mark (matching
  `config.mark_distance_m`); before that it is still run-relative.
- `rev_speed_stddev` is over the same 10 revolutions, in m/s.
- `profile` is the active profile's id and name.
- `config` is the engine configuration currently in effect: the active profile's
  per-mark distances (`mark_distance_m[k]` = distance from mark `k` to mark `k+1`,
  wrapping) and their sum `belt_length_m`, plus the fixed timing constants.

The diagnostic page's "Use measured distances" button (enabled once `phase_locked`
is true and `stats_revolutions >= 10`) proposes new per-mark distances from the
current mark shares: `d[k] = mark_share[(k+1) % marks] × belt_length_m`, then saves
them to the active profile with `POST /api/profiles/{id}`.

## Pulse record

```json
{"seq":42,"t_us":123456789,"interval_us":338400,"mark":2,"mark_speed_mps":2.778,"rev_speed_mps":2.700}
```

CSV columns are identical: `seq,t_us,interval_us,mark,mark_speed_mps,rev_speed_mps`.

- `t_us` wraps every ≈ 71.6 minutes; compute differences with unsigned 32-bit arithmetic.
- `interval_us` is 0 for the first pulse of a run (after boot, reset or a stop).
- `mark` is the pulse index since the last restart modulo `marks_count` until
  `phase_locked` is true in `/api/status`, after which it names the physical mark.

## `/api/signal`

```json
{"sample_rate_hz":2000,"min":812,"max":3390,"margin":2578,"samples":[...400 values...]}
```

Capture blocks the main loop for 200 ms; the ISR ring absorbs pulses meanwhile.

## Profile

A profile describes one treadmill: a name, a type, and the distance from each mark
to the next.

```json
{"id":0,"name":"Home treadmill","type":"belt","marks":3,
 "mark_distance_m":[0.940,0.940,0.940],"belt_length_m":2.820}
```

- `name`: 1–23 bytes, no `"` or `\`.
- `type`: `"belt"` or `"slats"` (slats is representable but not implemented).
- `marks`: 1–8.
- `mark_distance_m[k]`: distance from mark `k` to mark `k+1` (wrapping to mark 0
  after the last), 0.05–10 m.
- `belt_length_m`: the sum of `mark_distance_m`, computed, not stored.

`validate()` checks a profile before it is saved or applied and returns one of these
reasons verbatim in `{"error":"…"}`: `bad version`, `marks must be 1..8`,
`name empty`, `name too long`, `name has invalid characters`, `bad type`,
`distance out of range`.
