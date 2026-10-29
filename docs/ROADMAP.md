# Roadmap đã điều chỉnh (2026-10-16)

Điều chỉnh trên khung spec gốc (`~/workspace/Project/project-B-embedded-linux.md`), không đập đi làm lại.
Mục tiêu: làm sâu hai tầng còn mỏng trong spec (bootloader/platform và middleware userspace),
và sửa một lỗi kỹ thuật trong spec gốc.

## Lý do

So độ chi tiết giữa các tầng trong spec gốc:
- Kernel: sâu nhất (10 hạng mục)
- Build system: gần ngang kernel
- Bootloader/platform: chỉ 3 từ khóa, không có thiết kế
- Userspace/middleware: 4 từ khóa, không có kiến trúc luồng dữ liệu, không có IPC cụ thể

Hai lỗi thiết kế trong spec gốc:
1. Spec ghi "FIFO watermark interrupt". MPU6050 không có ngắt này (INT_ENABLE chỉ có
   DATA_RDY, FIFO_OFLOW, I2C_MST và các ngắt chuyển động). Dùng DATA_RDY và đọc FIFO theo lô.
2. Demo/HMI đọc dữ liệu liên tục qua sysfs. Sysfs dành cho cấu hình (một giá trị một file).
   Đọc hàng trăm lần/giây kiểu open→read→parse ASCII→close tốn CPU trên máy 1 core.
   Luồng dữ liệu phải đi qua char device, dạng nhị phân, đánh thức bằng poll().

## Quyết định thiết kế

### Hợp đồng kernel ↔ userspace (chốt ở GĐ1, không đổi ở GĐ2)
- `/dev/mpu6050`: `read()` trả nhiều bản ghi nhị phân một lần
  `struct { s16 ax, ay, az; s16 temp; u64 ts_ns; }`, `ts_ns` lấy bằng `ktime_get_ns()` trong IRQ
- `poll()` đánh thức khi có dữ liệu (wait queue); `ioctl()` cho lệnh điều khiển
- Struct và mã ioctl nằm trong một header uapi dùng chung giữa driver và app
- Sysfs chỉ cấu hình: sample rate, thang đo, bộ đếm mẫu bị mất
- Nguồn ngắt: DATA_RDY; đọc FIFO theo lô trong threaded IRQ; kfifo giữa IRQ và read()
- Trong báo cáo/phỏng vấn: nêu rõ kernel chuẩn hóa cảm biến kiểu này bằng IIO subsystem
  (driver mainline ở `drivers/iio/imu/inv_mpu6050`); project tự viết char device để hiểu cơ chế

### Middleware GĐ2: `sensord` (C++17)
```
/dev/mpu6050 --poll()--> [luồng đọc, SCHED_FIFO] --> SPSC ring (lock-free) --> [luồng publish]
                                                                                    |
                                                         POSIX shared memory (ring + số thứ tự)
                                                               |                  |
                                                         HMI (LVGL)          AI (TFLite)
HMI --POSIX message queue--> sensord   (lệnh: đổi sample rate, đặt ngưỡng)
```
- Data plane (luồng mẫu liên tục, nhiều người đọc): shared memory ring có số thứ tự.
  Người ghi không bao giờ chờ người đọc; người đọc chậm tự thấy bị tụt (seq nhảy) và bỏ qua.
- Control plane (lệnh thưa, cần đúng thứ tự): POSIX message queue.
- SPSC ring giữa hai luồng là lock-free vì luồng SCHED_FIFO không được chờ lock.
  Mutex + condition_variable dùng cho cấu hình dùng chung giữa các luồng.
- Chạy bằng systemd unit, user không phải root (udev rule cho `/dev/mpu6050`).
- Recipe Yocto (CMake) + unit test ring buffer (GoogleTest, chạy qua ptest).

### Benchmark runner GĐ2 (C)
`fork/exec` stress-ng (giới hạn `--vm-bytes`) và công cụ đo, `kill` + `waitpid` lấy exit status,
đặt `SCHED_FIFO`. Chế độ Benchmark chỉ chạy `sensord`; đo độ trễ IRQ → userspace xuyên qua daemon.

### OTA
Lùi xuống thiết kế trên giấy trong báo cáo. Spec gốc đã để optional.

## Phân chia GĐ1 / GĐ2

### Giữ ở GĐ1 (thêm khoảng nửa ngày đến một ngày, không đổi mốc tuần 8)
| Việc | Tuần | Vì sao không dời |
|---|---|---|
| API driver nhị phân (`read`/`poll`/`ioctl`) + header uapi; sysfs chỉ cấu hình | 4–8 | Đổi sau phải sửa driver, trái quy tắc "GĐ2 không sửa driver" |
| Chương trình test GĐ1 dùng `poll()` + đọc nhị phân | 4–8 | Cùng công sức với đọc sysfs, chứng minh luôn API |
| Kernel cmdline / extlinux nằm trong layer (không sửa tay trên thẻ) | 2 | Cần để U-Boot nạp overlay; bài học tuần 1: `sed` trên thẻ mất khi build lại |
| Device tree overlay MPU6050: `compatible` riêng, pinmux I2C2 + chân INT, đóng gói `.dtbo` trong layer | 3 | Overlay vốn là sản phẩm GĐ1. Kiểm pinmux bằng `devmem` của busybox (control module `0x44E10000`) |

Lợi ích phụ: `.dtbo` nằm trên phân vùng FAT, khi đã có SSH thì đổi overlay bằng `scp` + reboot,
không cần flash lại thẻ (đầu đọc SD hiện tại không đáng tin).

### Dời trong GĐ1 (tuần 2 → tuần 3)
| Việc | Vì sao dời |
|---|---|
| `.bbappend` kernel (config fragment) | Kiểm `.config` ngày 2026-10-20: `I2C_OMAP`, `I2C_CHARDEV` built-in, `PREEMPT` bật; overlay do U-Boot gộp nên không cần `OF_OVERLAY`. Chưa có cấu hình nào cần giữ riêng; `.bbappend` rỗng buộc build lại kernel mà không thu thông tin. Người dùng đầu tiên là kernel RT (`PREEMPT_RT`) ở tuần 3 |

### Dời sang GĐ2
| Việc | Tuần |
|---|---|
| Machine conf riêng (`bbb-projectb.conf`), distro conf | 9–11 hoặc khi rảnh |
| Watchdog handoff U-Boot → `omap_wdt`, `RuntimeWatchdogSec` | 9–11 |
| Benchmark runner (`fork/exec/waitpid`) | 9–11 |
| udev rule cho `/dev/mpu6050` | 12–14 |
| `sensord`, shared memory, message queue, mutex/condvar | 12–14 |
| HMI và AI đọc từ `sensord` qua shared memory | 12–14 |
| OTA: chỉ thiết kế trên giấy | 15–16 |

Nếu tuần 12–14 trễ: cắt AI xuống bản ngưỡng đơn giản trước, giữ `sensord` và HMI.

## Kế hoạch driver tuần 4–8 (chốt 2026-10-23)

Spec chỉ cho thứ tự char device → I2C → sysfs → ngắt + FIFO. Đổi một chỗ: I2C lên trước (chưa probe
thì char device không có gì để trả). Giao diện (uapi, sysfs) chốt sớm; IRQ ở tuần 6 để tuần 7–8 làm
buffer. Nguồn dữ liệu và `read`/`poll` tách nhau bằng kfifo: tuần 5 nguồn tạm là hrtimer + work
(`delayed_work` không ra được 10 ms với `HZ=100`, xem PROGRESS tuần 5), tuần 6–7 thay bằng IRQ + FIFO,
phía đọc không đổi.

| Tuần | Việc | Mốc kiểm cuối tuần |
|---|---|---|
| 4 | Recipe module, vòng `bitbake` → `scp` → `insmod` (module chưa vào ảnh: udev sẽ tự nạp lúc boot). Fragment debug `DEBUG_ATOMIC_SLEEP` + `PROVE_LOCKING`, bật/tắt bằng biến trong template. Probe: `DEVICE_RESET`, chờ, ghi tường minh mọi thanh ghi cấu hình (kể cả `DLPF_CFG` 1–6, không thì `SMPLRT_DIV` vô hiệu), `WHO_AM_I` chọn thông số. Đọc burst 14 byte | `UU`; `0x44E10878` = `0x27`; chip id trong dmesg; \|a\| ≈ 1 g; 20 vòng `insmod`/`rmmod` + unbind/bind sạch, giá trị cấu hình đọc lại đúng sau mỗi vòng; logic analyzer: một giao dịch 14 byte |
| 5 | Header uapi (struct 16 byte, `static_assert`), `miscdevice`, kfifo, wait queue, `read` trọn bản ghi, `O_NONBLOCK`, một người mở. Sysfs `sample_rate`, `accel_range`, `dropped`. App test `poll()` có recipe | Lắc → số đổi; đổi `sample_rate` → tốc độ đổi; `strace`; `EINVAL`, `EAGAIN`; `rmmod` khi fd mở bị chặn |
| 6 | DATA_RDY, `request_threaded_irq` + `IRQF_ONESHOT`, `ktime_get_ns()` ở primary handler. Bỏ hrtimer + work | `/proc/interrupts` tăng đúng `sample_rate`; thread `SCHED_FIFO` 50; logic analyzer INT + I2C; 30 phút với `stress-ng`; lockdep im |
| 7 | FIFO (`ACCEL` + `TEMP`, 8 byte/mẫu), lô, `ts_ns` nội suy lùi, tràn → `dropped` + reset FIFO. Thí nghiệm tràn có/không reset. Đo thời gian I2C 100 kHz, quyết 400 kHz. Build non-RT + san bằng `.config` chung đợt | `dropped` = 0 ở tốc độ tối đa đã chọn; tràn cố ý → `dropped` tăng, tự hồi; cả hai kernel qua bộ kiểm |
| 8 | Đường lỗi (I2C lỗi, unbind khi stream), module vào ảnh, tài liệu kiến trúc, ảnh LA, log, diễn tập demo, tag `v1.0-chuyennganh` | Thẻ ghi mới, driver tự nạp, test chạy không thao tác tay |

Ràng buộc đã chốt khi duyệt kế hoạch:
- "512 chia hết cho 8 nên tràn FIFO không lệch bản ghi" là giả thuyết; datasheet chỉ ghi "oldest data
  will be lost". Driver luôn reset FIFO khi tràn; tuần 7 kiểm bằng thí nghiệm, chưa kiểm thì không ghi
  vào báo cáo như sự thật.
- Probe reset chip bằng `DEVICE_RESET` vì `insmod`/`rmmod` không làm chip mất nguồn: không được dựa
  vào giá trị reset lúc cấp nguồn.
- Fragment lockdep đổi kích thước `spinlock_t` mà `vermagic` không đổi: mỗi lần bật/tắt fragment phải
  build lại module, không dùng lại `.ko` cũ. Số đo thời gian tuần 7 đo trên kernel không có fragment.
  Chặn bằng hậu tố: `PROJECTB_KERNEL_DEBUG = "1"` thêm `-debug` vào `LINUX_VERSION_EXTENSION` (`:append`,
  giữ `??=` của `linux-yocto.inc`), vermagic khác nên module lệch bị từ chối. Không chọn `MODVERSIONS`
  (genksyms, `Module.symvers` cho module ngoài cây). Thử âm: `hid-multitouch.ko` RT không debug lưu ở
  `~/projectb-baseline/rt-nodebug-ko/` (`9bb08bd2…`), insmod trên kernel debug phải bị từ chối.

Cắt nếu trễ, theo thứ tự: LVGL → ioctl còn một lệnh → FIFO theo lô (giữ DATA_RDY từng mẫu, ~100 Hz)
→ test non-RT dời đầu GĐ2. Không cắt: uapi, tên sysfs, ngắt.

Ghi nhận cho GĐ2: trên RT `gpio-omap` xin IRQ với cờ 0 (`gpio-omap.c:1077`) nên bị ép thread
`irq/26-4804c000.gpio`; handler `IRQF_ONESHOT` của driver không bị ép thread (`manage.c:1372`) nhưng
được gọi từ trong thread đó. `ts_ns` trên RT đã gồm độ trễ đánh thức thread GPIO.

## Phủ kiến thức sau điều chỉnh

| Mảng | Mục | Ở đâu |
|---|---|---|
| Embedded Linux | Kernel module | Driver out-of-tree, recipe `inherit module` |
| | Character device | `read`/`poll`/`ioctl`, wait queue, `copy_to_user`, kfifo |
| | Device tree | Overlay, `compatible` ↔ `of_match_table`, `interrupts` |
| | Pinmuxing | `pinctrl` cho I2C2 và INT, kiểm thanh ghi bằng `devmem` (busybox) |
| System programming | File I/O | `/dev/mpu6050` nhị phân, `O_NONBLOCK`, `poll` |
| | System calls | `open/read/ioctl/poll/mmap/shm_open/mq_open/sched_setscheduler`; quan sát bằng `strace` |
| | Multithreading | `sensord` hai luồng, `SCHED_FIFO` |
| | Mutex | Kernel: mutex cho read/ioctl, spinlock IRQ↔thread. Userspace: mutex + condvar |
| | Shared memory | Data plane `sensord` → HMI, AI |
| | Message queue | Control plane HMI → `sensord` |
| | Process management | Benchmark runner `fork/exec/waitpid/kill`; signal trong `sensord` |
