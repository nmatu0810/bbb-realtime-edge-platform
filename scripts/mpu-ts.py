#!/usr/bin/env python3
"""Phân tích file bản ghi /dev/mpu6050 (16 byte: s16 ax ay az temp, u64 ts_ns, little-endian).

    scripts/bo.sh 'dd if=/dev/mpu6050 bs=16 count=3000 2>/dev/null' > rec.bin
    python3 scripts/mpu-ts.py rec.bin [chu_kỳ_ms] [g_range]

In khoảng cách ts liền nhau (so với chu kỳ, mặc định 10 ms), số lần ts không tăng, khoảng lệch
hơn nửa chu kỳ, vài khoảng đầu (mẫu dư lúc start), trường temp (°C theo MPU-6500) và |a| (g):
FIFO lệch byte thì temp và |a| nhảy lung tung.
"""
import math
import struct
import sys

data = open(sys.argv[1], 'rb').read()
period = float(sys.argv[2]) * 1e6 if len(sys.argv) > 2 else 10e6
lsb = 16384 / (int(sys.argv[3]) / 2) if len(sys.argv) > 3 else 16384
recs = [struct.unpack_from('<4hQ', data, i) for i in range(0, len(data) - 15, 16)]
if len(recs) < 2:
    sys.exit('fewer than 2 records')

ts = [r[4] for r in recs]
d = [b - a for a, b in zip(ts, ts[1:])]
ds = sorted(d)


def q(p):
    return ds[min(len(ds) - 1, int(p * len(ds)))] / 1e6


print(f'{len(recs)} records, {(ts[-1] - ts[0]) / 1e9:.3f} s, '
      f'{(len(recs) - 1) / ((ts[-1] - ts[0]) / 1e9):.3f} rec/s by ts')
print(f'dt ms: min {ds[0] / 1e6:.3f}  p1 {q(0.01):.3f}  median {q(0.5):.4f}  '
      f'p99 {q(0.99):.3f}  max {ds[-1] / 1e6:.3f}  mean {sum(d) / len(d) / 1e6:.4f}')
bad = [(i, x) for i, x in enumerate(d) if abs(x - period) > period / 2]
print(f'non-increasing {sum(1 for x in d if x <= 0)}, |dt - T| > T/2: {len(bad)}')
for i, x in bad[:10]:
    print(f'  #{i + 1}: dt {x / 1e6:.3f} ms')
print('first dt ms:', ' '.join(f'{x / 1e6:.3f}' for x in d[:6]))
t = [r[3] / 333.87 + 21 for r in recs]
a = [math.sqrt(r[0] ** 2 + r[1] ** 2 + r[2] ** 2) / lsb for r in recs]
print(f'temp C {min(t):.2f}..{max(t):.2f}, |a| g {min(a):.3f}..{max(a):.3f}, '
      f'temp outside 15..60 C (byte-shifted frame): {sum(1 for x in t if not 15 < x < 60)}')
