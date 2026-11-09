#!/usr/bin/env python3
"""Hình cho báo cáo ĐACN2. Chạy từ gốc repo: python docs/report/figures.py <thư mục ra>

Sơ đồ vẽ bằng matplotlib; dạng sóng lấy thẳng từ file .sr của logic analyzer trong docs/;
đồ thị nghiêng và rút SDA lấy từ log demo trong docs/logs-2026-10-30/.
"""
import os
import re
import sys
import zipfile

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch

OUT = sys.argv[1] if len(sys.argv) > 1 else 'docs/report/fig'
os.makedirs(OUT, exist_ok=True)
plt.rcParams.update({'font.family': 'Liberation Serif', 'font.size': 11, 'savefig.dpi': 200})


def box(ax, x, y, w, h, text, fc='#ffffff', ec='#333333', fs=10, bold=False):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle='round,pad=0.02,rounding_size=0.08',
                                fc=fc, ec=ec, lw=1.1))
    ax.text(x + w / 2, y + h / 2, text, ha='center', va='center', fontsize=fs,
            fontweight='bold' if bold else 'normal', wrap=True)


def arrow(ax, x0, y0, x1, y1, text='', fs=9, style='-|>', off=(0.05, 0), ha='left'):
    ax.add_patch(FancyArrowPatch((x0, y0), (x1, y1), arrowstyle=style, mutation_scale=12,
                                 lw=1.1, color='#333333'))
    if text:
        ax.text((x0 + x1) / 2 + off[0], (y0 + y1) / 2 + off[1], text, fontsize=fs,
                ha=ha, va='center', color='#222222')


def canvas(w, h, xl, yl):
    fig, ax = plt.subplots(figsize=(w, h))
    ax.set_xlim(0, xl)
    ax.set_ylim(0, yl)
    ax.axis('off')
    return fig, ax


def save(fig, name):
    fig.savefig(os.path.join(OUT, name), bbox_inches='tight', facecolor='white')
    plt.close(fig)


# ---------------------------------------------------------------- kết nối phần cứng
def hardware():
    fig, ax = canvas(7.2, 3.6, 10, 5.4)
    box(ax, 0.2, 1.0, 3.8, 4.2, '', fc='#eef3fb')
    ax.text(2.1, 4.75, 'BeagleBone Black\n(AM3358, Cortex-A8 1 GHz)', ha='center', va='center',
            fontsize=10, fontweight='bold')
    box(ax, 6.0, 1.0, 3.8, 4.2, '', fc='#fbf3e8')
    ax.text(7.9, 4.75, 'Module MPU6050\n(chip họ MPU-6500)', ha='center', va='center',
            fontsize=10, fontweight='bold')
    pins = [('P9_3  (3.3 V)', 'VCC'), ('P9_1  (GND)', 'GND'), ('P9_19 (I2C2_SCL)', 'SCL'),
            ('P9_20 (I2C2_SDA)', 'SDA'), ('P9_12 (GPIO1_28)', 'INT'), ('P9_2  (GND)', 'AD0')]
    for i, (l, r) in enumerate(pins):
        y = 4.0 - i * 0.5
        ax.text(3.85, y, l, ha='right', va='center', fontsize=9, family='DejaVu Sans Mono')
        ax.text(6.15, y, r, ha='left', va='center', fontsize=9, family='DejaVu Sans Mono')
        ax.plot([3.95, 6.05], [y, y], color='#555555', lw=1)
    ax.text(2.1, 0.45, 'microSD: ảnh Yocto · UART0: console · Ethernet: SSH',
            ha='center', va='center', fontsize=9)
    ax.text(7.9, 0.45, 'I2C2 400 kHz, địa chỉ 0x68 · INT: xung 50 µs, cạnh lên',
            ha='center', va='center', fontsize=9)
    save(fig, 'hardware.png')


# ---------------------------------------------------------------- chuỗi khởi động
def bootflow():
    fig, ax = canvas(7.6, 3.6, 12.4, 6)
    steps = [('ROM code', 'trong SoC'), ('SPL (MLO)', 'thẻ SD'), ('U-Boot 2025.01', 'thẻ SD'),
             ('extlinux.conf', 'phân vùng boot'), ('Kernel 6.12 RT', 'zImage + dtb\n+ mpu6050.dtbo'),
             ('systemd', 'rootfs ext4'), ('udev', 'modalias →\nmpu6050.ko'),
             ('mpu6050-boot-check', 'selftest + đo')]
    for i, (a, b) in enumerate(steps):
        row, col = divmod(i, 4)
        x = 0.2 + col * 3.05
        y = 3.6 - row * 2.6
        box(ax, x, y, 2.6, 1.5, f'{a}\n\n{b}', fc='#eef3fb' if row == 0 else '#eef8ee', fs=8.5)
        if col < 3:
            arrow(ax, x + 2.62, y + 0.75, x + 3.03, y + 0.75)
    arrow(ax, 0.2 + 3 * 3.05 + 1.3, 3.58, 0.2 + 1.3, 2.52, style='-|>')
    save(fig, 'bootflow.png')


# ---------------------------------------------------------------- các tầng phần mềm
def layers():
    fig, ax = canvas(7.4, 4.4, 12, 7.4)
    rows = [
        ('Ứng dụng', 'mpu6050-test (poll + read)    mpu6050-boot-check (systemd)    sensord (GĐ2)', '#eef8ee'),
        ('Giao diện uapi', '/dev/mpu6050: read() bản ghi 16 B, poll(), ioctl()\nsysfs: sample_rate, accel_range, dropped', '#fff7e0'),
        ('Driver mpu6050.ko', 'i2c_driver + misc device · threaded IRQ · kfifo · watchdog · kref', '#fbf3e8'),
        ('Kernel', 'I2C core + i2c-omap · gpio-omap (IRQ) · PREEMPT_RT · udev/kmod', '#eef3fb'),
        ('Device tree', 'am335x-boneblack.dtb (-@) + mpu6050.dtbo:\n&i2c2 400 kHz, imu@68, INT gpio1 28', '#f3eefb'),
        ('Phần cứng', 'AM335x I2C2, GPIO1  ──  MPU-6500: FIFO 512 B, DATA_RDY', '#eeeeee'),
    ]
    for i, (name, desc, c) in enumerate(rows):
        y = 6.3 - i * 1.18
        box(ax, 0.2, y, 2.8, 0.9, name, fc=c, bold=True, fs=10)
        box(ax, 3.2, y, 8.6, 0.9, desc, fc=c, fs=9)
    save(fig, 'layers.png')


# ---------------------------------------------------------------- luồng dữ liệu
def dataflow():
    fig, ax = canvas(7.6, 4.6, 12.6, 7.6)
    box(ax, 0.2, 5.9, 3.2, 1.3, 'MPU-6500\nmẫu → FIFO 512 B\n(8 B: ax ay az temp)', fc='#eeeeee', fs=9)
    box(ax, 4.6, 5.9, 3.4, 1.3, 'Nửa trên mpu6050_irq\nts = ktime_get_ns()\n→ kfifo edges (128)', fc='#eef3fb', fs=9)
    box(ax, 9.2, 5.9, 3.2, 1.3, 'Thread IRQ\nSCHED_FIFO 50', fc='#eef3fb', fs=9, bold=True)
    box(ax, 4.6, 3.2, 7.8, 2.0,
        '1. chụp q cạnh   2. chờ tới 60 µs sau cạnh\n3. đọc FIFO_COUNT (lẻ / đầy → reset FIFO, dropped)\n'
        '4. ghép khung với cạnh, mỗi mẫu một ts\n5. đọc n khung trong một giao dịch I2C', fc='#f6f9fe', fs=9)
    box(ax, 4.6, 0.5, 3.4, 1.6, 'kfifo fifo\n256 bản ghi 16 B\n(một producer)', fc='#fbf3e8', fs=9)
    box(ax, 9.2, 0.5, 3.2, 1.6, 'read() / poll()\nngười dùng\n(read_lock)', fc='#eef8ee', fs=9)
    box(ax, 0.2, 0.5, 3.2, 1.6, 'sysfs, ioctl\ncấu hình (cfg_lock)\ndropped', fc='#fff7e0', fs=9)
    arrow(ax, 3.4, 6.55, 4.6, 6.55, 'xung INT', off=(0, 0.3), ha='center')
    arrow(ax, 8.0, 6.55, 9.2, 6.55, 'đủ batch', off=(0, 0.3), ha='center')
    arrow(ax, 10.8, 5.9, 10.8, 5.2)
    arrow(ax, 1.8, 5.9, 4.6, 4.2, 'I2C 400 kHz', off=(-0.5, -0.45), ha='right')
    arrow(ax, 6.3, 3.2, 6.3, 2.1, 'kfifo_put', off=(0.15, 0))
    arrow(ax, 8.0, 1.3, 9.2, 1.3, 'wake_up', off=(0, 0.3), ha='center')
    save(fig, 'dataflow.png')


# ---------------------------------------------------------------- dạng sóng từ .sr
def load_sr(path):
    z = zipfile.ZipFile(path)
    meta = z.read('metadata').decode()
    rate = float(re.search(r'samplerate=([\d.]+) (\w)Hz', meta).group(1)) * \
        {'k': 1e3, 'M': 1e6}[re.search(r'samplerate=[\d.]+ (\w)Hz', meta).group(1)]
    names = sorted((n for n in z.namelist() if n.startswith('logic-1-')),
                   key=lambda n: int(n.rsplit('-', 1)[1]))
    return b''.join(z.read(n) for n in names), rate


def wave(ax, d, rate, i0, i1, step=1):
    import numpy as np
    a = np.frombuffer(d[i0:i1:step], dtype=np.uint8)
    t = (np.arange(len(a)) * step) / rate * 1e3
    for k, (bit, name) in enumerate(((2, 'INT'), (1, 'SDA'), (0, 'SCL'))):
        y = (a >> bit) & 1
        ax.step(t, y * 0.7 + k * 1.1, where='post', lw=0.8, color=('#c0392b', '#1f5fa8', '#333333')[k])
        ax.text(-0.01 * t[-1], k * 1.1 + 0.35, name, ha='right', va='center', fontsize=9)
    ax.set_yticks([])
    ax.set_xlim(0, t[-1])
    for s in ('top', 'right', 'left'):
        ax.spines[s].set_visible(False)


def la_batch():
    d, rate = load_sr('docs/fifo-100hz-400k-2026-10-30.sr')
    # cạnh INT và lần SCL bận đầu sau cạnh thứ 10
    ints = [i for i in range(1, len(d)) if d[i] & 4 and not d[i - 1] & 4]
    e0 = ints[20]  # bỏ vài cạnh đầu bản ghi
    # tìm khối bus bận đầu tiên sau e0: SDA hoặc SCL xuống
    j = next(i for i in range(e0, len(d)) if not (d[i] & 3) == 3)
    k = j
    idle = 0
    while idle < int(rate * 200e-6):
        idle = idle + 1 if d[k] & 3 == 3 else 0
        k += 1
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(7.4, 3.8), gridspec_kw={'height_ratios': [1, 1]})
    s0 = max(0, j - int(rate * 0.105))
    wave(a1, d, rate, s0, k + int(rate * 0.004), step=20)
    a1.set_xlabel('ms (toàn cảnh: 100 Hz, lô 1 mẫu, mỗi xung INT một lần đọc)')
    wave(a2, d, rate, j - int(rate * 150e-6), k + int(rate * 50e-6))
    a2.set_xlabel('ms (phóng to: ghi 0x72, đọc FIFO_COUNT 2 byte; ghi 0x74, đọc 1 khung 8 byte)')
    fig.tight_layout()
    save(fig, 'la-batch-100hz.png')


def la_nack():
    d, rate = load_sr('docs/int-nack-nonrt-2026-10-30.sr')
    # tìm cạnh INT mà SDA ở mức cao tại bit thứ 9 của byte thanh ghi (NACK): giải mã đơn giản như la-fifo.py
    tx, cur, bits = [], None, []
    for i in range(1, len(d)):
        c, p = d[i], d[i - 1]
        if c & 1 and p & 1:
            if p & 2 and not c & 2:
                cur = cur or {'start': i, 'nack': []}
                bits = []
            elif not p & 2 and c & 2 and cur is not None:
                cur['end'] = i
                tx.append(cur)
                cur, bits = None, []
        if c & 1 and not p & 1 and cur is not None:
            bits.append((c >> 1) & 1)
            if len(bits) == 9:
                cur['nack'].append(bits[8])
                bits = []
    bad = [t for t in tx if len(t['nack']) >= 2 and t['nack'][1] == 1]
    t = bad[0]
    ints = [i for i in range(1, t['start']) if d[i] & 4 and not d[i - 1] & 4]
    e = ints[-1]
    fig, ax = plt.subplots(figsize=(7.4, 1.9))
    wave(ax, d, rate, e - int(rate * 10e-6), t['end'] + int(rate * 20e-6))
    ax.set_xlabel('ms tính từ 10 µs trước cạnh INT (non-RT: START sau cạnh ~21 µs, chip NACK byte 0x72)')
    fig.tight_layout()
    save(fig, 'la-nack.png')
    return (t['start'] - e) / rate * 1e6


# ---------------------------------------------------------------- đồ thị từ log demo
def tilt():
    rows = []
    for line in open('docs/logs-2026-10-30/demo-tilt.txt'):
        m = re.search(r'ax ([+-][\d.]+) ay ([+-][\d.]+) az ([+-][\d.]+) g', line)
        if m:
            rows.append([float(x) for x in m.groups()])
    fig, ax = plt.subplots(figsize=(7.2, 2.6))
    for k, n in enumerate(('ax', 'ay', 'az')):
        ax.plot(range(1, len(rows) + 1), [r[k] for r in rows], marker='o', ms=2.5, lw=1, label=n)
    ax.set_xlabel('giây (mỗi điểm: mẫu cuối của giây đó, 100 Hz)')
    ax.set_ylabel('g')
    ax.grid(alpha=0.3)
    ax.legend(ncol=3, fontsize=9, loc='upper right')
    fig.tight_layout()
    save(fig, 'demo-tilt.png')


def sda_pull():
    rate, drop, gap, sec = [], [], [], 0
    for line in open('docs/logs-2026-10-30/demo-sda-pull.txt'):
        m = re.search(r'^\s*(\d+)/s .* dropped \+(\d+)', line)
        if m:
            sec += 1
            rate.append((sec, int(m.group(1))))
            drop.append((sec, int(m.group(2))))
        elif 'without data' in line:
            sec += 1
            gap.append(sec)
    fig, a1 = plt.subplots(figsize=(7.2, 2.6))
    for s in gap:
        a1.axvspan(s - 0.5, s + 0.5, color='#dddddd', zorder=0, lw=0)
    a1.bar([s for s, _ in rate], [r for _, r in rate], color='#9bb7d9', label='bản ghi/s')
    a1.set_ylabel('bản ghi/s')
    a1.set_xlabel('giây kể từ khi app chạy (vùng xám: SDA bị rút, app báo "1 s without data")')
    a2 = a1.twinx()
    a2.plot([s for s, _ in drop], [v for _, v in drop], 'o', ms=3, color='#c0392b',
            label='dropped (app in mỗi giây có dữ liệu)')
    a2.set_ylabel('dropped (tích lũy)')
    fig.legend(loc='upper left', bbox_to_anchor=(0.09, 0.97), fontsize=8.5)
    fig.tight_layout()
    save(fig, 'demo-sda-pull.png')


# ---------------------------------------------------------------- ảnh PulseView, cắt phần trống
def crop(src, name, boxes):
    """boxes: các dải (y0, y1) giữ lại, ghép dọc (thước thời gian + tín hiệu/giải mã)."""
    from PIL import Image
    im = Image.open(src).convert('RGB')
    parts = [im.crop((0, y0, im.width, y1)) for y0, y1 in boxes]
    out = Image.new('RGB', (im.width, sum(p.height for p in parts)))
    y = 0
    for p in parts:
        out.paste(p, (0, y))
        y += p.height
    out.save(os.path.join(OUT, name))


if __name__ == '__main__':
    hardware()
    bootflow()
    layers()
    dataflow()
    la_batch()
    us = la_nack()
    tilt()
    sda_pull()
    crop('docs/i2c-whoami-2026-10-22.png', 'la-whoami.png', [(90, 125), (490, 680)])
    crop('docs/i2c-burst-2026-10-23.png', 'la-burst.png', [(95, 130), (365, 550)])
    print('cạnh INT → START của giao dịch NACK: %.1f µs' % us)
    print(sorted(os.listdir(OUT)))
