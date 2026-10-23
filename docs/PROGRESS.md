# Nhật ký tiến độ

Mỗi session kết thúc ghi thêm một mục ở đầu file (mới nhất lên trên).
Mục gồm: đã làm, kết quả đo được, việc tiếp theo, câu hỏi còn mở, commit tương ứng.

---

## Tuần 4 — driver MPU6050 (bắt đầu 2026-10-23)

Kế hoạch tuần 4–8 và ràng buộc: `docs/ROADMAP.md`, mục "Kế hoạch driver tuần 4–8".

**Kernel debug cho phát triển driver (2026-10-23)**
- `files/debug-locking.cfg` (`PROVE_LOCKING`, `DEBUG_ATOMIC_SLEEP`), bật bằng `PROJECTB_KERNEL_DEBUG = "1"` trong template; `projectb-kernel.inc` thêm `-debug` vào `LINUX_VERSION_EXTENSION` bằng `:append` (giữ `??=` của `linux-yocto.inc`). Kiểm `bitbake -e` trước build: RT bật `-yocto-preempt-rt-debug`, tắt `-yocto-preempt-rt` (không thừa ký tự), non-RT `-yocto-standard-debug`
- Build (`docs/build-2026-10-23-debug.log`): 0 WARNING, `log.do_kernel_configcheck` sạch. `.config` so với mốc chỉ lệch `LOCALVERSION` + những gì hai dòng fragment kéo theo (`LOCKDEP`, `DEBUG_SPINLOCK`, `DEBUG_LOCK_ALLOC`, `DEBUG_RT_MUTEXES`, `DEBUG_WW_MUTEX_SLOWPATH`, `TRACE_IRQFLAGS`, `PROVE_RCU`, `LOCKDEP_*_BITS`, `PREEMPTIRQ_TRACEPOINTS`, `DRM_DEBUG_MODESET_LOCK`). `PROVE_RAW_LOCK_NESTING` tắt (bật khi thử non-RT tuần 7)
- `zImage` 9059216 → 9800544 byte (+8.2%), `a9872bda…`; dtb, dtbo không đổi. Chuỗi phiên bản lan vào đường dẫn `/lib/modules/` và tên mọi gói kernel. Ảnh `rootfs-20261023085915.wic` (`20fbfbbc…`)
- Trên bo (`boot-serial-2026-10-23-debug.log`, 2 lần boot): `uname -r` có `-debug`, `debug_locks: 1`, không báo cáo lockdep nào từ driver có sẵn: mốc sạch trước driver. `hid_multitouch` vẫn tự nạp (udev theo `modules.alias`)
- Thử âm: `hid-multitouch.ko` RT không debug (`~/projectb-baseline/rt-nodebug-ko/`, `9bb08bd2…`) → `insmod` `Invalid module format`, dmesg `version magic '…preempt-rt …' should be '…preempt-rt-debug …'`. Bị chặn ở `check_modinfo` trước bước kiểm trùng tên (`kernel/module/main.c:2842`/`2847`), nên thử được cả khi bản debug đang nạp
- Mốc RT không debug đầy đủ: `~/projectb-baseline/rt-20261022/` (wic `05bd56a4…`, `zImage` `818fb8a7…`, dtb, dtbo, modules, manifest, `kernel.config` rút từ `zImage` bằng `extract-ikconfig`), `sha256sum -c` OK. Lấy lại từ `sstate-cache` vì deploy đã bị gỡ (bẫy bên dưới)

**Khung driver (2026-10-23)**
- `driver/mpu6050/` (`mpu6050.c`, `Makefile`, `COPYING`), recipe `meta-projectb/recipes-kernel/mpu6050/mpu6050-mod_0.1.bb` (`inherit module`, `SRC_URI = "file://mpu6050"` qua `FILESEXTRAPATHS` ra `driver/`). Chưa vào ảnh; vòng thử: `bitbake mpu6050-mod` → `ssh … 'cat > /tmp/mpu6050.ko'` → `insmod`
- Build: không warning (dòng `Skipping depmod` trong `do_install` là bình thường, depmod chạy lúc dựng rootfs). Bản để thử: `packages-split/kernel-module-mpu6050-*/…/updates/mpu6050.ko` (6456 byte, đã strip); bản có debug info cho `addr2line`: `image/…/mpu6050.ko` (207876 byte). modinfo: alias `of:N*T*Cprojectb,mpu6050` (+`C*`), vermagic `-debug`
- Trên bo: `insmod` → probe tự chạy (driver đăng ký thì bus khớp client `2-0068` có sẵn), `MPU-6500 family (WHO_AM_I 0x70), irq 50`. Ô 0x68 `68` → `UU` → `68` sau `rmmod`. `devmem 0x44E10878` `0x37` → `0x27`, sau `rmmod` vẫn `0x27`. `debug_locks` vẫn 1. Taint `O` (out-of-tree) không tắt lockdep
- IRQ 50 là virq: `/sys/kernel/irq/50/hwirq` = 28 (chân trong bank GPIO1). Hai tầng: P9_12 → GPIO1 (hwirq 28, virq 50) → INTC (hwirq 98, virq 26, `4804c000.gpio`) → CPU. Virq 50 chưa có trong `/proc/interrupts` vì chưa có `request_irq` (`proc.c:494` bỏ desc không có action). `chip_name` rỗng: `gpio-omap` dùng `irq_print_chip`, không đặt `name`
- Pinctrl khi gỡ driver chỉ trả quyền sở hữu, không ghi lại thanh ghi: `pinmux-pins` pin 30 lúc bind `2-0068 … function mpu6050-int-pins`, sau `rmmod` `MUX UNCLAIMED`, thanh ghi vẫn `0x27`. `pinmux_disable_setting` → `pin_free`, `pinctrl-single` không có `.free` (`pinctrl-single.c:448`). Muốn trạng thái khi không có driver thì khai `sleep` state + `pinctrl_pm_select_sleep_state()`; chân INT không cần (input kéo xuống, chip đẩy push-pull)

**Bước 2: reset, cấu hình, burst (2026-10-23)**
- Probe: `WHO_AM_I` → bảng `mpu6050_chips[]` (6500/6050: giá trị reset `PWR_MGMT_1`, `ACCEL_CONFIG2`, signal path reset, công thức nhiệt độ, FIFO) → `DEVICE_RESET`, 100 ms, `SIGNAL_PATH_RESET` (họ 6500, như `inv_mpu_core.c:1758`), 100 ms → kiểm `PWR_MGMT_1` = giá trị reset → ghi + đọc lại: `PWR_MGMT_1` 0x01 (PLL, gyro bật vì PLL lấy nhịp gyro), `PWR_MGMT_2` 0, `CONFIG` 0x03 (`DLPF_CFG` 3), `SMPLRT_DIV` 9 (100 Hz), `GYRO_CONFIG` 0, `ACCEL_CONFIG` 0 (±2 g), `ACCEL_CONFIG2` 0x03 (41 Hz), `INT_PIN_CFG`/`INT_ENABLE`/`FIFO_EN`/`USER_CTRL` 0 → burst 14 byte (`i2c_smbus_read_i2c_block_data`), in một mẫu (tạm, bỏ ở tuần 5). Remove: `PWR_MGMT_1` 0x41
- NACK trên `omap_i2c` trả `-EREMOTEIO` (-121), `i2c-omap.c:782`
- Trên bo: `i2cget -f` lúc bind `0x6b`=0x01, `0x19`=0x09, `0x1a`=0x03; sau `rmmod` 0x6b = 0x41. 20 vòng `insmod`/`rmmod` (`pass=20`; vòng nào cũng bắt đầu từ 0x41 nên mỗi lần đọc 0x01 là bằng chứng reset chạy) + 5 lần unbind/bind: 27 probe, 27 remove (26 + 1 lần thử tay trước), |a| 994–1002 mg (đa số 997), nhiệt độ 34.0 → 34.4 °C trong vòng thử, gyro đứng yên 0.2–1.3 °/s. Không có lỗi/lockdep từ lần nạp đầu (`grep rc=1`), `debug_locks: 1`
- Còn của tuần 4: ảnh logic analyzer giao dịch burst

**Bẫy mới**
- busybox `dmesg` không có `-C`; dùng `dmesg -c` (đọc rồi xóa). Khối lệnh dán vào shell có thể mất dấu xuống dòng: lệnh sau dính vào chuỗi của lệnh trước
- `bitbake -e` không chạy task nhưng vẫn bắn `ReachableStamps`: đổi `PREFERRED_PROVIDER_virtual/kernel` (kể cả bằng `-R x.conf` chỉ để hỏi biến) làm kernel kia "không còn dùng tới", `sstate_eventhandler_reachablestamps` (`sstate.bbclass`) gỡ `zImage`, dtb, `modules-*.tgz` khỏi deploy và xóa thư mục work của nó. `sstate-cache/` còn nguyên. Bằng chứng: `tmp/log/eventlog/20261023085532.json`, `Removing 1 recipes from the beaglebone_yocto sysroot`. Lưu mốc ra ngoài `tmp/` trước khi đổi kernel
- `bitbake -e | grep` phải neo `^TÊN=`: phần lịch sử gán in dưới dạng dòng `#` chứa biểu thức chưa tính
- Đường dẫn dài dán vào terminal bị gãy thành hai lệnh: đặt biến (`WIC=…`) rồi dùng; kiểm `sha256sum $WIC` trước khi ghi thẻ
- Ảnh có thể không có `sftp-server`: chép file lên bo bằng `ssh … 'cat > /tmp/x' < file`
- Thông báo kernel in thẳng ra `ttyS0` chen vào giữa stderr của lệnh trên serial

---

## Tuần 4 — bàn giao, chưa bắt đầu (ghi 2026-10-22)

Đọc mục này trước khi bắt đầu tuần 4 ở session mới. Tuần 1–3 xong, tag `bringup-done` (`33d90b0`, đã push). Chi tiết ở các mục bên dưới; ghi chú kiến thức tuần 3 ở `~/workspace/VKU/DoAn/week3-knowledge-summary.md`.

**Hệ thống đang chạy (sau tuần 3)**
- Bo chạy ảnh RT `rootfs-20261022161133.wic`: kernel `6.12.47-rt6-yocto-preempt-rt` (`zImage` `818fb8a7…`), dtb `am335x-boneblack.dtb` có `__symbols__` (`cd9c1103…`, giống nhau ở RT và non-RT), U-Boot nạp `/overlays/mpu6050.dtbo` (`1a1a0a24…`) qua `fdtoverlays` trong extlinux
- Build: `build-clean`, template `local.conf.in` đặt `PREFERRED_PROVIDER_virtual/kernel = "linux-yocto-rt"`. Bỏ dòng đó + `setup-build.sh build-clean` để về non-RT (sstate có cả hai, chỉ build lại image). Phần chung của hai kernel ở `meta-projectb/recipes-kernel/linux/projectb-kernel.inc` (fragment `hid-multitouch.cfg`, `KERNEL_DTC_FLAGS = "-@"`)
- Mốc non-RT ngoài repo: `~/projectb-baseline/non-rt-20261022/` (mốc benchmark), `non-rt-20261020/` (ảnh tuần 2)
- Phần cứng: màn 7" HDMI (nguồn 5V 3A riêng, bật trước BBB), cảm ứng USB vào cổng host BBB, BBB nguồn jack 5V 2A. Module MPU: VCC→P9_3, GND→P9_1, SCL→P9_19, SDA→P9_20, INT→P9_12 (gpio1_28), AD0→P9_2. Logic analyzer clone Saleae (PulseView `fx2lafw`) trên laptop
- Chip là họ MPU-6500 (`WHO_AM_I` 0x70, `PWR_MGMT_1` reset 0x01), địa chỉ 0x68. Datasheet ở `~/workspace/VKU/DoAn/Datasheet/` (Register Map rev 2.1, Product Spec rev 1.1)
- Trên bo: client I2C `2-0068` (`name` `mpu6050`, `modalias` `of:NimuT(null)Cprojectb,mpu6050`), chưa có driver; `i2cdetect` ô 0x68 hiện `68`; `devmem 0x44E10878` = `0x37`. Thread IRQ của I2C2 và GPIO1 là `SCHED_FIFO` 50
- Ảnh không có gcc, không có `kernel-devsrc`: module build bằng Yocto, không build trên bo. Có `i2c-tools`, `devmem`, `gdbserver`, `stress-ng`, `rt-tests`, `libinput-bin`, `hid-multitouch`. Busybox: không `ps -eo`, `ip -br`, `dmesg --level`, `head -N` (xem bẫy)
- SSH chỉ bằng khóa; host key đổi mỗi lần ghi ảnh, so vân tay ECDSA trên serial trước khi `ssh-keygen -R`

**Tuần 4 theo spec**
Spec không có checklist từng tuần cho 4–8, chỉ có thứ tự: char device API nhị phân + `poll` → I2C subsystem → sysfs cấu hình → ngắt DATA_RDY + đọc FIFO theo lô; test trên cả kernel thường và RT (dòng 166; yêu cầu kernel ở dòng 70, quy tắc GĐ2 ở dòng 152–156). Cột mốc cuối tuần 8: đọc sensor qua `/dev/mpu6050` + `poll()`, interrupt chạy. Hợp đồng driver ↔ userspace đã chốt trong `docs/ROADMAP.md`. Session tuần 4 cần đề xuất kế hoạch tuần 4–8 (spec chưa chia) rồi bắt đầu.

**Điểm xuất phát đã kiểm cho driver**
- `i2c_driver` với `of_match_table` khớp `projectb,mpu6050`; `MODULE_DEVICE_TABLE(of, ...)` để udev tự nạp theo modalias
- Probe: đọc `WHO_AM_I`, chấp nhận 0x70 (và 0x68 nếu muốn hỗ trợ 6050), chọn thông số (nhiệt độ `/333.87 + 21`, FIFO 512 byte, `ACCEL_CONFIG2`). Chip thức sẵn sau reset (`PWR_MGMT_1` = 0x01)
- Đọc burst 14 byte từ 0x3B trong một giao dịch (`i2c_smbus_read_i2c_block_data` hoặc `i2c_transfer`), big-endian `s16`
- Struct uapi dùng kiểu độ rộng cố định (ABI ARM32 khác x86_64, đã thấy với `input_event`)
- Ngắt: `interrupts = <28 IRQ_TYPE_EDGE_RISING>` trên `gpio1`; INT chip mặc định active-high, xung 50 µs (`INT_PIN_CFG` = 0). `request_threaded_irq` → thread `SCHED_FIFO` 50 trên RT
- Khi driver bind phải thấy: `devmem 0x44E10878` từ `0x37` thành `0x27` (pinctrl áp lúc probe), `i2cdetect` ô 0x68 thành `UU`
- Mẫu recipe module: `poky/meta-skeleton/recipes-kernel/hello-mod/` (`inherit module`). Spec quy tắc 1: driver ở thư mục riêng trong repo, có recipe. Module build cho kernel đang chọn (`vermagic` theo chuỗi phiên bản), nên thử trên non-RT phải đổi `PREFERRED_PROVIDER` rồi build lại
- `.config` RT/non-RT lệch ~140 dòng ngoài preemption (xem mục tuần 3). Không ảnh hưởng việc viết driver; san bằng trước tuần 6

**Dữ kiện chỉ người làm có, phải hỏi trước**
- Câu trả lời của Tus: ai gửi NACK ở byte đọc cuối (để biết đã hiểu giao thức trước khi viết `i2c_transfer`)
- Chữ in trên con chip
- Đã đọc những phần nào của MPU-6500 Register Map (`PWR_MGMT_1`, `CONFIG`, `SMPLRT_DIV`, `INT_PIN_CFG`, `INT_ENABLE`, `FIFO_EN`, `USER_CTRL`, `FIFO_COUNT`), kinh nghiệm viết module kernel trước đây (Nothan-OS có driver model)
- Thời lượng thật của đồ án chuyên ngành (spec dòng 129: nếu ngắn hơn 8 tuần thì dời interrupt sang GĐ2)

**Lệch giữa spec và thực tế (đã quyết, đừng làm theo spec ở các chỗ này)**
- Spec ghi MPU6050 FIFO 1024 byte (dòng 33): chip thật 512
- Spec tuần 3 ghi overlay có `pinctrl` cho I2C2: không khai lại, dtb gốc đã giữ chân SDA/SCL
- Spec ghi `evtest`: dùng `libinput-bin` (evtest ở meta-oe)

**Bẫy đã gặp, nhớ để không lặp lại** (thêm vào danh sách ở mục bàn giao tuần 3)
- Busybox trên bo: `ip -4 addr show`, `head -n 8`, `journalctl -k -p warning` thay cho `dmesg --level`, đọc `/proc/<pid>/stat` thay cho `ps -eo`
- `find /proc/device-tree/` (có dấu `/`), không thì không vào symlink
- Đo điện áp lúc bus rảnh không phát hiện được dây cắm nhầm chân
- U-Boot bỏ qua overlay không báo lỗi rõ khi dtb thiếu `__symbols__`
- Màn bật sau BBB thì EDID timeout, DRM chạy mode dự phòng 1024x768
- Một lần boot "sạch" không chứng minh lỗi đã hết; phải đếm trong điều kiện gây lỗi
- zsh: biến chứa nhiều đối số không tự tách (dùng mảng); `$VAR:a...` bị hiểu là modifier, viết `${VAR}:...`
- Log serial: `picocom -g` nối tiếp vào file cũ nếu mở lại cùng tên

---

## Tuần 3 — màn, MPU6050, overlay, kernel RT (2026-10-21 đến 10-22)

Tiêu chí spec: màn hiện hình (1024x600, cả non-RT và RT), `i2cdetect` thấy 0x68, ảnh logic analyzer `docs/i2c-whoami-2026-10-22.png`, kernel RT boot và chịu tải, `inv_mpu6050` không bật ở cả hai kernel. Còn: tag `bringup-done`. Câu hỏi để Tus tự trả lời: ai gửi NACK ở byte đọc cuối; chữ in trên con chip.

**Đã làm**
- Mốc non-RT ở `~/projectb-baseline/non-rt-20261020/`, `sha256sum -c` OK cả 6 file, hash khớp bàn giao
- Màn 7" HDMI + cảm ứng USB (màn nguồn 5V 3A riêng, BBB 5V 2A jack): hiện login trên tty1 không cần sửa gì. `tda998x 0-0070` (I2C0) → `tilcdc ... bound` → `fb0`. EDID ưu tiên `1024x600`, `fb0` = `1024,600`. Log kernel không lên màn vì cmdline chỉ có `console=ttyS0`; getty@tty1 là do systemd
- Cảm ứng: WCH `1a86:e5e3` (USB→I2C), trên BBB nhận bằng `hid-generic` (`CONFIG_HID_MULTITOUCH` không bật), `event0`, `PROP=0`, chỉ X/Y + `BTN_TOUCH`. `hexdump` thấy sự kiện khi chạm (16 byte/sự kiện trên ARM32)

**Cảm ứng tự ngắt USB trên BBB** (đếm `USB disconnect` trong `journalctl -k -b`, 2026-10-21 UTC)
- Mỗi lần ngắt, thiết bị quay lại sau 0.64–0.66 s. Bốn lần trong 9–27 s đầu boot, sau đó chỉ khi chạm
- A: không đụng gì 11 phút: 0. B: vuốt ~1.6 phút: 39. C: lắc dây 1 phút: 1. D: chỉ tap (12:29:26→12:32:01): 26. E: vuốt, tay kia giữ vỏ cổng Ethernet/USB của BBB, 1 phút: 21
- E không giảm so với B: bác giả thuyết điện tích từ người đi qua mass USB
- F: cắm cảm ứng vào laptop (`xhci`, nhận bằng `hid-multitouch`, `PROP=2` DIRECT), chạm 1 phút: 0 lần ngắt. `hexdump` trên laptop có MT kiểu B (`ABS_MT_TRACKING_ID/POSITION_X/Y`), gói đều ~10.8 ms kể cả khi ngón đứng yên; trên BBB chỉ có sự kiện khi đổi trạng thái
- Sau F nghi driver/chế độ thiết bị. Thêm `CONFIG_HID_MULTITOUCH=m` (`linux-yocto_%.bbappend` + `files/hid-multitouch.cfg`), ảnh `rootfs-20261021125313.wic`, `zImage` `a73cf9fb…`, dtb vẫn `23abc1f3…`, `.config` chỉ lệch đúng dòng đó. Boot: `hid-generic` nhận lúc 9.07 s, udev nạp module, `hid-multitouch` nhận lại cùng id `.0001` lúc 15.60 s (reprobe trong `hid-core.c`). `libinput list-devices`: `Capabilities: touch`, `Size: 228x137mm` (descriptor khai sai, màn 7" thật khoảng 154x86 mm). Boot này không có disconnect nào
- G: `hid-multitouch`, vuốt 13:29:47→13:30:29 (42 s): 15 lần ngắt. Bác giả thuyết driver/chế độ. Pha H (về `hid-generic`) không cần làm nữa
- Còn lại: `musb` so với `xhci`, nguồn VBUS của cổng USB host BBB (bộ cảm ứng có thể ăn nguồn từ VBUS chứ không từ nguồn màn), mass qua HDMI + USB. Mẫu reconnect sau ~0.65 s giống thiết bị tự reset (sụt nguồn?) hơn là host gỡ. Để GĐ2 (HMI): thử hub USB có nguồn riêng, đo VBUS lúc chạm
- Fragment vẫn giữ vì HMI cần (`INPUT_PROP_DIRECT`, MT). Kernel RT dùng chung fragment; mốc benchmark non-RT khi so RT sẽ là kernel có fragment, không phải `non-rt-20261020`

**MPU6050 (2026-10-22)**
- Nối: VCC→P9_3, GND→P9_1, SCL→P9_19, SDA→P9_20, INT→P9_12, AD0→P9_2. Lần đầu `i2cdetect -y -r 2` trống, `i2cget` `Read failed`: SDA cắm nhầm chân. Đo DCV lúc rảnh không bắt được lỗi này (chân nhầm cũng ở mức cao); chỉ thấy khi có giao dịch
- Pinmux đúng trước cả khi có overlay: `devmem 0x44E10978/0x44E1097C` = `0x33` (mode 3, pull-up, RXACTIVE), khớp `i2c2_pins` trong `am335x-bone-common.dtsi`
- Sau khi cắm lại: `i2cdetect` thấy `68`, 200/200 lần đọc OK. `WHO_AM_I` (0x75) = `0x70`, `PWR_MGMT_1` (0x6B) = `0x01` ngay sau cấp nguồn. Cả hai khớp giá trị reset của MPU-6500 (Register Map rev 2.1: 107 = 0x01, 117 = 0x70), không phải MPU-6050 (107 = 0x40, 117 = 0x68). Chip trên module là họ MPU-6500 (hoặc clone khai như vậy)
- Dữ liệu thô (`i2cdump -y -r 0x3b-0x48 2 0x68 b`, ±2g, ±250 °/s mặc định): nằm phẳng Z = 16064, lật úp Z = −16740, X/Y < 0.04 g. Hiệu chỉnh 2 điểm trục Z: offset −338 LSB (−0.021 g), độ nhạy 16402 LSB/g (lệch 0.1% so với 16384). Nhiệt độ 3904–3952 → 32.7–32.8 °C theo công thức 6500 (công thức 6050 ra ~48 °C, vô lý). Gyro đứng yên 0.2–0.8 °/s (zero-rate offset). `i2cdump -r` đọc từng byte riêng lẻ nên có thể bị xé giá trị; driver phải đọc burst 14 byte hoặc qua FIFO
- Ảnh hưởng driver: nhiệt độ `TEMP_OUT/333.87 + 21` (6050: `/340 + 36.53`), FIFO 512 byte (6050: 1024), có `ACCEL_CONFIG2` (0x1D). Driver đọc `WHO_AM_I` lúc probe để chọn thông số, như `inv_mpu6050` mainline

- Logic analyzer (clone Saleae FX2, PulseView `fx2lafw`, 2 MHz): `docs/i2c-whoami-2026-10-22.png` + `.sr` (đoạn đã chọn, 1237 mẫu). `S, 0xD0 ACK, 0x75 ACK, Sr, 0xD1 ACK, 0x70 NACK, P`: repeated START, không STOP giữa chọn thanh ghi và đọc. Phân tích file `.sr`: 38 cạnh lên SCL (36 bit + Sr + P), chu kỳ trung bình 10.17 µs (~98 kHz, DT đặt 100 kHz), START→STOP 455.5 µs cho 1 byte dữ liệu. Ảnh cuối có nhãn SCL/SDA và nhãn decoder đầy đủ (còn khoảng trống phía trên, cắt khi đưa vào báo cáo)

**Overlay MPU6050 (2026-10-22)**
- `recipes-bsp/projectb-overlays` (`inherit devicetree`, `files/mpu6050.dts`): node `imu@68` dưới `&i2c2`, `compatible = "projectb,mpu6050"` (chuỗi riêng để `inv_mpu6050` không bao giờ nhận), `interrupt-parent = <&gpio1>`, `interrupts = <28 IRQ_TYPE_EDGE_RISING>`, pinctrl P9_12 (`0x878`) mode 7 input pull-down (`0x27`). Không khai lại pinmux SDA/SCL (dtb gốc đã giữ)
- dtb gốc không có `__symbols__` → `fdtoverlay` thử offline báo `FDT_ERR_BADOFFSET ... compiled the base blob with '-@'`; U-Boot gặp lỗi này chỉ in `Failed to apply overlay` rồi boot tiếp. Thêm `KERNEL_DTC_FLAGS = "-@"` vào `linux-yocto_%.bbappend`: dtb `cd9c1103…` (trước `23abc1f3…`), `zImage` giữ `a73cf9fb…`
- `IMAGE_BOOT_FILES:append` đưa `devicetree/mpu6050.dtbo` vào `/overlays/` trên FAT, `do_image_wic[depends]` chờ overlay; extlinux bỏ comment `fdtoverlays`. Ảnh `rootfs-20261022155529.wic`, `.dtbo` `1a1a0a24…` (kiểm bằng mtools trên phân vùng boot)
- Trên bo (`boot-serial-2026-10-22-overlay.log`): U-Boot `Retrieving file: /overlays/mpu6050.dtbo`, không báo lỗi. `/proc/device-tree/.../i2c@0/imu@68` có, `compatible` đúng, `interrupts` = `00 00 00 1c 00 00 00 01`. I2C core tạo client `2-0068` (`name` = `mpu6050`, `modalias` = `of:NimuT(null)Cprojectb,mpu6050`), chưa có driver. `i2cdetect` vẫn `68` (chưa `UU`)
- `devmem 0x44E10878` = `0x37` (mode 7, input, kéo lên), chưa phải `0x27`: pinctrl của thiết bị chỉ áp khi driver probe (`pinctrl_bind_pins` trong `really_probe`). Tuần 4 đọc lại khi driver bind, kỳ vọng `0x27`
- Bẫy: `find /proc/device-tree -name ...` ra rỗng vì điểm bắt đầu là symlink; dùng `/proc/device-tree/`
- 12 dòng `Fixed dependency cycle(s)` (tda19988 ↔ lcdc) và 2 dòng `tps65217 ... Failed to locate of_node` có ở mọi log từ 2026-10-16, không liên quan overlay

**Kernel RT (2026-10-22)**
- `linux-yocto-rt_%.bbappend` (`COMPATIBLE_MACHINE`, `KMACHINE = beaglebone` → `bsp/beaglebone/beaglebone-preempt-rt.scc` có sẵn trong kernel-cache), phần chung chuyển sang `projectb-kernel.inc`; `PREFERRED_PROVIDER_virtual/kernel = "linux-yocto-rt"` trong template `local.conf`. Nguồn RT `b463156a` (merge `v6.12/standard/base` vào `preempt-rt/base`) đã có trong `downloads/`
- Mốc non-RT thứ hai trước khi build: `~/projectb-baseline/non-rt-20261022/` (ảnh `rootfs-20261022155529`, có fragment, `-@`, overlay), `sha256sum -c` OK
- Build: 57 task, không WARNING. Kernel `6.12.47-rt6-yocto-preempt-rt`, `zImage` `818fb8a7…`, dtb vẫn `cd9c1103…` (cùng nguồn dts). `.config`: `PREEMPT_RT=y`, `HID_MULTITOUCH=m`, `# CONFIG_IIO is not set` (không có `INV_MPU6050`). Ảnh `rootfs-20261022161133.wic`
- `.config` RT so với non-RT lệch 141 dòng, không chỉ preemption: RT `-O2` (non-RT `-Os`), không KFENCE, không KGDB/KDB, không BPF_JIT/BPF_SYSCALL, CIFS/BRIDGE thành module, có KEXEC/CRASH_DUMP. Giống nhau: `HZ=100`, FTRACE, SCHED_DEBUG, LATENCYTOP, CPU_IDLE. Đây là khác biệt của ktype `standard`/`preempt-rt` trong kernel-cache. Trước benchmark tuần 6: san bằng bằng fragment trong `projectb-kernel.inc` (ít nhất `-Os`/`-O2`, KFENCE, BPF), hoặc ghi rõ trong báo cáo. Hệ quả đã thấy: systemd báo `unit configures an IP firewall, but the local system does not support BPF/cgroup firewalling` (chỉ có ở RT)
- Trên bo (`boot-serial-2026-10-22-rt.log`): `uname -a` = `6.12.47-rt6-yocto-preempt-rt #1 PREEMPT_RT`, `/sys/kernel/realtime` = 1, `is-system-running` = `running`, `--failed` rỗng, `eth0` có IP. Overlay vẫn nạp (`imu@68` có), `WHO_AM_I` = 0x70. `hid-multitouch` RT nạp được; `vermagic` = `6.12.47-rt6-yocto-preempt-rt preempt mod_unload ARMv7 p2v8` (chỉ ghi `preempt`, phân biệt bằng chuỗi phiên bản). Các dòng `l3-aon-clkctrl failed to disable`, `ti-sysc ... -16`, `regulatory.db` có y hệt ở log non-RT
- IRQ threaded ép buộc: `irq/27-4819c000.i2c`, `irq/26-4804c000.gpio` (GPIO1, có P9_12)... đều `SCHED_FIFO` 50 (đọc trường 40/41 của `/proc/<pid>/stat`; busybox `ps` không có `-eo`). Ưu tiên luồng `SCHED_FIFO` của `sensord` GĐ2 phải tính tới mức 50 này
- `stress-ng --cpu 1 --io 1 --vm 1 --vm-bytes 64M --timeout 120s`: passed 3, failed 0; không có cảnh báo kernel trong lúc chạy (`journalctl -k -p warning`)
- Màn bật sau khi BBB boot: tda998x `read edid timeout` ×2 lúc HPD lên, DRM dùng mode dự phòng (`1024x768` đầu danh sách). Reboot với màn bật sẵn: không timeout, `fb0` có, mode `1024x600`, `virtual_size` `1024,600` như non-RT. Thứ tự cấp nguồn, không phải RT. Ghi cho HMI: bật màn trước BBB, hoặc xử lý hotplug ở GĐ2

**Việc tiếp theo**
- Tag `bringup-done` sau commit RT
- Tuần 4 (driver, chạy trên kernel RT): đọc `WHO_AM_I` lúc probe để chọn thông số 6500/6050; đọc burst 14 byte (không đọc từng byte); khi bind, kiểm `devmem 0x44E10878` từ `0x37` thành `0x27` và `i2cdetect` ô 0x68 thành `UU`; `MODULE_DEVICE_TABLE(of)` với `projectb,mpu6050` để udev tự nạp (modalias `of:NimuT(null)Cprojectb,mpu6050`)
- Tuần 6: san bằng `.config` RT/non-RT trước khi so độ trễ
- GĐ2: cảm ứng tự ngắt USB khi chạm (hub có nguồn, đo VBUS); `Size` cảm ứng khai sai (228x137 mm); EDID khi màn bật sau BBB; meta-oe cho LVGL/GoogleTest/evtest
- `libinput-bin` thay `evtest` (evtest ở meta-oe)

---

## Tuần 3 — bàn giao, chưa bắt đầu (ghi 2026-10-20)

Đọc mục này trước khi bắt đầu tuần 3 ở session mới. Tuần 1 và 2 đã xong, chi tiết ở các mục bên dưới.

**Hệ thống đang chạy (sau tuần 2)**
- Boot: ROM → SPL/U-Boot 2025.01 trên thẻ SD (bootloader eMMC đã vô hiệu, không cần giữ S2) → `extlinux.conf` từ layer (`meta-projectb/wic/projectb-extlinux.conf`) → kernel 6.12.47 (`zImage` `fcbb876b…`, dtb `am335x-boneblack.dtb` `23abc1f3…`) → systemd 257.6. Dòng `fdtoverlays /overlays/mpu6050.dtbo` đang comment trong extlinux, chờ tuần 3
- Image `projectb-image` build trong `poky/build-clean` (build duy nhất còn lại), ảnh cuối `projectb-image-beaglebone-yocto.rootfs-20261020043011.wic`
- Mạng: `eth0` DHCP qua systemd-networkd, router `192.168.50.1`, bo nhận `192.168.50.182`; laptop phải cùng mạng `192.168.50.x`. PHY LAN8710 vẫn lỗi `-5` lúc probe, `projectb-phy-retry.service` thử lại
- SSH chỉ bằng khóa: `ssh -i ~/.ssh/id_ed25519_bbb -o IdentitiesOnly=yes root@192.168.50.182`. Host key đổi mỗi lần ghi ảnh: so vân tay trên serial rồi mới `ssh-keygen -R`. Serial: `picocom -b 115200 -g docs/<log>.log /dev/ttyUSB0`, `login: root` không mật khẩu
- Có sẵn trên bo: `i2c-tools`, `devmem` (busybox), `gdbserver`, `stress-ng`, `rt-tests`. Chưa có `systemd-analyze`, `evtest`

**Quy trình đã quen (đừng đổi khi không có lý do)**
- Build: `docker run --rm -it -v ~/workspace_projectb:/workspace yocto-build`, `cd /workspace/poky && source oe-init-build-env build-clean`, `bitbake projectb-image` (hoặc `scripts/yocto.sh bitbake projectb-image`). Đổi template thì chạy lại `scripts/setup-build.sh build-clean`
- Ghi thẻ: `lsblk -o NAME,SIZE,MODEL,TRAN,RM` và xác nhận thiết bị (thẻ 29.7G, `RM=1`, thường là `sda`; ổ hệ thống là `nvme0n1`) → `echo 180 | sudo tee /sys/block/sdX/device/timeout` → `scripts/flash-verified.sh <wic> /dev/sdX`
- Mỗi lần boot thử: mở `picocom` ghi log trước khi cấp nguồn, log lưu vào `docs/boot-serial-<ngày>-<tên>.log`

**Checklist tuần 3 (spec dòng 203–214)** và điểm xuất phát đã kiểm
1. Màn HDMI 7" + cảm ứng USB: kernel có `DRM_TILCDC=y`, `DRM_I2C_NXP_TDA998X=y`; log boot hiện `tilcdc ... Cannot find any crtc or sizes` vì chưa nối màn. Cần `evtest` trong image để thử cảm ứng
2. Nối MPU6050: VCC 3.3V, SDA P9_20, SCL P9_19 (I2C2), INT vào một GPIO rảnh (spec gợi ý P9_12, không trùng chân HDMI); kiểm pinout trước khi nối
3. `i2cdetect -y -r 2` thấy `0x68` (`0x69` nếu AD0 nối cao). Kernel đã có `I2C_OMAP=y`, `I2C_CHARDEV=y`; log boot có `omap_i2c 4819c000.i2c: bus 2` nên I2C2 đã bật trong dtb gốc, còn pinmux thì chưa kiểm
4. Overlay `.dtbo` trong layer (node dưới `&i2c2`, `compatible` riêng, pinctrl I2C2 + INT mode 7 input), nạp qua `fdtoverlays` của extlinux (U-Boot 2025.01 hỗ trợ, `pxe_utils.c`). Kiểm mode chân bằng `devmem 0x44E10000+offset` (tự tra offset trong TRM)
5. `i2cget` đọc `WHO_AM_I` (0x75), ghi giá trị thật (clone có thể khác 0x68)
6. Logic analyzer bắt SCL/SDA khi `i2cget`, lưu ảnh cho báo cáo
7. Kernel RT: `poky/meta/recipes-kernel/linux/linux-yocto-rt_6.12.bb` cùng 6.12.47 (`KBRANCH v6.12/standard/preempt-rt/base`) nhưng `COMPATIBLE_MACHINE` chỉ có qemu, nên cần `.bbappend` trong layer (đây là dòng `.bbappend` kernel dời từ tuần 2) và `PREFERRED_PROVIDER_virtual/kernel`. **Trước khi build RT: lưu ảnh + `zImage` + dtb non-RT hiện tại** làm mốc benchmark (xem CLAUDE.md)
8. Xác nhận `inv_mpu6050` không bật: kernel thường đã kiểm, không có `CONFIG_INV_MPU6050*` và không có `CONFIG_IIO`. Kernel RT phải kiểm lại
9. Git tag `bringup-done`

Xong tuần 3 khi: màn hiện hình, `i2cdetect` thấy MPU6050, có ảnh logic analyzer xung I2C, kernel RT boot được, tag đã đặt.

**Dữ kiện chỉ người làm có, phải hỏi trước**
- MPU6050 đã nối chưa, chân INT nối vào chân nào, AD0 nối GND hay VCC
- Có logic analyzer chưa (loại nào), màn 7" đang ở đâu, cáp micro-HDMI
- Muốn làm HDMI trước hay MPU6050 trước (spec khuyên HDMI sớm vì rủi ro tilcdc/tda998x)

**Bẫy đã gặp, nhớ để không lặp lại**
- `dd` của busybox không có `conv=fsync`; lệnh lỗi thì không ghi gì, phải đọc lại để kiểm
- zsh: glob không khớp hủy cả lệnh; `~` không mở rộng sau `if=`; `=====` bị hiểu là `=cmd`; `ls` là alias `eza` có icon nên đừng `$(ls ...)`; dấu nhắc đổi thành `❯❯`/`quote>` là còn nháy chưa đóng, lệnh chưa chạy
- `picocom` cũ giữ khóa cổng (`cannot lock /dev/ttyUSB0`): thoát bằng `Ctrl-A Ctrl-X`
- `ssh ... | grep` che dấu nhắc của phiên; dùng `ssh ... 'lệnh'` để phiên tự đóng
- Đồng hồ bo không có pin: lúc boot giờ là khoảng lúc build ảnh, `systemd-timesyncd` chỉnh đúng khi có mạng, nên các dòng journal trước lúc đó mang giờ sai. Tính khoảng thời gian thì dùng `journalctl -o short-monotonic`. Journal ở RAM, mất khi tắt nguồn
- `git commit` lấy cả những gì đã stage từ trước: xem `git diff --cached --name-status` trước mỗi commit

---

## Tuần 2 — image riêng, SSH bằng khóa, boot flow (2026-10-15 đến 10-20) — XONG 2026-10-20

Ba tiêu chí của spec đạt trên ảnh cuối (`rootfs-20261020043011.wic`): boot + SSH bằng khóa, layer build độc lập, `boot-flow.md` từ log thật. Dòng `.bbappend` kernel trong checklist chuyển sang tuần 3 (xem ROADMAP): chưa có cấu hình nào cần giữ riêng, `.bbappend` rỗng chỉ tốn một vòng build.

**Đã làm**
- `projectb-image` (kế thừa `core-image-minimal`): `ssh-server-openssh`, `i2c-tools`, `gdbserver`, `stress-ng`, `rt-tests`
- `projectb-ssh-keys`: khóa công khai `id_ed25519_bbb` vào `${ROOT_HOME}/.ssh/authorized_keys` (`/home/root` với sysvinit, `/root` với systemd) (0700/0600, chủ root). Khóa riêng ở laptop
- `projectb-phy-retry`: bind lại PHY, tối đa 10 lần cách nhau 5 giây. Bản đầu là script sysvinit (S03); từ 2026-10-16 là `projectb-phy-retry.service` (systemd)
- Ghi serial log từ lúc cấp nguồn (`docs/boot-serial-*.log`) và viết `docs/boot-flow.md` từ log đó
- Thử USB gadget NCM (`g_ncm`): nạp được, `usb0` lên link nhưng không truyền dữ liệu (BBB `rx 77, tx 5`, laptop `RX 0`). Chưa điều tra, đường này bỏ
- Sửa `meta-projectb/conf/layer.conf`: `LAYERDEPENDS = "core yoctobsp"`. Chưa build kiểm, nên chưa commit

**Kết quả đo được**
- Image boot, SSH vào bằng khóa, đã đối chiếu vân tay host key trên serial với vân tay `ssh` báo
- Có hai đường boot trên cùng thẻ SD. Không giữ S2: SPL + U-Boot 2019.04 trên eMMC (`bootdelay 0`) nạp kernel từ thẻ SD qua `extlinux`. Giữ S2: SPL + U-Boot 2025.01 trên thẻ SD. Chi tiết trong `boot-flow.md`
- Kernel và dtb là cùng file với tuần 1 (đóng dấu 2026-09-27), không build lại kernel
- PHY LAN8710: probe lỗi `-5` (`-EIO`, PHY không ACK MDIO) ở mọi lần boot. Lúc `ifup` có lần kernel gắn được PHY, có lần không. Đã bác lần lượt các giả thuyết về thời gian reset, thời gian từ lúc boot, và bootloader nào chạy trước; nguyên nhân gốc chưa biết
- Với `projectb-phy-retry`: 5/5 lần boot đường 1 có `192.168.50.182` mà không can thiệp, số lần thử 3, 1, 1, 1, 0. Mẫu nhỏ, một bo, chưa thử đường 2
- Kernel config: `CONFIG_DEVMEM=y`, `STRICT_DEVMEM` không bật. Build: 223 task (118 sstate miss) lần đầu, 17 task khi thêm module, 35 task khi thêm ssh-keys

**Việc tiếp theo (theo thứ tự)**
1. ~~Build sạch~~ Xong 2026-10-16: `setup-build.sh build-clean` (sstate trống, 0 cache, conf từ template), `Tasks Summary: Attempted 4651 ... all succeeded`, 927 recipe parse 0 lỗi. So với bản `build`: manifest giống, `zImage` và `am335x-boneblack.dtb` cùng `sha256`, danh sách file rootfs giống. `LAYERDEPENDS = "core yoctobsp"` chạy được. `downloads/` dùng chung; recipe mới không tải gì
2. ~~extlinux/cmdline trong layer~~ Xong 2026-10-16, kiểm trên bo: `wic/projectb.wks` + `wic/projectb-extlinux.conf` (`bootloader --configfile`, `root=/dev/mmcblk0p2`, dòng `fdtoverlays` đang comment cho tuần 3), `WKS_FILE = "projectb.wks"` trong `projectb-image.bb`. Trên bo: `/boot/extlinux/extlinux.conf` đúng file của layer, `/proc/cmdline` = `root=/dev/mmcblk0p2 rootwait console=ttyS0,115200`
3. ~~Busybox `devmem`~~ Xong 2026-10-16, kiểm trên bo: `busybox_%.bbappend` + `devmem.cfg` (`CONFIG_DEVMEM=y`); `devmem 0x44E10000 32` đọc được `0x4E8B0100` (thanh ghi revision của control module, chưa đối chiếu giá trị với TRM)
4. Chuyển sang systemd, boot được 2026-10-16, còn mở một mục: `INIT_MANAGER = "systemd"` trong template `local.conf`; `projectb-phy-retry` viết lại thành unit (`.service` + script foreground, journal thay cho file log). Trên bo (log `boot-serial-2026-10-16-extlinux-d1.log`): systemd 257.6, `systemctl --failed` rỗng, `eth0` có `192.168.50.182/24` do networkd (không còn `init-ifupdown`), unit `enabled`, `/root/.ssh` có (0700). `is-system-running` ra `starting` khi gõ ngay sau đăng nhập (~40 s sau khi systemd chạy), lần kiểm sau ra `running`, `systemctl list-jobs` rỗng: chỉ là boot chưa xong lúc đó, chưa biết unit nào chậm (bo chưa có `systemd-analyze`, nằm trong gói `systemd-analyze`). `/var/log/journal` không có: journal mất khi tắt nguồn. SSH 2026-10-20: lần đầu `ssh -v` ra `Authenticated ... using "none"`, không dùng khóa, vì `allow-empty-password` đặt `PermitEmptyPasswords yes` và root có `root::`; vậy các lần "SSH được" trước đó chưa chứng minh khóa. Sửa tay `PermitEmptyPasswords no` trên bo thì ra `Offering public key ... Authenticated using "publickey"`, vân tay `SHA256:vu299Wkf…` trùng khóa trong layer và `authorized_keys` trong rootfs. Template bỏ `allow-empty-password`; build lại chỉ rootfs/wic (kernel nguyên `fcbb876b…`). Trên ảnh cuối, không sửa tay gì: `Offering public key … id_ed25519_bbb`, `Authenticated … using "publickey"`, `whoami` ra `root` (host key mới `SHA256:eXTaeZLl…` trùng vân tay trên serial). Phép thử âm `ssh -F /dev/null -o PubkeyAuthentication=no -o PreferredAuthentications=none,password` ra `Permission denied (publickey,password)`, kể cả mật khẩu rỗng. Serial `login: root` vẫn vào không mật khẩu (`boot-serial-2026-10-20-final.log`). Chưa xong: lần boot này PHY đã gắn trước khi unit chạy nên đường "bind lại" của unit chưa được thử. Kernel bị build lại (dự đoán trước là không): `.config` giống hệt tuần 1 nhưng `zImage` thành `fcbb876b…` (trước `9204960d…`); dtb vẫn `23abc1f3…`
5. ~~Vô hiệu bootloader trên eMMC~~ Xong 2026-10-16, xem `docs/emmc-bootloader-disable.md`: sao lưu có kiểm checksum, ghi 512 byte 0 vào sector 256, boot không S2 ra U-Boot 2025.01 (`docs/boot-serial-2026-10-16-emmc-off.log`). Chưa thử khôi phục thật
6. Dọn: ~~bỏ `allow-empty-password`~~ xong 2026-10-20 (giữ `empty-root-password` cho serial); còn quyết định có giữ `g_ncm` không

**Câu hỏi còn mở**
- Vì sao PHY không ACK MDIO lúc probe đầu (cần đo MDIO/nRST bằng logic analyzer)
- Vì sao gadget NCM không truyền dữ liệu
- `.wic` khác giữa hai build tuần 1 (chưa điều tra)
- Vì sao `zImage` khác dù `.config`, `System.map` (tên và kiểu symbol) và chuỗi version giống hệt: chỗ lệch đầu tiên là ngay sau `kernel_config_data_end`, cấu hình nhúng nén lớn hơn 92 byte (34068 so với 33976) trong khi nội dung giải nén giống hệt. Giả thuyết chưa kiểm chứng: `gzip` mà kernel gọi khi build là `pigz-native` ở bản này, còn bản tuần 1 là `gzip` của host (kernel không khai báo phụ thuộc, nên phụ thuộc thứ tự task); dấu thời gian stamp khớp giả thuyết ở bản mới, riêng bản cũ còn một điểm chưa khớp

**Commit**: `b3fccf4` (image, recipes), `96c352c` (boot-flow, log), `5d90500` (wic, devmem), `0615924` (systemd, SSH). Tài liệu và log đợt sau ở commit liền sau.

---

## Tuần 1 — kiểm chứng build từ đầu (2026-10-06)

**Đã làm**
- Tạo `scripts/setup-build.sh` và `scripts/templates/` để sinh `conf/` từ repo, thay vì copy từ build cũ
- Build `build-clean` từ đầu: `downloads/` trống (`DL_DIR` ghi đè cho bài test), sstate trống

**Kết quả đo được**
- `Sstate summary: Wanted 2057 Local 0`, `Tasks Summary: Attempted 4416 ... all succeeded`
- 4 cảnh báo fetch từ URL gốc, BitBake lấy từ mirror và vẫn qua
- `zImage` và `am335x-boneblack.dtb` có sha256 giống build cũ
- `.wic` có sha256 khác build cũ (chưa điều tra)

**Việc tiếp theo**
- Tuần 2 (chưa bắt đầu)

**Commit**: `0b39da9` (`scripts/setup-build.sh`, `scripts/templates/`, README)

---

## Tuần 1 — hoàn thành (2026-10-05)

**Đã làm**
- Dựng môi trường build Docker (Ubuntu 24.04, user uid khớp host, locale UTF-8)
- Build `core-image-minimal` cho beaglebone-yocto, có sửa lỗi `pseudo`/`openat2` bằng `.bbappend`
- Ghi image xuống thẻ SD bằng `scripts/flash-verified.sh`, xác minh 70/70 khối khớp SHA256
- Boot trên BBB tới login prompt; đăng nhập `root`; kernel `6.12.47-yocto-standard armv7l`

**Kết quả đo được**
- Boot log qua serial: đầy đủ từ ROM đến login
- Checksum 70/70 khối khớp, kiểm tra cả khối đầu và khối cuối

**Việc tiếp theo**
- Tuần 2: layer riêng, image recipe thêm gói dev, `.bbappend` kernel, U-Boot/device tree, systemd

**Câu hỏi còn mở**
- `.wic` của build sạch khác `.wic` của build cũ (sha256 khác). `zImage` và `dtb` giống hệt. Chưa biết khác ở đâu (timestamp? rootfs?)

**Commit**: `995a811` (cùng các commit `e7c4186`, `978563e`, `d2b3e9d`)

---

## Mẫu cho các session sau

## Tuần X — (ngày)

**Đã làm**
-

**Kết quả đo được**
-

**Việc tiếp theo**
-

**Câu hỏi còn mở**
-

**Commit**:
