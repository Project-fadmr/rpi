#!/usr/bin/env python3
"""Receive check for espnow0; pair with test/esp32_sender.
Prints each frame and the loss rate per sender from the message numbers.
Usage: espnow_rx.py [seconds]   (default 30)"""
import re
import socket
import sys
import time

ETH_P_ESPNOW = 0x88B5
secs = float(sys.argv[1]) if len(sys.argv) > 1 else 30

s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(ETH_P_ESPNOW))
s.bind(("espnow0", ETH_P_ESPNOW))
s.settimeout(1.0)

seen = {}  # sender -> set of message numbers
big_ok = big_bad = 0
end = time.monotonic() + secs
while time.monotonic() < end:
    try:
        frame = s.recv(4096)
    except socket.timeout:
        continue
    src, payload = frame[6:12].hex(":"), frame[14:]
    m = re.match(rb"(hello|big) (\d+)", payload)
    print(f"{src} -> {frame[0:6].hex(':')}  {len(payload):4d} B  {payload[:24]!r}")
    if m:
        seen.setdefault(src, set()).add(int(m[2]))
        if m[1] == b"big":
            if len(payload) == 1000:
                big_ok += 1
            else:
                big_bad += 1

for src, nums in seen.items():
    span = max(nums) - min(nums) + 1
    print(f"{src}: received {len(nums)}/{span} ({100 * len(nums) / span:.0f}%)")
print(f"v2.0 1000-byte frames: {big_ok} whole, {big_bad} truncated")
sys.exit(0 if seen and not big_bad else 1)
