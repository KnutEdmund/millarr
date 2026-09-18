# HTTP API

Base URL: `http://192.168.4.1` (or `http://treadmill.local`) on the sensor's access point.

| Method | Path | Response |
|--------|------|----------|
| GET | `/` | Diagnostic page |
| GET | `/api/status` | JSON status (below) |
| GET | `/api/pulses.json?since=SEQ` | JSON array of records with `seq > SEQ`, max 500 |
| GET | `/api/pulses.csv` | Whole log as CSV download |
| GET | `/api/signal` | 200 ms ADC capture of the sensor's analog output |
| POST | `/api/reset` | Clears log, distance and counters; `{"ok":true}` |

## `/api/status`

```json
{"speed_mps":2.778,"speed_kmh":10.000,"running":true,"distance_m":123.4,
 "pulse_count":131,"revolution_count":43,"rejected_debounce":2,"isr_dropped":0,
 "mark_share":[0.333,0.334,0.333],"rev_speed_stddev":0.0120,"stats_revolutions":10,
 "log_size":131,"log_overwritten":0,"last_seq":131,"uptime_s":99,
 "config":{"belt_length_m":2.820,"marks_count":3,"debounce_us":50000,"stop_timeout_us":3000000}}
```

- `speed_mps` is the revolution speed of the newest pulse, 0 when stopped.
- `mark_share[m]` is the fraction of revolution time in the interval ending at mark `m`,
  averaged over the last 10 revolutions. Ideal is `1/marks_count`.
- `rev_speed_stddev` is over the same 10 revolutions, in m/s.

## Pulse record

```json
{"seq":42,"t_us":123456789,"interval_us":338400,"mark":2,"mark_speed_mps":2.778,"rev_speed_mps":2.700}
```

CSV columns are identical: `seq,t_us,interval_us,mark,mark_speed_mps,rev_speed_mps`.

- `t_us` wraps every ≈ 71.6 minutes; compute differences with unsigned 32-bit arithmetic.
- `interval_us` is 0 for the first pulse of a run (after boot, reset or a stop).
- `mark` is the pulse index since the last restart modulo `marks_count`.

Sending the whole log blocks the sensor's main loop for up to about a second; the interrupt ring absorbs roughly 60 pulses meanwhile, so download while the belt is stopped, or check `isr_dropped` in `/api/status` afterwards.

## `/api/signal`

```json
{"sample_rate_hz":2000,"min":812,"max":3390,"margin":2578,"samples":[...400 values...]}
```

Capture blocks the main loop for 200 ms; the ISR ring absorbs pulses meanwhile.
