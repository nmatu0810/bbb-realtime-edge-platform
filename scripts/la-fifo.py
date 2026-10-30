#!/usr/bin/env python3
"""Đọc FIFO trên dây (tuần 7) so với bản ghi /dev/mpu6050 của cùng lần chạy.

    python3 scripts/la-fifo.py x.sr rec.bin [kênh INT, mặc định 2] [tần số mẫu LA, mặc định 2e6]

LA: SCL D0, SDA D1, INT D2. rec.bin: bản ghi 16 byte lấy bằng dd trong lúc chụp.
In: số cạnh INT, giao dịch đọc FIFO_COUNT (0x72) và giá trị count, giao dịch đọc FIFO_R_W
(0x74) và số khung. Mỗi khung ghép với bản ghi trùng 8 byte dữ liệu; cạnh của khung là
cạnh INT cuối cùng trước START của lần đọc count ngay trước đó, lùi dần cho các khung cũ hơn
trong lô. Lệch = ts_ns − thời điểm cạnh (trừ đi trung vị): gán đúng cạnh thì chỉ dao động
trong khoảng độ trễ cạnh → nửa trên, sai một cạnh thì nhảy cả chu kỳ.
"""
import struct
import sys
import zipfile

f, recf = sys.argv[1], sys.argv[2]
ich = int(sys.argv[3]) if len(sys.argv) > 3 else 2
RATE = float(sys.argv[4]) if len(sys.argv) > 4 else 2e6
z = zipfile.ZipFile(f)
names = sorted((n for n in z.namelist() if n.startswith('logic-1-')),
               key=lambda n: int(n.rsplit('-', 1)[1]))
d = b''.join(z.read(n) for n in names)

im = 1 << ich
ints = [i for i in range(1, len(d)) if d[i] & im and not d[i - 1] & im]
tx, cur, bits = [], None, []
for i in range(1, len(d)):
    c, p = d[i], d[i - 1]
    if c & 1 and p & 1:
        if p & 2 and not c & 2:
            if cur is None:
                cur = {'start': i, 'bytes': [], 'nack': []}
            bits = []
        elif not p & 2 and c & 2 and cur is not None:
            tx.append(cur)
            cur, bits = None, []
    if c & 1 and not p & 1 and cur is not None:
        bits.append((c >> 1) & 1)
        if len(bits) == 9:
            v = 0
            for b in bits[:8]:
                v = v << 1 | b
            cur['bytes'].append(v)
            cur['nack'].append(bits[8])
            bits = []

counts, reads, nacks = [], [], 0
for t in tx:
    b = t['bytes']
    if any(t['nack'][:3]) and len(b) >= 3:
        nacks += 1
    if len(b) == 5 and b[:3] == [0xd0, 0x72, 0xd1]:
        counts.append((t['start'], b[3] << 8 | b[4]))
    elif len(b) >= 11 and b[:3] == [0xd0, 0x74, 0xd1]:
        reads.append((t['start'], b[3:]))

recs = open(recf, 'rb').read()
rk = []
for i in range(0, len(recs) - 15, 16):
    ax, ay, az, tp, ts = struct.unpack_from('<4hQ', recs, i)
    rk.append((struct.pack('>4h', ax, ay, az, tp), ts))

hist = {}
for _, c in counts:
    hist[c] = hist.get(c, 0) + 1
print(f'{f}: {len(d) / RATE:.2f} s, {len(ints)} INT edges, {len(tx)} transactions '
      f'(NACK in header: {nacks}), {len(counts)} FIFO_COUNT reads, {len(reads)} FIFO reads')
print('FIFO_COUNT values (bytes: times):',
      ' '.join(f'{k}:{v}' for k, v in sorted(hist.items())))

# Ghép theo thứ tự: bản ghi trùng 8 byte có thật (1000 Hz, đứng yên), nên tìm khung kế
# tiếp trong một cửa sổ ngắn sau bản ghi vừa ghép, không tra theo khóa.
offs, unmatched, frames, ci, ei, ri = [], 0, 0, 0, 0, -1
for start, data in reads:
    while ci + 1 < len(counts) and counts[ci + 1][0] < start:
        ci += 1
    if not counts or counts[ci][0] > start:
        continue
    cstart = counts[ci][0]
    while ei + 1 < len(ints) and ints[ei + 1] < cstart:
        ei += 1
    n = len(data) // 8
    for k in range(n):
        frames += 1
        key = bytes(data[8 * k:8 * k + 8])
        e = ei - (n - 1 - k)
        lo = 0 if ri < 0 else ri + 1
        hi = len(rk) if ri < 0 else min(len(rk), ri + 200)
        m = next((j for j in range(lo, hi) if rk[j][0] == key), None)
        if m is None or e < 0 or ints[e] > cstart:
            unmatched += 1
            continue
        ri = m
        offs.append(rk[m][1] / 1e3 - ints[e] / RATE * 1e6)
print(f'{frames} frames on the wire, {len(offs)} matched to records, {unmatched} not')
if offs:
    s = sorted(offs)
    med = s[len(s) // 2]
    r = sorted(o - med for o in offs)
    print(f'ts - edge, minus median: min {r[0]:.0f}  p1 {r[len(r) // 100]:.0f}  '
          f'p99 {r[len(r) * 99 // 100]:.0f}  max {r[-1]:.0f} us')
