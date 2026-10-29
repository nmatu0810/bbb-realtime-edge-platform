#!/usr/bin/env python3
# Độ trễ cạnh lên INT → START I2C kế tiếp trên dây (file .sr fx2lafw, 2 MHz, SCL D0,
# SDA D1, INT D2 hoặc kênh cho ở đối số 2). START = SDA xuống khi SCL cao (không tính Sr:
# chỉ lấy START đầu tiên sau mỗi cạnh INT, trước cạnh INT kế). In min/trung vị/p99/max.
#   python3 scripts/la-int2start.py docs/int-i2c-2026-10-30.sr [kênh INT]
import sys, zipfile
f = sys.argv[1]
ich = int(sys.argv[2]) if len(sys.argv) > 2 else 2
z = zipfile.ZipFile(f)
names = sorted((n for n in z.namelist() if n.startswith('logic-1-')),
               key=lambda n: int(n.rsplit('-', 1)[1]))
d = b''.join(z.read(n) for n in names)
RATE = 2e6
im = 1 << ich
ints = [i for i in range(1, len(d)) if d[i] & im and not d[i - 1] & im]
starts = [i for i in range(1, len(d)) if d[i] & 1 and d[i - 1] & 1 and d[i - 1] & 2 and not d[i] & 2]
lat, miss, j = [], 0, 0
for k, e in enumerate(ints):
    nxt = ints[k + 1] if k + 1 < len(ints) else len(d)
    while j < len(starts) and starts[j] < e:
        j += 1
    if j < len(starts) and starts[j] < nxt:
        lat.append((starts[j] - e) / RATE * 1e6)
    else:
        miss += 1
lat.sort()
p = lambda q: lat[min(len(lat) - 1, int(len(lat) * q))]
print(f"{f}: {len(ints)} INT edges, {len(lat)} followed by START, {miss} without")
if lat:
    print(f"INT -> START: min {lat[0]:.1f}  med {p(0.5):.1f}  p99 {p(0.99):.1f}  max {lat[-1]:.1f} us")
