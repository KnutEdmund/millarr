#!/usr/bin/env python3
"""Reset the ESP32 via RTS and capture serial output for a few seconds.

Usage: python3 scripts/serial_capture.py [--port PORT] [--seconds N] [--no-reset]
Run with PlatformIO's Python, which bundles pyserial:
  /opt/homebrew/Cellar/platformio/6.2.0/libexec/bin/python3 scripts/serial_capture.py
"""
import argparse
import sys
import time

import serial


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbserial-0001")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=5.0)
    ap.add_argument("--no-reset", action="store_true")
    args = ap.parse_args()

    s = serial.Serial(args.port, args.baud, timeout=0.2)
    if not args.no_reset:
        s.setDTR(False)
        s.setRTS(True)
        time.sleep(0.1)
        s.setRTS(False)
    t0 = time.time()
    buf = b""
    while time.time() - t0 < args.seconds:
        buf += s.read(1024)
    s.close()
    sys.stdout.write(buf.decode("utf-8", "replace"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
