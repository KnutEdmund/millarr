#!/usr/bin/env python3
"""Reset the sensor's AP name and password to the config.h defaults over USB.

Equivalent to holding BOOT for 3 s after a reset, for boards without buttons:
the CP2102's RTS line resets the board and its DTR line pulls GPIO0 low.

Usage: $(brew --prefix platformio)/libexec/bin/python3 scripts/wifi_reset.py [--port PORT]
"""
import argparse
import time

import serial


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbserial-0001")
    args = ap.parse_args()

    s = serial.Serial(args.port, 115200, timeout=0.2)
    s.setDTR(False)
    s.setRTS(True)   # EN low: reset
    time.sleep(0.1)
    s.setRTS(False)  # run
    time.sleep(2.0)  # let setup() finish
    s.setDTR(True)   # GPIO0 low: "BOOT held"
    time.sleep(4.0)
    s.setDTR(False)
    t0 = time.time()
    buf = b""
    while time.time() - t0 < 5.0:
        buf += s.read(1024)
    s.close()
    text = buf.decode("utf-8", "replace")
    ok = "wifi settings reset" in text
    print("wifi settings reset: %s" % ("confirmed" if ok else "NOT seen (check the port and try again)"))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
