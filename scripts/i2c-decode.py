#!/usr/bin/env python3
# Giải mã I2C từ file .sr của PulseView (fx2lafw, unitsize 1, SCL = D0, SDA = D1,
# 2 MHz), độc lập với decoder của PulseView. In mỗi giao dịch trọn (S..P): thời điểm,
# khoảng cách tới giao dịch trước, độ dài, số cạnh lên SCL, các byte ((N) = NACK).
# Giao dịch bị cắt ở đầu file (thiếu START) bị bỏ.
#   python3 scripts/i2c-decode.py docs/i2c-stream-2026-10-29.sr
import sys, zipfile
f = sys.argv[1]
z = zipfile.ZipFile(f)
# PulseView ghi một khối logic-1-1, sigrok-cli chia khối 20480 mẫu: nối theo số thứ tự.
names = sorted((n for n in z.namelist() if n.startswith('logic-1-')),
               key=lambda n: int(n.rsplit('-', 1)[1]))
d = b''.join(z.read(n) for n in names)
RATE = 2e6
scl = [b & 1 for b in d]; sda = [(b >> 1) & 1 for b in d]
tx = []; cur = None; bits = []; edges = 0
for i in range(1, len(d)):
    if scl[i] and scl[i-1]:
        if sda[i-1] and not sda[i]:  # START / Sr
            if cur is not None:
                cur['ev'].append(('Sr', i))
            else:
                cur = {'start': i, 'ev': [('S', i)], 'bytes': [], 'edges': 0}
            bits = []
        elif not sda[i-1] and sda[i] and cur is not None:  # STOP
            cur['ev'].append(('P', i)); cur['end'] = i; tx.append(cur); cur = None; bits = []
    if scl[i] and not scl[i-1] and cur is not None:
        cur['edges'] += 1
        bits.append(sda[i])
        if len(bits) == 9:
            v = 0
            for b in bits[:8]: v = v << 1 | b
            cur['bytes'].append((v, 'N' if bits[8] else 'A')); bits = []
first_scl_fall = next(i for i in range(1, len(d)) if scl[i-1] and not scl[i])
print(f"{f}: {len(tx)} complete transactions, first SCL fall at {first_scl_fall/RATE*1e6:.1f} us")
prev = None
for t in tx:
    by = ' '.join(f"{v:02x}{'' if a=='A' else '(N)'}" for v, a in t['bytes'])
    dur = (t['end'] - t['start']) / RATE * 1e6
    gap = '' if prev is None else f" +{(t['start']-prev)/RATE*1e3:.2f} ms"
    print(f"{t['start']/RATE*1e3:8.3f} ms{gap:>11}  {dur:7.1f} us {t['edges']:3d} edges  {by}")
    prev = t['start']
