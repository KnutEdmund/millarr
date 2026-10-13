# HTTP API

Base URL: `http://192.168.4.1` (or `http://treadmill.local`) on the sensor's access point.

The access point answers every DNS query with its own address (captive portal).
A request whose `Host` is neither the sensor's IP nor `treadmill.local`, such as an
OS connectivity probe, gets `302` to `http://192.168.4.1/`; unknown paths on the
sensor itself get `404`.

| Method | Path | Response |
|--------|------|----------|
| GET | `/` | Diagnostic page |
| GET | `/settings` | Settings page |
| GET | `/api/status` | JSON status (below) |
| GET | `/api/pulses.json?since=SEQ` | JSON array of records with `seq > SEQ`, max 500 |
| GET | `/api/pulses.csv` | Whole log as CSV download |
| GET | `/api/signal` | 200 ms ADC capture of the sensor's analog output |
| POST | `/api/reset` | Clears log, distance and counters and starts a new session file; `{"ok":true}` |
| GET | `/api/sessions` | Session files on flash (below) |
| GET | `/api/sessions/{id}.csv` | One session file as a CSV download |
| POST | `/api/sessions/{id}/delete` | Delete a closed session file; `{"ok":true}` |
| GET | `/api/profiles` | All stored profiles, the active id and the maximum slot count |
| POST | `/api/profiles/{id}` | Create or replace profile `id` (0–19); form fields `name`, `type` (`belt`/`slats`), `marks` (1–8), `d0`…`d{marks-1}` (metres), optional `notes`; for slats also `slat_count` and `slat_pitch` (metres), and the distances may be omitted |
| POST | `/api/profiles/{id}/activate` | Make `id` the active profile; the engine is reconfigured immediately |
| POST | `/api/profiles/{id}/delete` | Delete `id`; refused for the active profile and for the last remaining one |
| GET | `/api/settings` | `{"ap_ssid":"…","mdns":"treadmill"}` |
| POST | `/api/settings/wifi` | Form fields `ssid` (1–32), `password` (8–63); the access point restarts about 1 s after the response |

Note on `/api/pulses.csv`: sending the whole log blocks the sensor's main loop for up
to about a second; the interrupt ring absorbs roughly 60 pulses meanwhile, so
download while the belt is stopped, or check `isr_dropped` in `/api/status` afterwards.
It only covers the RAM ring (last 2048 records); complete sessions come from the
session log below. Both exports start with the same `#` metadata lines.

## Session files

Every accepted pulse is also appended to `/log/NNNNNN.csv` on the LittleFS
partition (1.9 MB; about 115 KB per hour at one mark and 10 km/h, so roughly 16 hours
of the newest sessions are kept). A session is one `seq` space: a new file starts at
boot and at `POST /api/reset`; the file is created on the first flush, so idle boots
leave nothing behind. Writes happen from the main loop in batches of 16 records or
every 5 s, never from the pulse path. When the partition is short of space or holds
more than 32 sessions, the oldest closed sessions are deleted first.

A session file starts with `#` comment lines, then the same CSV as `/api/pulses.csv`:

```
# millarr session 123
# firmware a1b2c3d boot_uptime_s 12
# profile 0 "Slat mill" type=slats marks=1 belt_length_m=3.506 mark_distance_m=[3.506] slat_count=47 slat_pitch_m=0.07460
# t_us wraps every 4294967296 us: add 2^32 whenever it decreases. interval_us=0 marks a run restart.
seq,t_us,interval_us,mark,mark_speed_mps,rev_speed_mps
```

`GET /api/sessions` lists the files, ascending by id:

```json
{"flash_ok":true,"full":false,"total":1441792,"used":301056,"active":12,
 "sessions":[{"id":11,"bytes":230400,"active":false},{"id":12,"bytes":69040,"active":true,"records":1726}]}
```

`records` is exact for the active session only; for closed files estimate
`(bytes − header) / 40`. `GET /api/sessions/{id}.csv` streams the file with a known
`Content-Length` (filename `treadmill-session-000012.csv`); the active session is
flushed first. The main loop is blocked while streaming (5–20 s for a full 1.9 MB
file), but pulses are drained into the engine between 2 KB chunks, so nothing is
dropped; flash writes are held while the active file is read, so prefer downloading
closed sessions during a run. Errors: `bad session id` (not 1–9 digits),
`no such session` (404), `open failed` (500). `POST /api/sessions/{id}/delete`:
`bad session id`, `cannot delete active session`, `no such session` (404),
`delete failed` (500).

Activating another profile mid-session appends a `# profile … at_uptime_s=N` line at
that point instead of starting a new file. Session ids come from an NVS counter and
never repeat; gaps are normal (an id is consumed at every boot). The sensor has no
clock, so the download filename carries the date the browser saw.
`pio run -t erase` wipes the session log together with the profiles.

## Writes

`POST` bodies are `application/x-www-form-urlencoded`, not JSON — the firmware has no
JSON parser. On success the response is `{"ok":true}` (`/api/settings/wifi` adds
`"restarting_ap":true`). On failure the response is `400` or `500` with
`{"error":"reason"}`; the reason strings are listed under each endpoint below.

`POST /api/profiles/{id}` reconfigures the running engine immediately only when `{id}`
is the currently active profile; saving a different slot just writes it to NVS.

Errors from `/api/profiles/{id}`, in the order they are checked: `bad profile id`
(id isn't `0`–`19`), `name too long`, `notes too long` (`notes`, if given, is
≥ 127 bytes), `bad type` (not `belt` or `slats`), `marks must be 1..8`,
`missing slat count` / `missing slat pitch` (slats only), `missing distance` (a
`d0`…`d{marks-1}` field is absent; for slats the distances are optional and default
to an equal split of `slat_count × slat_pitch`), any `validate()` reason below
(including `notes has invalid characters`), then `save failed` (`500`, NVS write
failed).
`/api/profiles/{id}/activate`: `bad profile id`, `empty profile` (no profile stored
in that slot). `/api/profiles/{id}/delete`: `bad profile id`,
`cannot delete active profile`, `empty profile`, `at least one profile` (refuses to
delete the last remaining profile), then `erase failed` (`500`, NVS write failed).
`/api/settings/wifi`: `ssid must be 1-32 printable characters`,
`password must be 8-63 printable characters`.

## `/api/status`

```json
{"speed_mps":2.500,"speed_kmh":9.000,"running":true,"phase_locked":true,"distance_m":12.340,
 "pulse_count":13,"revolution_count":4,"rejected_debounce":1,"rejected_implausible":0,"isr_dropped":2,
 "mark_share":[0.330,0.330,0.340],"rev_speed_stddev":0.0120,"stats_revolutions":4,
 "log_size":13,"log_capacity":2048,"log_overwritten":0,"last_seq":13,"uptime_s":99,
 "flash":{"ok":true,"full":false,"total":1441792,"used":301056,"session_id":12,
 "session_records":1726,"session_bytes":69040,"write_errors":0,"sessions":3},
 "ble":{"advertising":false,"connected":true,"connects":2,"notifications":1234,"last_speed_mps":2.500},
 "profile":{"id":1,"name":"Gym A"},
 "config":{"marks_count":3,"mark_distance_m":[0.940,0.940,0.940],"belt_length_m":2.820,
 "debounce_us":50000,"stop_timeout_us":7206000,"max_speed_mps":8.000}}
```

- `speed_mps` is the revolution speed of the newest pulse, 0 when stopped.
- `phase_locked` is false during the first revolution of every run (after boot, reset
  or a stop); until it is true, `mark` in pulse records is run-relative (pulse index
  since the run started, modulo `marks_count`) and mark speed uses the mean mark
  distance rather than the per-mark one. It becomes true once the engine has picked
  which physical mark the run started on.
- `mark_share[m]` is the fraction of revolution time in the interval ending at mark
  `m`, averaged over the last 10 revolutions. Ideal is `1/marks_count`. Index `m`
  names a physical mark (matching `config.mark_distance_m`): shares are only
  accumulated once the phase is locked.
- `rev_speed_stddev` is over the same 10 revolutions, in m/s.
- `rejected_debounce` counts pulses closer than `debounce_us` to the previous one (ISR
  and engine rules combined). `rejected_implausible` counts edges that came closer to
  another edge than `config.max_speed_mps` allows over the shortest mark distance. Of
  such a pair one edge is kept and the other dropped before it reaches the log, the
  statistics window and the distance sum: the one whose interval fits the current
  speed, or the first one when there is no speed yet. Each pulse is therefore held
  back for that minimum gap (about 0.45 s on a 3.6 m one-mark belt) before it is
  logged. A single rejected pulse at start-up is normal (sensor settling); a growing
  count during a run means a mark is double-triggering or something else on the belt
  reflects.
- `log_size` / `log_capacity` / `log_overwritten` describe the RAM ring that feeds
  `/api/pulses.json` and `/api/pulses.csv` (2048 records, about 44 minutes at one
  mark and 10 km/h). The flash session log below is not limited by it.
- `flash` is the session log on the LittleFS partition: `ok` false means the
  filesystem could not be mounted and nothing is written; `full` means even
  evicting closed sessions left no room, so records are being dropped
  (`write_errors` counts dropped batches and failed writes). `session_id` is the
  current session, `session_records` the records queued or written for it, and
  `session_bytes` its size on flash (updated at each flush, every 16 records or 5 s).
  `sessions` is the number of session files on flash.
- `ble` is the Bluetooth LE foot pod (RSC service). `advertising` and `connected`
  are exclusive: the sensor stops advertising while a watch or phone is connected
  and resumes when it disconnects. `connects` counts connections since boot,
  `notifications` the RSC measurements sent (one per second while connected), and
  `last_speed_mps` the speed in the most recent one. None of these are reset by
  `POST /api/reset`.
- `profile` is the active profile's id and name.
- `config` is the engine configuration currently in effect: the active profile's
  per-mark distances (`mark_distance_m[k]` = distance from mark `k` to mark `k+1`,
  wrapping) and their sum `belt_length_m`, plus the timing constants. `debounce_us`
  and `max_speed_mps` are compile-time constants. `stop_timeout_us` is derived from
  the profile: `longest mark distance / MIN_SPEED_MPS` (0.5 m/s), clamped to 3–15 s,
  so the slowest measurable speed is 0.5 m/s (1.8 km/h) unless the 3 s floor applies
  (mark distance under 1.5 m, where it is `distance / 3 s`). A 1-mark 3.603 m belt
  gets 7.2 s; the default 3-mark 2.82 m belt stays at 3 s.

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

A profile describes one treadmill: a name, freeform notes, a type, the distance
from each mark to the next and, for slat belts, the slat count and pitch. Profile
version 3. Stored v2 blobs (212 bytes) are upgraded on load with zeroed slat fields;
60-byte v1 blobs are treated as empty slots (see `LEDGER.md`).

```json
{"id":0,"name":"Home treadmill","notes":"","type":"belt","marks":3,
 "mark_distance_m":[0.940,0.940,0.940],"slat_count":0,"slat_pitch_m":0.00000,"belt_length_m":2.820}
{"id":4,"name":"Slat mill","notes":"","type":"slats","marks":1,
 "mark_distance_m":[3.506],"slat_count":47,"slat_pitch_m":0.07460,"belt_length_m":3.506}
```

`GET /api/profiles` wraps a list of these under `{"active":N,"max":20,"profiles":[…]}`;
`max` is the number of usable slots (`MAX_PROFILES`, ids 0–19).

- `name`: 1–47 bytes, no `"` or `\`.
- `notes`: 0–127 bytes, optional, no `"` or `\`; newlines (`\n`) are allowed and are
  escaped as `\n` (two characters) in the JSON string.
- `type`: `"belt"` (tape-measured belt with painted marks) or `"slats"` (slat belt;
  the sensor still reads painted marks, but the belt length is `slat_count ×
  slat_pitch_m`, the chain's neutral line, instead of a tape measurement).
- `marks`: 1–8.
- `mark_distance_m[k]`: distance from mark `k` to mark `k+1` (wrapping to mark 0
  after the last), 0.05–10 m. This is what the engine uses for both types.
- `slat_count` (2–1000) and `slat_pitch_m` (0.005–0.5 m): slats only, 0 for belts.
  For slats the distances must sum to `slat_count × slat_pitch_m` within 0.5 %;
  calibration ("Use measured distances") redistributes them but keeps the sum.
- `belt_length_m`: the sum of `mark_distance_m`, computed, not stored.

`validate()` checks a profile before it is saved or applied and returns one of these
reasons verbatim in `{"error":"…"}`: `bad version`, `marks must be 1..8`,
`name empty`, `name too long`, `name has invalid characters`, `notes too long`,
`notes has invalid characters`, `bad type`, `slat count must be 2..1000`,
`slat pitch out of range`, `distance out of range`, `distances do not match slats`.

## Backup file

The Settings page's Backup section downloads all stored profiles as a JSON file and
can restore them from one, entirely in the browser — there is no backup endpoint.
Restore replays the normal `POST /api/profiles/{id}` and
`POST /api/profiles/{id}/activate` writes above, one per profile in file order.

```json
{
  "format": "millarr-profiles",
  "version": 2,
  "exported_at": "2026-09-26T12:34:56Z",
  "active": 0,
  "profiles": [
    {"id":0,"name":"Home treadmill","notes":"","type":"belt","marks":3,"mark_distance_m":[0.940,0.940,0.940],"slat_count":0,"slat_pitch_m":0},
    {"id":4,"name":"Slat mill","notes":"","type":"slats","marks":1,"mark_distance_m":[3.506],"slat_count":47,"slat_pitch_m":0.0746}
  ]
}
```

`belt_length_m` is omitted (it's derived from `mark_distance_m`). Wi-Fi settings are
never included. Version 1 files (without the slat fields) are still accepted on
restore; a version 1 slats profile needs `slat_count` and `slat_pitch_m` to be added
by hand before it can be restored.
