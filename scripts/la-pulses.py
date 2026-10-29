#!/usr/bin/env python3
# Thống kê từng kênh của file .sr (fx2lafw, unitsize 1, 2 MHz): mức đầu, số cạnh lên,
# độ rộng mức cao và khoảng giữa hai cạnh lên (min/trung vị/max). Dùng để tìm kênh INT
# và đo xung DATA_RDY; kênh không đổi mức in "idle".
#   python3 scripts/la-pulses.py docs/int-pulse-2026-10-30.sr [kênh ...]
import sys, zipfile
f = sys.argv[1]
chans = [int(c) for c in sys.argv[2:]] or range(8)
z = zipfile.ZipFile(f)
# PulseView ghi một khối logic-1-1, sigrok-cli chia khối 20480 mẫu: nối theo số thứ tự.
names = sorted((n for n in z.namelist() if n.startswith('logic-1-')),
               key=lambda n: int(n.rsplit('-', 1)[1]))
d = b''.join(z.read(n) for n in names)
RATE = 2e6

def stat(xs, unit, scale):
    if not xs:
        return '-'
    s = sorted(xs)
    return f"{s[0]*scale:.2f}/{s[len(s)//2]*scale:.2f}/{s[-1]*scale:.2f} {unit}"

print(f"{f}: {len(d)} samples, {len(d)/RATE:.3f} s")
for c in chans:
    m = 1 << c
    rises, falls = [], []
    prev = d[0] & m
    for i in range(1, len(d)):
        v = d[i] & m
        if v != prev:
            (rises if v else falls).append(i)
            prev = v
    if not rises and not falls:
        print(f"D{c}: idle {'high' if d[0] & m else 'low'}")
        continue
    widths = []
    for r in rises:
        fl = next((x for x in falls if x > r), None)
        if fl is not None:
            widths.append(fl - r)
    periods = [b - a for a, b in zip(rises, rises[1:])]
    print(f"D{c}: start {'high' if d[0] & m else 'low'}, {len(rises)} rising, "
          f"high {stat(widths, 'us', 1e6/RATE)}, period {stat(periods, 'ms', 1e3/RATE)}")
