# Kiến trúc GĐ1: BeagleBone Black + MPU6050

Bản tóm tắt cho báo cáo đồ án chuyên ngành. Chi tiết từng quyết định và số đo gốc ở
`docs/PROGRESS.md` (mỗi tuần một mục), kế hoạch và hợp đồng ở `docs/ROADMAP.md`, chuỗi
boot ở `docs/boot-flow.md`.

## Các tầng

```
 phần cứng     MPU-6500 (WHO_AM_I 0x70) ── I2C2 400 kHz (P9_19 SCL, P9_20 SDA) ── AM335x
               INT ── P9_12 gpio1_28, xung 50 µs mỗi mẫu, cạnh lên
 boot          U-Boot 2025.01 ── extlinux.conf (layer) ── zImage + am335x-boneblack.dtb (-@)
                                                     └─ fdtoverlays /overlays/mpu6050.dtbo
 DT/overlay    &i2c2 { clock-frequency 400000; imu@68 compatible "projectb,mpu6050",
               interrupts gpio1 28 EDGE_RISING, pinctrl chân INT mode 7 kéo xuống }
 kernel        linux-yocto-rt 6.12.47 (PREEMPT_RT), i2c-omap, gpio-omap
 driver        mpu6050.ko (ngoài cây, driver/mpu6050/): i2c_driver + misc device
 uapi          <projectb/mpu6050.h>: struct mpu6050_sample 16 B, 3 ioctl; sysfs cấu hình
 userspace     mpu6050-test (poll + read), mpu6050-boot-check (systemd, udev)
```

Khởi động không thao tác tay: overlay tạo thiết bị `2-0068` → uevent mang modalias
`of:N…Cprojectb,mpu6050` → udev modprobe `mpu6050` (từ `modules.alias`) → probe tạo
`/dev/mpu6050` → rule `90-mpu6050.rules` (quyền 0600 root, `TAG+="systemd"`,
`SYSTEMD_WANTS`) → unit `mpu6050-boot-check` chạy selftest + 1000 bản ghi, kết quả ở
`journalctl -u mpu6050-boot-check`. Unit `BindsTo=dev-mpu6050.device`: unbind thì unit
dừng, bind lại thì chạy lại.

Mỗi tầng chỉ dựa vào tầng ngay dưới qua một giao diện có tên:
- Driver không biết chân nào, bus nào: lấy địa chỉ, ngắt, kiểu cạnh từ DT, tốc độ bus từ
  `clock-frequency` của adapter (để tính cỡ lô). Đổi chân INT hay tốc độ bus chỉ sửa overlay.
- Userspace không biết thanh ghi: chỉ thấy bản ghi đã có đơn vị (LSB, ts ns) và hệ số
  (`accel_lsb_per_g`, `temp_sens_x100`) qua ioctl. Đổi chip 6050/6500 không sửa app.
- Mọi thứ build từ layer `meta-projectb/`: overlay (recipe `projectb-overlays`), cmdline
  và extlinux (`wic/`), module (`mpu6050-mod`), header (`mpu6050-uapi`, allarch, để app
  không build lại khi đổi kernel), app, unit tự kiểm. Không sửa tay trên thẻ.

## Hợp đồng driver ↔ userspace (chốt tuần 5, không đổi tới GĐ2)

- `/dev/mpu6050`, một người mở (lần mở thứ hai `EBUSY`). Mở là bật nguồn dữ liệu, đóng là tắt.
- `read()` trả bội số 16 byte, chỉ bản ghi trọn; rỗng thì chặn hoặc `EAGAIN`. `poll()` báo
  `POLLIN` khi có bản ghi.
- ioctl: `GET_CONFIG` (tốc độ thật, dải, hệ số), `GET_STATS` (`dropped`), `FLUSH`.
- sysfs `/sys/class/misc/mpu6050/`: `sample_rate` (4..1000 Hz, đọc lại tốc độ thật),
  `accel_range` (2/4/8/16 g), `dropped` (chỉ đọc). Đổi cấu hình lúc đang stream: dừng, ghi,
  xóa fifo, chạy lại; mọi bản ghi đọc sau khi `write` trả về là theo cấu hình mới.
- `dropped`: số mẫu chip đã tạo ra (có cạnh INT) mà driver làm mất, đếm từ lúc probe. Chip
  mất nguồn thì không có mẫu, `dropped` không tăng; khoảng mất thấy trong `ts_ns`.
- Thiết bị bị gỡ (unbind) khi fd còn mở: `read` trả nốt bản ghi còn lại rồi `ENODEV`,
  `poll` báo `POLLHUP | POLLERR`, ioctl `ENODEV`.

## Luồng dữ liệu

```
 chip: mẫu → FIFO 512 B (khung 8 B: accel xyz + temp) + xung INT
   │
   ▼ cạnh lên gpio1_28
 nửa trên (mpu6050_irq; trên RT chạy trong thread irq/26 của gpio-omap)
   ts = ktime_get_ns() → kfifo edges (128); đủ batch cạnh mới → đánh thức thread
   │
   ▼
 thread IRQ (SCHED_FIFO 50, mpu6050_irq_thread)
   chụp q cạnh → chờ tới 60 µs sau cạnh (chip NACK nếu đọc trong xung 50 µs)
   → đọc FIFO_COUNT → kiểm (count % 8, đầy) → ghép khung với cạnh → đọc n khung một
   giao dịch → mỗi khung một ts → kfifo fifo (256 bản ghi) → wake_up
   │
   ▼
 read()/poll() của userspace
```

- Cỡ lô (`batch`): khoảng 10 ms, nhưng không quá số khung đọc kịp trước lần chip ghi mẫu
  kế: khung = 72 bit / tốc độ bus, ngân sách = chu kỳ − 450 µs (từ cạnh tới lúc bắt đầu
  đọc khung) − 150 µs lề. 400 kHz: 100 Hz → 10, 500 Hz → 5, 1000 Hz → 2. Lý do: đọc FIFO
  đúng lúc chip ghi thì ~1% số lần con trỏ FIFO của chip trượt vài byte, mọi khung sau lệch
  (LA `docs/fifo-misalign-1khz-2026-10-30.sr`).
- ts: mỗi mẫu mang ts của chính cạnh INT của nó. Mẫu không có cạnh (FIFO có sẵn lúc start,
  hoặc cạnh dồn) được đặt lùi theo chu kỳ hoặc chia vào khoảng cạnh còn dư.
- Không có ngắt "FIFO đủ N mẫu" trên chip này: chia nhịp làm ở nửa trên bằng đếm cạnh.
- Chờ sau cạnh: driver chỉ có ts (lấy ở nửa trên), không có thời điểm cạnh thật. ts trễ cạnh
  6–18 µs trên RT không debug, ≥ 40 µs trên RT debug (lockdep làm thread `irq/26` chậm), ~0
  trên non-RT. Chờ tới ts + 60 µs, trừ 40 µs chỉ khi RT + lockdep; trừ 40 µs trên RT
  không debug thì đọc ở 31–37 µs sau cạnh, NACK 1.3% (500 Hz) và 8.6% (1000 Hz).

## Khóa và ngữ cảnh

| Đối tượng | Ai ghi | Ai đọc | Bảo vệ |
|---|---|---|---|
| `edges` (kfifo ts cạnh) | nửa trên | thread | một ghi một đọc, không khóa |
| `fifo` (bản ghi) | thread | `read` | một producer; `read_lock` (mutex) phía đọc |
| thanh ghi chip, cấu hình, bật/tắt | open/release, sysfs, watchdog | | `cfg_lock` (mutex) |
| `dropped` | nửa trên, thread | sysfs, ioctl | `atomic64_t` (u64 trên ARM32) |

- Thứ tự: `cfg_lock` rồi `read_lock`, không bao giờ ngược lại. Thread IRQ không lấy khóa
  nào: `mpu6050_stop` giữ `cfg_lock` và `disable_irq` chờ thread xong.
- Watchdog (delayed work) lấy `cfg_lock`, nên `cancel_delayed_work_sync` chỉ gọi ngoài
  `cfg_lock`. Lockdep (kernel debug) im suốt tuần 4–8.

## Vòng đời và đường lỗi

- `struct mpu6050` sống theo kref: thiết bị giữ một tham chiếu (trả bằng devm action
  cuối cùng, sau khi ngắt đã gỡ), mỗi fd mở giữ một. rmmod khi có fd bị chặn bởi
  `fops.owner`; unbind thì `remove` đặt `gone`, đánh thức người đọc.
- Đọc `FIFO_COUNT` lỗi: giữ nguyên cạnh, lô sau đọc lại. Đọc khung lỗi, count lẻ, FIFO
  đầy: reset FIFO, mọi cạnh đang chờ tính vào `dropped`. Reset lỗi giữa chừng
  (`FIFO_EN` có thể đã 0): `reset_pending`, lô sau làm lại.
- Bus hỏng khi đang stream (rút SDA): chip vẫn chạy, cạnh vẫn tới; hàng đợi cạnh đầy thì
  mỗi cạnh mới tính `dropped` ngay; cắm lại thì FIFO đầy → reset → chạy tiếp. Log có giới
  hạn tốc độ (≤ 10 dòng/5 s).
- Chip mất nguồn: hết cạnh INT; watchdog (1 s, chỉ khi có fd mở) thấy không có cạnh thì
  khởi tạo lại chip như probe (đọc `WHO_AM_I` trước, bus còn hỏng thì không ghi gì). Cũng
  watchdog đó bật lại nguồn dữ liệu khi đổi cấu hình lỗi giữa chừng.
- Ghi sysfs lúc bus hỏng: trả lỗi (`EREMOTEIO`) trong ~0.15 s, cấu hình cũ giữ nguyên.

## Số đo chính

| Phép đo | Kết quả | Nguồn |
|---|---|---|
| 100–500 Hz, 60 s, RT debug và non-RT | `dropped` 0, Δirq = bản ghi + `dropped` | tuần 7 |
| 1000 Hz | non-RT 30000/30000; RT debug ~0.03% mất, vài khung hỏng | tuần 7 |
| ts so với cạnh INT (LA, 100 Hz) | −59..+52 µs, 0 bản ghi chung cạnh | tuần 7 |
| `load 1800` (stress-ng, 30 phút, 100 Hz) | 180650 bản ghi, `dropped` 0; khoảng ts lớn nhất 26.2 ms (RT debug), 10.6 ms (non-RT) | tuần 7 |
| Tràn FIFO cố ý (chặn thread 1 s) | khoảng ts 107 chu kỳ, `dropped` +106 | tuần 7 |
| Rút SDA 17.9 s ở 100 Hz | `dropped` +1795 (≈ 17.9 s × 100), tự hồi | tuần 8 |
| Mất nguồn chip 14.7 s | watchdog khởi tạo lại, stream tiếp, 0 khung lệch byte | tuần 8 |
| Unbind khi stream, 20 vòng | 0 lỗi, `st` giải phóng đủ, slab không tăng | tuần 8 |

Kernel của ảnh nộp (`6.12.47-rt6-yocto-preempt-rt`, không debug, module `a38b5ad7`):

| Phép đo | Kết quả |
|---|---|
| sweep 100–1000 Hz, 10 s mỗi mức | `dropped` 0, `misaligned` 0, CPU rảnh 90–98% |
| 500 / 1000 Hz, 30000 bản ghi, ftrace | đọc count 70–81 µs sau cạnh, NACK 0, `misaligned` 0 |
| `load 600` (stress-ng cpu+io+vm, 100 Hz) | 60568 bản ghi = irq, `dropped` 0, khoảng ts lớn nhất 10.2 ms |
| Tràn cố ý 1 s | `dropped` +107, dư 0 |
| Rút SDA 22.1 s (demo) | `dropped` +2210 |

Tốc độ cam kết cho `dropped` = 0: 500 Hz (sạch trên mọi kernel đã thử); 1000 Hz sạch trên
RT không debug và non-RT, còn ~0.03% mất trên RT debug.

## Giới hạn đã biết

- RT: cạnh INT có lúc tới nửa trên trễ vài ms (thread `irq/26` chờ khóa BH của một
  kworker); không mất mẫu, ts của cạnh đó trễ. Trên RT debug ở 1000 Hz việc này đẩy lô
  sang lúc chip ghi, FIFO lệch, vài khung hỏng lọt trước khi count lẻ phát hiện.
- Độ trễ ts so với cạnh là hằng số chọn theo cấu hình kernel (đo bằng `scripts/mpu-lag.sh`);
  kernel khác (cấu hình khác) phải đo lại.
- `dropped` gộp mọi nguồn mất (FIFO đầy, hàng đợi cạnh đầy, lô lỗi); không tách theo nguyên nhân.
- `.config` RT và non-RT chưa san bằng (~140 dòng ngoài preemption): so benchmark RT/non-RT
  để GĐ2.
- Một chip, tên `/dev/mpu6050` cố định.

## Ảnh LA dùng cho báo cáo

| File | Cho thấy |
|---|---|
| `i2c-whoami-2026-10-22.png` | giao dịch đọc `WHO_AM_I` = 0x70 đầu tiên |
| `i2c-record-2026-10-29.png` | đọc một bản ghi (tuần 5, polling) |
| `int-pulse-2026-10-30.sr` | xung INT 50 µs mỗi mẫu |
| `fifo-100hz-400k-2026-10-30.sr` (+ `.bin`) | đọc FIFO theo lô ở 400 kHz, khớp từng khung với bản ghi |
| `fifo-misalign-1khz-2026-10-30.sr` | FIFO lệch byte khi đọc trùng lúc chip ghi |
| `int-nack-nonrt-2026-10-30.sr` | chip NACK khi bị đọc trong xung INT (non-RT) |
