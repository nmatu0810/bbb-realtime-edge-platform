# Nhật ký tiến độ

Mỗi session kết thúc ghi thêm một mục ở đầu file (mới nhất lên trên).
Mục gồm: đã làm, kết quả đo được, việc tiếp theo, câu hỏi còn mở, commit tương ứng.

---

## GĐ2 — bàn giao, chưa bắt đầu (ghi 2026-10-30)

Đọc mục này trước khi bắt đầu GĐ2 (tuần 9) ở session mới. GĐ1 xong; tag `v1.0-chuyennganh` (annotated, trên `9ec9884`) đã push 2026-11-01. Báo cáo ĐACN2 và đề cương sửa theo sản phẩm ở `docs/report/` (README trong đó: dựng lại từ `content.py`, mở bằng Word để cập nhật mục lục; mẫu của khoa và đề cương bản nộp nằm ở `docs/`, không commit). Còn: Tus đọc lại báo cáo, xuất PDF từ Word, nộp trước 2026-11-25. Chi tiết tuần 8 ở mục ngay dưới; tóm tắt kiến trúc cho báo cáo: `docs/architecture.md`. Ghi chú kiến thức tuần 8: `~/workspace/VKU/DoAn/week8-knowledge-summary.md`. Hạn nộp chuyên ngành 2026-11-25.

**Hệ thống đang chạy (sau tuần 8)**
- Thẻ ghi ảnh nộp `projectb-image-beaglebone-yocto.rootfs-20261030092552.wic` (mốc `~/projectb-baseline/rt-submit-20261030/`, `SHA256SUMS`): kernel `6.12.47-rt6-yocto-preempt-rt` không debug (zImage `818fb8a7…`, trùng mốc `rt-20261022`), overlay 400 kHz `51b9e80e…`, `mpu6050.ko` `a38b5ad7…` trong `/lib/modules/…/updates/`, `mpu6050-test`, `mpu6050-boot-check`. extlinux một nhãn (`Yocto`) từ layer; không còn nhãn `NonRT`, `zImage-nonrt` (mốc ở `nonrt-20261030/`)
- Boot: udev tự nạp module theo modalias, `/dev/mpu6050` 0600 root (`90-mpu6050.rules`), unit `mpu6050-boot-check` tự chạy khi thiết bị xuất hiện; `journalctl -u mpu6050-boot-check` kết thúc bằng "KẾT QUẢ: ĐẠT"
- `/tmp` trên bo có script (`mpu-check`, `mpu-stress`, `mpu-trace`, `mpu-sched`, `mpu-fault`, `mpu-lag`, `demo.sh`), `strace`, `/tmp/mpu6050-test` → `/usr/bin/mpu6050-test`; mất khi reboot. Host key SSH đổi mỗi lần ghi thẻ: Tus chạy `ssh-keygen -R 192.168.50.182`, Claude so fingerprint (`ssh-keyscan | ssh-keygen -lf -`) rồi `StrictHostKeyChecking=accept-new`
- `build-clean/conf/local.conf` = template, giờ `PROJECTB_KERNEL_DEBUG = "0"` (RT không debug). Sửa driver mà cần lockdep: đặt "1", build lại kernel (sstate có) + module, nạp bằng `KO=` hoặc ghi thẻ `rt-debug-20261030/`
- Chưa commit gì của tuần 8 (chờ Tus)

**Vòng sửa–thử**: như tuần 7. `scripts/ko-push.sh` vẫn dùng được (module trong ảnh bị thay đến lần boot sau). Thêm `scripts/mpu-fault.sh` (watch/unbind/held), `scripts/mpu-lag.sh` (độ trễ cạnh → ts → đọc count, NACK), `scripts/demo.sh`.

**GĐ2 theo kế hoạch (ROADMAP, spec)**
- Tuần 9–11: PREEMPT_RT + cyclictest (chế độ Benchmark, 1 core + SCHED_FIFO), jitter và độ trễ IRQ → userspace; benchmark runner (`fork/exec/waitpid`); machine/distro conf riêng; watchdog handoff U-Boot → `omap_wdt` + `RuntimeWatchdogSec`. Đầu tuần 9: san bằng `.config` RT/non-RT (dời từ tuần 7) trước khi so độ trễ
- Tuần 12–14: `sensord` C++17 (SCHED_FIFO, poll trên `/dev/mpu6050`, SPSC ring, shm + mq, chạy không root), HMI LVGL, AI đơn giản
- Tuần 15–16: tài liệu tốt nghiệp, OTA trên giấy

**Điểm cần quyết đầu GĐ2 (phân tích rồi khuyến nghị)**
- Kernel đo benchmark: RT không debug của ảnh nộp và non-RT sau khi san bằng `.config`. Mốc non-RT `nonrt-20261030` chưa san bằng
- Tách ảnh: `projectb-image` (thiết bị) và `projectb-image-dev` (công cụ, root không mật khẩu) như quyết định tuần 2; `EXTRA_IMAGE_FEATURES` vẫn `empty-root-password allow-root-login`
- `sensord` không chạy root: thêm user/group (`useradd` class), rule udev `GROUP`, `MODE 0640`
- `MPU6050_TS_LAG_MIN_NS` là hằng theo cấu hình kernel (RT+lockdep 40 µs, còn lại 0). San bằng `.config` hay đổi kernel thì đo lại bằng `mpu-lag.sh` (NACK phải 0, đọc count ≥ 60 µs sau cạnh)
- Đo độ trễ IRQ → userspace dùng `ts_ns` (lấy ở nửa trên, trên RT là trong thread `irq/26`): trễ thật so với cạnh phải cộng irq26 → irq50 (6–18 µs, đo bằng ftrace) hoặc đo bằng LA
- Tham số thử (`batch`, `oflow_reset`, `stall_ms`) và debugfs vẫn trong driver (ghi "test:" trong mô tả); `mpu-check oflow` cần chúng

**Giới hạn mang sang**
- RT: cạnh INT có lúc trễ vài ms tới nửa trên (khóa BH của kworker), ts của cạnh đó trễ; 1000 Hz trên RT debug còn ~0.03% mất và khung hỏng lọt. RT không debug và non-RT 1000 Hz sạch
- `dropped` không tính mẫu lúc chip mất nguồn (không có cạnh); khoảng mất chỉ thấy qua ts (Tus duyệt giữ nghĩa này, 2026-10-30)
- Đổi cấu hình lỗi giữa chừng (bus hỏng) thì stream dừng tới khi watchdog bật lại; mẫu trong lúc dừng không tính `dropped` (như lúc đóng)
- Ghi sysfs `accel_range`/`sample_rate` trả `EREMOTEIO` lúc bus hỏng nhưng chip có thể đã nhận một nửa cấu hình (driver cố ghi lại cấu hình cũ, cũng lỗi). Có fd mở thì watchdog ghi lại toàn bộ khi bus về. Không fd mở thì chip giữ cấu hình nửa vời tới lần ghi sysfs hay probe sau: `open` (`mpu6050_start`) chỉ reset FIFO, không ghi lại tốc độ/dải. Sửa ở GĐ2 nếu cần: `start` gọi `mpu6050_configure`, đổi lại open chậm thêm ~10 giao dịch

**Bẫy mới**
- Busybox: không có `pgrep`, `head -c`, `od -A/-t/--endian` (dùng `hexdump -e '1/1 "%02x"'`); vòng chờ phía laptop dùng `pgrep` sẽ không bao giờ thoát
- Lệnh nền trong `sh` không tương tác bỏ qua SIGINT: `dd` nền phải `kill -TERM`; app tự bắt INT nên không gặp
- `kfree` ở cuối hàm là tail call: event `kmem:kfree` ghi call_site là hàm gọi (`release_nodes`), không phải `mpu6050_*`; ghép theo `ptr`
- Hàm static nhỏ bị inline không có trong `available_filter_functions` (`mpu6050_free`)
- Sắp xếp trong awk của busybox là O(n²) với n = 30000: rất chậm, dùng `sort -n`
- Log trên `/tmp` của bo mất khi ghi thẻ: chép về `docs/logs-*` trước khi đưa lệnh ghi thẻ
- Unit `BindsTo=dev-mpu6050.device` chạy lại sau mỗi unbind/bind: tự kiểm chạy lại, bình thường

**Dữ kiện chỉ người làm có**: giảng viên có yêu cầu kernel cụ thể cho demo không (mặc định đang là RT không debug); trường có cho dùng cùng đề tài cho hai đồ án không (spec).

---

## Tuần 8 — đường lỗi, module vào ảnh, tài liệu, demo (2026-10-30) — XONG 2026-10-30 (trừ tag)

Mốc kiểm tuần 8 đạt: bus hỏng khi stream (rút SDA, mất nguồn chip, ghi sysfs lúc hỏng) không treo, log có giới hạn tốc độ, `dropped` khớp thời gian hỏng × nhịp, tự hồi không rmmod; unbind khi fd mở và khi stream 20 vòng sạch (kref, `ENODEV`), slab không tăng; ảnh mới có driver, app, overlay 400 kHz, driver tự nạp, `/dev/mpu6050` 0600 root, tự kiểm lúc boot không thao tác tay (journal "ĐẠT"); ghi thẻ bằng `flash-verified.sh`, hồi quy rút gọn trên kernel nộp đạt; `docs/architecture.md`; demo diễn tập đủ 6 bước. Lockdep 1 sau mọi bước trên RT debug. Chưa: tag `v1.0-chuyennganh` (Tus duyệt).

**Quyết định (đầu tuần, theo khuyến nghị)**
- Kernel ảnh nộp: RT không debug (Tus: chưa biết giảng viên yêu cầu gì, theo mặc định). Đường lỗi làm trước trên RT debug để có lockdep
- extlinux trong layer giữ một nhãn; non-RT không vào wic (cần recipe chép kernel thứ hai, không đáng cho GĐ1)
- Tham số thử, debugfs: giữ. Khung hỏng 1000 Hz RT debug: ghi là giới hạn, cam kết 500 Hz
- Quyền `/dev/mpu6050`: 0600 root ghi tường minh trong rule udev (GĐ1 chỉ root đọc); nhóm cho `sensord` ở GĐ2
- Test không thao tác tay: unit oneshot không `[Install]`, udev kéo lên bằng `SYSTEMD_WANTS` khi `/dev/mpu6050` xuất hiện (không làm chậm `multi-user.target`, chạy lại sau bind)
- Chip mất nguồn: `dropped` không tăng (không có mẫu); Tus duyệt giữ nghĩa `dropped`

**Bước 1: kref, watchdog (RT debug, `.ko` `3a22d99c`, sau đó `2ed09f00`)**
- `st` cấp bằng `kzalloc` + kref; thiết bị giữ một tham chiếu (devm action đầu tiên, chạy sau cùng), mỗi fd một. `remove`: `misc_deregister` (misc_open gọi `open` dưới `misc_mtx` nên sau đó không còn open nào), `gone`, dừng nguồn, `cancel_delayed_work_sync`, `wake_up` với `EPOLLHUP`. `read` trả nốt bản ghi rồi `ENODEV`, `poll` `HUP|ERR`, ioctl `ENODEV`. App thoát mã 1 khi `ENODEV`
- Watchdog `delayed_work` 1 s, chỉ khi có fd mở (`active`): không cạnh nào trong một nhịp (nửa trên đếm `n_edges`) hoặc `streaming` false (bật lại lỗi) → `mpu6050_recover`: `WHO_AM_I` (lỗi thì thôi, không ghi gì, không log), `DEVICE_RESET`, cấu hình, start. Lấy `cfg_lock`, nên `cancel_delayed_work_sync` gọi sau khi nhả `cfg_lock` (release, remove). Lỗi khởi tạo lại chỉ báo lần đầu và lúc hồi (bản đầu báo mỗi giây). debugfs `recover`
- Thứ tự khóa, uapi, sysfs, nghĩa `dropped` không đổi; thread IRQ vẫn không lấy khóa

**Bước 2: unbind (`scripts/mpu-fault.sh`)**
- `held` (fd mở không đọc, unbind, đọc bằng fd cũ): 110 bản ghi còn lại rồi `No such device`; giải phóng ở `mpu6050_release`. `cat` chặn trong `read()`: dậy sau 0.12 s, `read error: No such device`
- `unbind 20` lúc app đang đọc: 20/20 app thoát mã 1, bind lại đọc 50 bản ghi. Event `kmem:kmalloc/kfree` ghép theo `ptr`: `st` (8840 B) cấp 20, giải phóng 19 + 1 đang sống (bản trước vòng đầu giải phóng ở vòng 1); 18 lần từ `release_nodes`, 1 từ `mpu6050_release`. Slab 26.80 → 26.82 MB. dmesg không bug/warn. Lockdep 1
- ftrace function trên `mpu6050_free` không được (bị inline); `kfree` tail call nên call_site là hàm gọi

**Bước 3: bus hỏng (Tus rút/cắm dây, 100 Hz)**
- Lượt A, rút SDA rồi ghi `sample_rate` 200: lúc rút ngắt vẫn ~100/s, `dropped` tăng ~100/s sau khi hàng đợi cạnh đầy; `FIFO_COUNT read failed: -121` ≤ 10 dòng/5 s ("~492 callbacks suppressed"). Ghi sysfs: `Remote I/O error` sau 0.15 s, `sample_rate` đọc lại 100; stream dừng (ngắt 0), watchdog thử mỗi giây. Cắm lại: `chip reinitialised`, `recover` 1, stream tiếp. App: 37 lần "1 s without data", 20141 bản ghi, temp 34.2 °C, |a| ≈ 1.02 g
- Lượt B, rút SDA không ghi sysfs: khoảng ts lớn nhất 17.92 s (× 100 = 1792), `dropped` +1795. Δirq 15832, bản ghi + `dropped` 15857 (dư −25 = mẫu không cạnh quanh lúc reset). Lúc cắm lại 2 lần `FIFO count 10/17 not a multiple of 8, reset`
- Lượt C, rút VCC module (~14.7 s), ghi mọi bản ghi bằng `dd` (`REC=1`): `no INT for 1000 ms, reinit chip`, 12 lần thử lỗi, `chip reinitialised`. 41641 bản ghi, khoảng ts lớn nhất 14.665 s, `dropped` không đổi, 0 bản ghi temp ngoài 15..60 °C (không lệch byte); |a| lệch khỏi 1 g ở 40–60 s và 75–76 s (lúc tay chạm module rút/cắm dây). 72 cặp dt ~16/4 ms trong 430 s: cạnh trễ trên RT (đã biết)
- Log của ba lượt nằm trên thẻ cũ, mất khi ghi thẻ; số ở trên chép từ output trong session

**Bước 4: ảnh nộp**
- Recipe mới `recipes-apps/mpu6050-boot-check` (script, unit, `90-mpu6050.rules`; `RDEPENDS` `mpu6050-test kernel-module-mpu6050`), thêm vào `projectb-image`. Template + conf: `PROJECTB_KERNEL_DEBUG = "0"`. Kernel lấy từ sstate, zImage `818fb8a7…` = mốc `rt-20261022`
- Lần build đầu session phải build lại kernel RT debug thật (~15 phút): tuần 7 đổi `PREFERRED_PROVIDER` qua lại
- Ghi thẻ lần 1 (`rootfs-20261030084228`, đầu đọc `aaaa:8816`, không phải Genesys): module tự nạp, `/dev/mpu6050` `crw------- root:root`, unit tự kiểm "ĐẠT" 19 s sau boot. Nhưng dmesg có `FIFO_COUNT read failed: -121`

**Bước 5: NACK trên RT không debug (`scripts/mpu-lag.sh`)**
- ftrace irq26 (hardirq bank GPIO ≈ cạnh) → irq50 (ts) → `i2c_write [72]`/`i2c_result`: ts trễ cạnh 6–18 µs (RT debug ≥ 40). Driver trừ 40 µs khi `CONFIG_PREEMPT_RT` nên đọc count ở 31–37 µs sau cạnh, trong xung INT: NACK 79/6063 (500 Hz), 1349/15679 (1000 Hz), 1000 Hz `misaligned` +4, `dropped` +8
- Sửa: trừ 40 µs chỉ khi `CONFIG_PREEMPT_RT && CONFIG_LOCKDEP` (trễ đó do lockdep). A/B cùng lần boot, `.ko` mới `a38b5ad7`: đọc count 70–81 µs sau cạnh, NACK 0/6000 (500 Hz) và 0/15000 (1000 Hz), `misaligned` 0, `dropped` 0. Lượt B đầu tiên trace chỉ 48 sự kiện (`dd` thoát sớm hoặc trace tắt, chưa rõ, không lặp lại); script giờ in dòng trạng thái của `dd`
- Bản trên RT debug không đổi hành vi (vẫn trừ 40 µs); chưa chạy lại trên RT debug với `a38b5ad7` nguồn (chỉ khác hằng số này so với `2ed09f00`)

**Bước 6: hồi quy rút gọn trên kernel nộp (`a38b5ad7` từ `/tmp`, sau đó trong ảnh)**
- selftest 0 FAIL; `sweep` 100–1000 Hz `dropped` 0, `misaligned` 0, CPU rảnh 90–98%; `oflow 1000`: +107, dư 0; `cfg` 100 → 200 Hz, ±2 → ±4 g; `load 600`: stress-ng 3/3 passed, 60568 bản ghi = irq, `dropped` 0, khoảng ts lớn nhất 10.2 ms, 0 giây > 20 ms
- Ảnh `rootfs-20261030092552` (mốc `rt-submit-20261030`), ghi thẻ lần 2: unit "ĐẠT", 0 NACK; `mpu-lag 1000 15000` trên module của ảnh: NACK 0, đọc count 71–81 µs

**Bước 7: tài liệu, demo**
- `docs/architecture.md`: tầng, khởi động không thao tác tay, hợp đồng, luồng dữ liệu, khóa, vòng đời và đường lỗi, số đo (GĐ1 + kernel nộp), giới hạn, ảnh LA chọn cho báo cáo. Log: `docs/logs-2026-10-30/` (journal tự kiểm, nghiêng, rút SDA, lag A/B, lag 1000 Hz, tóm tắt `load 600`)
- `scripts/demo.sh` 6 bước. Diễn tập trên thẻ mới (Claude chạy, Tus nghiêng/rút dây): 1–2 kernel, overlay 400000 Hz, tự kiểm `Result=success`; 3 nghiêng: `ax` −1.51..+0.36, `ay` −0.64..+1.24, `az` −0.67..+0.95 g, `dropped` 0; 4 101 → 505/s, ±8 g, `dropped` 0; 5 rút SDA 22.1 s → `dropped` +2210; 6 unbind → app mã 1, bind lại selftest 0 FAIL, unit chạy lại "ĐẠT". Sửa sau lượt chạy thử: `od --endian` → `hexdump`, `systemctl status` (mốc giờ lệch vì chưa có NTP lúc boot) → `systemctl show`

**Commit**: chưa (chờ Tus).

---

## Tuần 8 — bàn giao (ghi 2026-10-30)

Đọc mục này trước khi bắt đầu tuần 8 ở session mới. Tuần 1–7 xong; chi tiết tuần 7 ở mục ngay dưới. Ghi chú kiến thức tuần 7: `~/workspace/VKU/DoAn/week7-knowledge-summary.md`. Hạn nộp 2026-11-25.

**Hệ thống đang chạy (sau tuần 7)**
- Thẻ vẫn là ảnh tuần 4 (`rootfs-20261023085915.wic`), nhưng `/boot` đã sửa tay: `overlays/mpu6050.dtbo` = bản 400 kHz (`51b9e80e…`, bản 100 kHz giữ ở `mpu6050-100k.dtbo`), `zImage-nonrt` (`181dcc6d…`), `extlinux/extlinux.conf` có hai nhãn `Yocto` (RT debug) và `NonRT`, `default Yocto` (bản gốc ở `extlinux.conf.rt`). Rootfs có thêm `/lib/modules/6.12.47-yocto-standard` (7 module như ảnh RT + `depmod`). Đổi kernel: `sed -i 's/^default .*/default NonRT/' /boot/extlinux/extlinux.conf; sync; reboot` (phải hỏi Tus trước khi reboot)
- Đang boot RT debug (`6.12.47-rt6-yocto-preempt-rt-debug`). `/tmp`: `mpu6050.ko` chốt `8c1f95cf…` (đang nạp), app, strace, script. Sau boot: `scripts/bo-setup.sh` (chép app/strace/script, nạp ko)
- `build-clean/conf/local.conf` về như template (RT debug). Module RT debug trong sstate: `packages-split` không có `.ko` khi bitbake lấy từ setscene, dùng `KO=~/projectb-baseline/rt-debug-20261030/mpu6050-final.ko scripts/ko-push.sh` hoặc `bitbake -C compile mpu6050-mod`
- Mốc mới: `~/projectb-baseline/rt-debug-20261030/` (wic, zImage `a9872bda…`, dtb, modules, manifest, kernel.config, dtbo 100k/400k, `mpu6050.ko` trước khi có chờ 60 µs, `mpu6050-final.ko` `8c1f95cf…`), `~/projectb-baseline/nonrt-20261030/` (wic `rootfs-20261030031535`, zImage `181dcc6d…`, `nonrt-mods.tgz` 7 module, kernel.config, `mpu6050-final.ko` `92f38033…`). Cả hai có `SHA256SUMS`
- Chưa commit gì của tuần 7 (chờ Tus)

**Vòng sửa–thử (tuần 7 thêm script)**
- Build như tuần 6. Chép + nạp: `scripts/ko-push.sh` (tự lấy hậu tố từ `uname -r` trên bo, so hash). Sau boot: `scripts/bo-setup.sh`
- Bộ kiểm: như tuần 6 + `mpu-check.sh one <hz> [s]`, `rec <hz> <n>` (ra `/tmp/rec.bin`, `/tmp/rec.txt`), `oflow <ms> [reset]`. Phân tích trên laptop: `scripts/mpu-ts.py rec.bin <chu kỳ ms>` (khoảng ts, khung lệch byte), `scripts/la-run.sh <hz> <n> <tên> [12m] [24m]` (ghi + chụp LA + `la-fifo.py`), `scripts/mpu-sched.sh` (ai giữ CPU khi cạnh trễ, trên bo)
- debugfs `/sys/kernel/debug/mpu6050/`: anchored, unanchored, extra, fifo_reset, misaligned, settle, batch. Tham số module chỉ để thử: `batch`, `oflow_reset`, `stall_ms`
- Lệnh nền trên bo phải `nohup … > /dev/null 2>&1 &`; chờ bằng một lệnh nền phía laptop

**Tuần 8 theo kế hoạch (ROADMAP)**: đường lỗi (I2C lỗi, unbind khi stream), module vào ảnh, tài liệu kiến trúc, ảnh LA, log, diễn tập demo, tag `v1.0-chuyennganh`. Mốc: thẻ ghi mới, driver tự nạp, test chạy không thao tác tay.

**Điểm cần quyết ở đầu tuần 8 (phân tích rồi khuyến nghị)**
- Kernel trong ảnh nộp: RT không debug, non-RT, hay cả hai nhãn extlinux như bo hiện tại. Lô và khoảng chờ được chỉnh trên RT debug và non-RT; RT không debug chưa đo (hằng số `MPU6050_READ_START_NS` 450 µs đo trên RT debug). Nếu chọn RT không debug phải chạy lại sweep 1000 Hz + `misaligned`
- Đưa overlay 400 kHz và các nhãn extlinux vào layer (hiện chỉ sửa tay trên thẻ). `zImage-nonrt` trong wic thì cần một recipe/wks chép thêm kernel thứ hai, hoặc bỏ
- Đường lỗi I2C: bus hỏng hẳn (rút dây SDA/SCL, chip mất nguồn): thread hiện giữ cạnh khi đọc count lỗi, reset khi đọc khung lỗi, `reset_pending` khi reset lỗi. Phải kiểm: không treo, log có giới hạn tốc độ, `dropped` tăng đúng, cắm lại thì tự hồi, ghi sysfs lúc hỏng. Treo `omap_i2c` 1 s (`OMAP_I2C_TIMEOUT`) làm `disable_irq` trong store chờ theo
- Unbind khi còn fd mở: kref (tồn từ tuần 5)
- Tham số thử (`batch`, `oflow_reset`, `stall_ms`) và debugfs: giữ trong bản nộp (ghi rõ là để thử) hay bỏ/ẩn sau `#ifdef`
- Khung hỏng lọt ra ở 1000 Hz trên RT debug (4–8 trên 30000, lô bị trễ sang lúc chip ghi): kiểm temp/|a| trong driver là heuristic; có thể đọc lại count sau lô để phát hiện sớm hơn (thêm một giao dịch). Hoặc ghi là giới hạn, tốc độ cam kết 500 Hz
- Test không thao tác tay: unit systemd chạy selftest lúc boot hay script demo; app `-t` đã có mã thoát = số FAIL

**Giới hạn đã biết, mang sang**
- 1000 Hz trên RT debug: ~0.03% `dropped`, 2–4 lần `misaligned`/30 s, vài khung hỏng lọt trước khi phát hiện. Non-RT 1000 Hz sạch (30000/30000). Tốc độ cam kết cho mốc `dropped` = 0: 500 Hz (sạch trên cả hai kernel)
- RT: cạnh INT tới nửa trên trễ tới vài ms khi một kworker giữ khóa BH (thread `irq/26` ở `D`, kworker được nâng prio 49). Không mất mẫu (FIFO giữ), ts của cạnh đó trễ: `load 1800` khoảng ts lớn nhất 26.2 ms trên RT debug, 10.6 ms trên non-RT
- Chip NACK byte thanh ghi nếu bị đọc < ~50 µs sau cạnh INT (trong xung). Driver chờ tới 60 µs sau cạnh (ước theo ts, trừ độ trễ ts tối thiểu 40 µs trên RT). Có thể đây là gốc lỗi 1000 Hz của tuần 6
- San bằng `.config` RT/non-RT dời sang GĐ2 (ROADMAP)
- Còn từ tuần 5–6: unbind khi còn fd mở; `dropped` gộp nhiều nguồn; kernel debug chậm lần đầu mỗi đường khóa

**Dữ kiện chỉ người làm có**: kernel nào sẽ chạy trong buổi demo (nếu giảng viên yêu cầu RT thì chọn RT không debug).

**Bẫy mới**
- Đổi `PREFERRED_PROVIDER` qua lại hai lần trong một ngày: mỗi lần kernel kia bị gỡ khỏi work và phải build lại thật (không chỉ setscene), ~10–15 phút mỗi lần ở 3 luồng
- Module lấy từ sstate thì `packages-split` rỗng (xem trên)
- `ko-push.sh` trên non-RT: không có `/proc/lockdep_stats`, grep trả 1 (đã thêm `; true`)
- Tác vụ nền phía công cụ (lệnh bị đẩy ra nền vì quá 10 phút) phải dừng bằng chính công cụ, `kill %1` không với tới; nó vẫn mở `/dev/mpu6050` làm `rmmod` báo "in use"
- Busybox không có `pkill`, `head -N` (dùng `head -n N`)
- zsh không tách `$cfg` thành từ: `set -- $cfg` không chạy, dùng `${cfg%%:*}`

---

## Tuần 7 — FIFO, đọc theo lô, 400 kHz, non-RT (2026-10-30) — XONG 2026-10-30

Mốc kiểm tuần 7 đạt: `dropped` = 0 ở tốc độ tối đa đã chọn (500 Hz, 60 s, cả hai kernel); bảo toàn Δirq = bản ghi + Δ`dropped` (+ mẫu dư); tràn cố ý → `dropped` tăng đúng số mẫu mất (khớp khoảng ts), FIFO reset, tự hồi; thí nghiệm không reset 4/4 không lệch khung; ts khớp cạnh INT (ftrace + LA), mẫu dư lúc start có ts lùi đúng chu kỳ; hồi quy (selftest, `mpu-check.sh`, `mpu-stress.sh`, `load 1800`, `shake`) đạt trên RT debug và non-RT; lockdep 1 sau mọi đợt trên RT debug. Quyết: I2C 400 kHz trong overlay. Lệch: 1000 Hz trên RT debug còn ~0.03% mất (giới hạn ở trên).

**Thiết kế chốt (sửa từ bản cuối session tuần 6)**
- Nửa trên giữ nguyên ý: ghi ts từng cạnh vào kfifo `edges` (128), đánh thức thread khi số cạnh chưa chụp (`len − inflight`) ≥ `batch`. Sửa: trước đó đếm cả cạnh đang xử lý, thread chạy lại ngay sau lô lúc cạnh mới nhất đã cũ, 977/1011 lô không neo được ở 1000 Hz
- Thread: chụp q cạnh; chờ bận tới 60 µs sau cạnh mới nhất (trừ 40 µs trên RT); đọc count; count % 8 ≠ 0 → reset; count = 512 → reset; neo (`now − cạnh + 300 µs < chu kỳ` và khoảng cạnh cuối ±300 µs) → n < q: q − n mất, n > q: mẫu không cạnh; không neo → ghép từ cũ nhất, trừ khi cửa sổ chốt < 1 chu kỳ và dư ≥ 2 (n − q − 1 khung chắc chắn không cạnh); đọc n khung một giao dịch; gán ts (`mpu6050_frame_ts`)
- ts mẫu không cạnh: sau start/reset lùi theo chu kỳ danh định; đang chạy thì đặt vào khoảng giữa hai cạnh còn dư thời gian nhiều nhất, chia đều
- `batch` = min(~10 ms, số khung đọc kịp trước mẫu kế) với khung = 72 bit / `clock-frequency` adapter, `chu kỳ − 450 − 150 µs`. 400 kHz: 1000 Hz → 2, 500 → 5, 333 → 4, ≤ 200 → ~10 ms
- Reset FIFO: `FIFO_EN` 0 → `USER_CTRL` `FIFO_RST` → `USER_CTRL` `FIFO_EN` → `FIFO_EN` 0x88 (như mainline). Lỗi bus giữa chừng → `reset_pending`, lô sau làm lại
- Đổi so với bàn giao: fifo_buf chặn theo `fifo_size` (count rác/không reset từng có thể ghi tràn bộ đệm 1 KB); thêm `stall_ms`, debugfs. ABI, sysfs, nghĩa `dropped`, thứ tự khóa không đổi; thread vẫn không lấy khóa nào
- `.ko` chốt: RT debug `8c1f95cf…` (22988 B), non-RT `92f38033…` (20748 B). dtbo 400 kHz `51b9e80e…`

**Bước 1: 100 Hz, 100 kHz (RT debug)**
- 10 s: irq +1001 = 1000 bản ghi + dư 1, `dropped` 0. 3000 bản ghi: khoảng ts trung vị 9.9904 ms (LA tuần 6: 9.9906), 100.095/s, không ts nào không tăng
- ftrace 1000 bản ghi: 0 chung cạnh, |ts − irq50| 2–5 µs, 4 sự kiện mỗi mẫu (26, 50, đọc count, đọc khung). LA 5 s (`fifo100`): count = 8 cả 501 lần, 500/500 khung trùng bản ghi, ts − cạnh ±83 µs

**Bước 2: tràn cố ý (`mpu-check.sh oflow`, 100 Hz)**
- Có reset: chặn 1000 ms → khoảng ts 1068.8 ms = 107 chu kỳ, `dropped` +106; 2000 ms (hàng đợi cạnh cũng đầy) → 207 chu kỳ, +206. Dư 0
- Không reset: 700/1000/1230 ms ở 100 kHz → +13/+38/+67 = (14/39/68 chu kỳ) − 1; 1000 ms ở 400 kHz → +38..+42. Temp 34.05–34.11 °C, |a| 1.005–1.013 g suốt: không lệch khung 4/4. Lặp lại ở 400 kHz và non-RT với reset mới: như trên
- Mẫu dư lúc start (500 Hz, 60 lần mở): 37/60 có `extra` 1 (một lần 3), đúng những lần đó khoảng đầu 2.000000 ms

**Bước 3: 1000 Hz ở 100 kHz (`fifo1k-100k*.sr`)**
- Đầu tiên 627/s + 316 dropped/s, 977/1011 lô không neo, NACK + timeout như tuần 6. Sau sửa `inflight`: 987/s, 2.1 dropped/s, 4 NACK/20 s, hết treo 1 s
- Vẫn hỏng: đọc count ≥ 0.45 ms nên 85% lô không neo; bus bận ~77%. Thêm luật dư ≥ 2: tốc độ theo ts về 1000.915/s nhưng khoảng ts méo, 255 cạnh dồn/10 s. Kết luận: cần 400 kHz

**Bước 4: 400 kHz (overlay, reboot 1)**
- `clock-frequency = <400000>` trong `&i2c2` của overlay. `/sys/bus/i2c/devices/i2c-2/of_node/clock-frequency` = 0x61a80. Đọc count ~0.22 ms, 99% lô neo
- Phát hiện khung hỏng ở 1000 Hz (cả ở 100 kHz trước đó): temp −77..+117 °C, khung `ffff`. LA (`fifo-misalign-1khz-2026-10-30.sr`): count dư 1..6 byte ở mọi lần đọc khi đã lệch, dữ liệu lệch 2 byte, có lần cạn giữa chừng
- Tách: DLPF không phải (500 Hz cùng DLPF sạch); `batch=1` 1000 Hz sạch, lần đầu 4998/5000 hỏng từ lúc start (reset cũ khi `FIFO_EN` còn bật). Sửa reset + phát hiện count lẻ: còn 16–21 lần lệch/10 s, count 79/97/139… Lô 1, 2 ở 1000 Hz và lô 5 ở 500 Hz sạch; lô 3..10 ở 1000 Hz và lô 10 ở 500 Hz lệch ~1% số lô. LA: lần đọc khung bắt đầu ~0.45 ms sau cạnh → lệch xảy ra khi lần đọc kéo qua lúc chip ghi mẫu kế
- Thử ngân sách động (đo thời gian/byte từ lần đọc count) sai: gộp chi phí giao dịch cố định, ra 0 khung, FIFO tràn liên tục; và reset lỗi giữa chừng để `FIFO_EN` = 0, treo âm thầm. Thay bằng `batch` tĩnh theo tốc độ bus + `reset_pending`
- Kết quả RT debug: 1000 Hz lô 2, 30000 bản ghi ×2: `misaligned` 2–4, `dropped` 7–12, 4–8 khung hỏng lọt. 500 Hz 60 s: `dropped` 0, 0 khung hỏng, 500.489/s theo ts

**Bước 5: cạnh trễ trên RT (`scripts/mpu-sched.sh`)**
- 1000 Hz: irq26 → irq50 max 3.8 ms, khoảng irq 50 tới 4–7 ms vài lần/s, không trùng NACK. `sched_switch`: `irq/26` dậy rồi `D`, `kworker/0:7` prio 49 chạy ~6 ms. Work item đều < 1.1 ms (`workqueue_execute_*`) → nhiều khả năng softirq chạy trong kworker khi `local_bh_enable`. Lúc khác không thấy: không đều

**Bước 6: hồi quy RT debug, 400 kHz**: selftest 13/13; `dup` 0/3000; `sweep` 100–500 Hz `dropped` 0, CPU rảnh 87–93%, 1000 Hz 0.3/s, rảnh 60–63%; `cfg` (101 → 202/s, trả về 100 Hz ±2 g); `strace`; `restart`; `gap` lúc stream ~5.0 ms (tuần 6 6–10 ms); ftrace + LA 100 Hz: 0 chung cạnh, count = 8 ×200, ts − cạnh −59..+52 µs; `oc` 20/20 `EBUSY`; `sysfs` 49–50 vòng 0 lỗi; `mod` 20 vòng, slab không đổi; `load 1800`: 180650 bản ghi, `dropped` 0, irq +180648 + 2 mẫu dư, 101–102/s, khoảng ts lớn nhất 26.2 ms (82/1788 giây > 10.5 ms); `shake`: `ay` −1.09..+0.72, `ax` −1.16..+0.37 g, `dropped` 0. Lockdep 1 sau mọi đợt

**Bước 7: non-RT (`6.12.47-yocto-standard`, `CONFIG_PREEMPT`, không debug; nhãn `NonRT`, reboot 2–5)**
- Build lại kernel thật (chữ ký khác 10-22), `zImage` `181dcc6d…`, dtb `cd9c1103…`. Rootfs 52 MB trống không chứa nổi `modules-*.tgz` (136 MB giải nén): chỉ chép 7 module ảnh có + `depmod`
- Thử âm vermagic: `.ko` RT debug → `Invalid module format`, `should be '6.12.47-yocto-standard …'`
- Không có `irq/26` (ngắt GPIO trong hardirq). Lần đầu: 100–500 Hz sạch, CPU rảnh 97–98%, nhưng NACK `-121` khi đọc count: 106/10 s ở 1000 Hz, 7/10 s ở 200 Hz. LA (`int-nack-nonrt-2026-10-30.sr`): cạnh → START đọc count 26 µs (RT debug ~160), NACK ở 21 µs đúng byte `0x72`. Chờ tới 60 µs sau cạnh: NACK 0 ở 200 và 1000 Hz (30 s mỗi mức)
- Bản chờ đầu (`usleep_range`, rồi `ndelay` 60 µs theo ts) làm RT debug xấu đi: `misaligned` 17–36/30 s, A/B với `.ko` cũ cùng lần boot 0–2; debugfs `settle` 14872/14967 lô. ts trên RT trễ cạnh ≥ 40 µs nên chờ theo ts là thừa; trừ 40 µs khi `CONFIG_PREEMPT_RT`: RT `settle` 0, `misaligned` 2/30 s
- Non-RT với `.ko` chốt: selftest 13/13; `sweep` 100–1000 Hz `dropped` 0, NACK 0, rảnh 95–96% (1000 Hz 88%); 1000 Hz 30000: irq = bản ghi = 30000, `dropped` 0, khoảng ts 0.953–1.047 ms, 0 khung hỏng; tràn +107, dư 0; `sysfs` 96 vòng `dropped` 0. Với `.ko` trước đó (khác ở `usleep_range`): `dup`, `cfg`, `strace`, `restart`, `gap` (1.3 ms không mở, 2.6 ms stream), `oc`, `mod`, tràn không reset 4/4, `load 1800` (180603 = irq, `dropped` 0, khoảng lớn nhất 10.6 ms, 5/1788 giây > 10.5 ms), `shake` đều đạt

**Script mới/sửa**: `ko-push.sh`, `bo-setup.sh`, `la-run.sh`, `la-fifo.py`, `mpu-ts.py`, `mpu-sched.sh`; `mpu-check.sh` thêm `one`, `rec`, `oflow`, bộ đếm debugfs; `mpu-trace.sh` đếm bản ghi chung cạnh. Ảnh LA: `docs/fifo-100hz-400k-2026-10-30.sr` (+ `.bin` bản ghi cùng lần), `docs/fifo-misalign-1khz-2026-10-30.sr`, `docs/int-nack-nonrt-2026-10-30.sr`.

**Commit**: chưa (chờ Tus).

---

## Tuần 7 — bàn giao (ghi 2026-10-30)

Đọc mục này trước khi bắt đầu tuần 7 ở session mới. Tuần 1–6 xong; chi tiết tuần 6 ở mục ngay dưới. Ghi chú kiến thức tuần 6: `~/workspace/VKU/DoAn/week6-knowledge-summary.md`. Hạn nộp đồ án chuyên ngành 2026-11-25: giữ cả ngắt lẫn FIFO, không cắt.

**Hệ thống đang chạy (sau tuần 6)**
- Ảnh trên bo vẫn là ảnh tuần 4 (`rootfs-20261023085915.wic`, kernel `-preempt-rt-debug`). Driver, app, header chưa vào ảnh
- Trên bo, trong `/tmp`: `mpu6050.ko` (`f7cab6bc…`, 16640 byte, đang nạp), `mpu6050-test` (`3ad98c68…`, không đổi), `strace`, `mpu-check.sh`, `mpu-stress.sh`, `mpu-trace.sh`
- Driver: nguồn dữ liệu là ngắt DATA_RDY. `devm_request_threaded_irq` (`IRQF_ONESHOT | IRQF_NO_AUTOEN`, `IRQ_DISABLE_UNLAZY`), thread `irq/50-mpu6050` `SCHED_FIFO` 50. Nửa trên lấy `ts_ns` và đếm mẫu mất bằng `atomic64_xchg(irq_ts)`, thread đọc burst 8 byte. Start = ghi `INT_ENABLE` + `enable_irq`; stop = `disable_irq` + tắt `INT_ENABLE`. `INT_PIN_CFG` 0x00. Phía đọc không đổi
- Chia việc mới (2026-10-29): Claude tự build, chép, chạy lệnh trên bo qua ssh, chụp LA bằng `sigrok-cli`; Tus làm phần cứng, sudo, ghi thẻ, reboot, commit. Xem CLAUDE.md

**Vòng sửa–thử tự động (Claude chạy hết, tuần 6 đã dùng)**
- Build: `./scripts/yocto.sh bitbake mpu6050-mod 2>&1 | grep -E 'WARNING|ERROR|Tasks Summary'`, rồi `grep -ciE 'warning|error' …/mpu6050-mod/0.1/temp/log.do_compile` phải 0. Trước lần build đầu mỗi session kiểm `docker ps` + `docker exec <container> ps` và `poky/build-clean/bitbake.lock` (không có bitbake khác thì build)
- `KO=poky/build-clean/tmp/work/beaglebone_yocto-poky-linux-gnueabi/mpu6050-mod/0.1/packages-split/kernel-module-mpu6050-6.12.47-rt6-yocto-preempt-rt-debug/usr/lib/modules/6.12.47-rt6-yocto-preempt-rt-debug/updates/mpu6050.ko`
- Lệnh bo: `scripts/bo.sh 'lệnh'`; chép: `scripts/bo.sh 'cat > /tmp/mpu6050.ko && sha256sum /tmp/mpu6050.ko' < "$KO"`, so với `sha256sum "$KO"`. Script: `scripts/bo.sh 'cat > /tmp/mpu-check.sh' < scripts/mpu-check.sh` (tương tự `mpu-stress.sh`, `mpu-trace.sh`)
- Bo: `rmmod mpu6050; insmod /tmp/mpu6050.ko; dmesg -c`; sau mỗi đợt `grep debug_locks /proc/lockdep_stats` (phải 1)
- Bộ kiểm: `/tmp/mpu6050-test -t`, `sh /tmp/mpu-check.sh irq|dup|sweep|cfg|strace|restart|gap|shake`, `sh /tmp/mpu-stress.sh oc|sysfs|load 1800|mod` (load dài: `nohup … > /tmp/x.txt &`, chờ bằng một lệnh nền `sleep N; scripts/bo.sh 'cat /tmp/x.txt'`), `sh /tmp/mpu-trace.sh 1000`
- LA: D0 SCL, D1 SDA, D2 INT (que đang kẹp ở chân INT của module). `sigrok-cli -d fx2lafw --config samplerate=2m --samples 1m -o x.sr` (PulseView phải đóng). Phân tích: `scripts/i2c-decode.py`, `scripts/la-pulses.py x.sr 0 1 2`, `scripts/la-int2start.py`. Ở 400 kHz 2 MHz không đủ (5 mẫu/bit): dùng 12m hoặc 24m, ít mẫu hơn
- Bo không lên ssh (`No route to host`, gặp 2026-10-30 sau tuần 6): nhờ Tus kiểm nguồn/dây mạng; bo boot lại thì `/tmp` mất, chép lại ko, app (`$APP`, `3ad98c68…`), strace, script

**Đã bắt đầu ở cuối session tuần 6 (2026-10-30), chưa thử trên bo, chưa commit**
- Đã đọc FIFO trong Register Map: `FIFO_EN` (0x23) bit 7 TEMP + bit 3 ACCEL → mỗi mẫu 8 byte ax ay az temp (thứ tự số thanh ghi, giống burst); `USER_CTRL` (0x6A) bit 6 `FIFO_EN`, bit 2 `FIFO_RST` (tự xóa); `CONFIG` bit 6 `FIFO_MODE` (0 = đầy thì ghi đè mẫu cũ nhất, 1 = ngừng ghi); đọc `FIFO_COUNT_H` (0x72) chốt cả H và L; đọc `FIFO_R_W` (0x74) khi rỗng trả lại byte cũ → luôn đọc count trước
- `driver/mpu6050/mpu6050.c` trong cây làm việc đã viết lại nguồn dữ liệu (build sạch, `.ko` `0328b42d…`). Quyết định, theo khuyến nghị, chưa đo:
  - Vẫn ngắt DATA_RDY mỗi mẫu. Nửa trên đẩy `ktime_get_ns()` của từng cạnh vào kfifo `edges` (128 × u64, một người ghi một người đọc), chỉ `IRQ_WAKE_THREAD` khi `kfifo_len(edges) >= batch`; `batch` = `DIV_ROUND_UP(hz, 100)` (~10 ms/lô: 1 mẫu ở 100 Hz, 10 ở 1000 Hz). `edges` đầy → `dropped`++ và `edges_lost`++
  - Thread: chụp q ts (`kfifo_out_peek`), đọc `FIFO_COUNT` (n mẫu), rồi một `i2c_transfer` đọc n × 8 byte từ 0x74. Mỗi mẫu mang ts của chính cạnh của nó (không nội suy). Neo mẫu mới nhất của FIFO vào cạnh mới nhất khi `now - newest + 300 µs < chu kỳ` và khoảng cách cạnh cuối với cạnh trước = chu kỳ ± 300 µs (`mpu6050_anchored`): n < q → q − n cạnh cũ nhất là mẫu mất (`dropped`); n > q → mẫu dư chưa có cạnh (vào FIFO trước khi ngắt mở lúc start), ts lùi từng chu kỳ, bỏ nếu `edges_lost`. Không neo được thì ghép từ cũ nhất, phần lệch để lô sau
  - FIFO đầy (n × 8 ≥ `fifo_size`): reset (`USER_CTRL` = `FIFO_EN | FIFO_RST`), `dropped` += q, bỏ q cạnh. Đọc FIFO lỗi: cũng reset và tính mất cả lô
  - `FIFO_MODE` = 0 (mẫu mất là mẫu cũ, neo từ mẫu mới vẫn đúng). Start: `FIFO_EN` = 0x88 (đọc lại) → `USER_CTRL` = 0x44 → `INT_ENABLE` → `enable_irq` (mẫu vào FIFO trước khi cạnh được đếm thành mẫu dư, không thành mất). Stop: `disable_irq` → `INT_ENABLE` 0 → `USER_CTRL` 0 → `FIFO_EN` 0
  - Tham số module chỉ để thử (không phải giao diện): `batch` (0 = tự động, đọc lúc open), `oflow_reset` (mặc định 1; 0 để thí nghiệm không reset, kiểm giả thuyết 512 chia hết cho 8 bằng trường temp và |a| sau tràn)
  - ABI uapi, tên sysfs, ý nghĩa `dropped`, thứ tự khóa không đổi; thread vẫn không lấy khóa nào
- Session mới: đọc lại diff (`git diff driver/`), soát thiết kế trên trước khi thử. Chỗ đáng nghi nhất: lề neo 300 µs so với độ trễ cạnh → nửa trên dưới tải (tuần 6 thấy `ts_ns` trễ tới ~6.5 ms), và mẫu dư lúc start ở 1000 Hz
- Thứ tự thử đã định: hồi quy + bảo toàn (Δirq = bản ghi + Δ`dropped`) → khoảng cách `ts` đều theo chu kỳ, `ts` khớp cạnh (ftrace + LA) → tràn cố ý (`batch=100` > 64 mẫu, có/không reset) → sweep 100 kHz kể cả 1000 Hz → 400 kHz (overlay, reboot: hỏi Tus) → load 1800, oc/sysfs/mod, shake → non-RT (lưu mốc trước, hỏi Tus)
- Bo không lên mạng lúc ghi mục này (ARP `FAILED` cho 192.168.50.182): nhờ Tus kiểm nguồn/mạng; sau boot `/tmp` trống, chép lại ko, app, strace, script

**Tuần 7 theo kế hoạch (ROADMAP)**: FIFO của chip (`ACCEL` + `TEMP`, 8 byte/mẫu), đọc theo lô, `ts_ns` nội suy lùi, tràn → `dropped` + reset FIFO, thí nghiệm tràn có/không reset. Đo thời gian I2C 100 kHz, quyết 400 kHz. Build non-RT + san bằng `.config`. Mốc: `dropped` = 0 ở tốc độ tối đa đã chọn; tràn cố ý → `dropped` tăng, tự hồi; cả hai kernel qua bộ kiểm.

**Điểm cần quyết ở đầu tuần 7 (phân tích rồi khuyến nghị)**
- Ngắt nào kích đọc lô: DATA_RDY mỗi mẫu rồi chỉ đọc khi `FIFO_COUNT` ≥ ngưỡng (vẫn 1000 lần đánh thức/s ở 1000 Hz), hay chia nhịp ở driver (đếm cạnh, đọc mỗi N ngắt). Chip không có ngắt watermark
- Đọc `INT_STATUS` mỗi lô để thấy `FIFO_OFLOW_INT` (cờ dính, chỉ xóa khi đọc 0x3A vì không bật ANYRD), hay suy ra tràn từ `FIFO_COUNT` = 512
- `ts_ns` nội suy lùi: dùng `ts` của cạnh cuối và chu kỳ thật của chip (đo được 9.9906 ms ở "100 Hz", nhanh 0.09% so với đồng hồ LA), hay chu kỳ danh định
- 400 kHz: đổi `clock-frequency` của i2c2 trong overlay (dtbo trên FAT, đổi bằng scp + reboot) hay trong dtb gốc. Dtb gốc đặt `clock-frequency = <100000>` cho `&i2c2` (`am335x-bone-common.dtsi:241`), trên bus còn EEPROM cape 0x54–0x57 (`atmel,24c256`). Reboot phải hỏi Tus
- Non-RT: đổi `PREFERRED_PROVIDER_virtual/kernel` kích `ReachableStamps` gỡ deploy/work của kernel RT (bẫy tuần 4): lưu mốc RT debug hiện tại ra `~/projectb-baseline/` trước. Ghi thẻ hay chép `zImage`/modules lên `/boot` là việc của Tus hoặc phải hỏi
- Lỗi 1000 Hz (dưới) phải đo lại trong chế độ đọc lô trước khi kết luận gì về nó

**Giới hạn đã biết, mang sang**
- Đọc từng mẫu ở 1000 Hz: chip thỉnh thoảng NACK byte địa chỉ đọc (`d0 3b Sr d1(N)`), lần sau `omap_i2c` báo `Arbitration lost` dù trên dây đủ 101 cạnh, lần sau nữa chờ `OMAP_I2C_TIMEOUT` 1 s với bus rảnh. 500 Hz 60 s sạch. Chưa rõ gốc; đã thử `i2ctransfer` từ userspace (quá chậm để kết luận) và xem vị trí cạnh INT (không quyết định). Đã chọn giữ `sample_rate` 4..1000 (hướng B), mẫu mất vào `dropped`
- Trong lúc thread kẹt trong lần treo 1 s, ghi sysfs cũng chờ tới 1 s (`disable_irq` chờ thread). Xét ở tuần 8 cùng đường lỗi I2C
- `ts_ns` có thể trễ tới ~6.5 ms dưới tải I/O (khe 16.5 ms, `dropped` 0). Giả thuyết chưa kiểm: `irq/26` (GPIO) cùng FIFO 50 với thread ngắt mmc/i2c. GĐ2 xét nâng ưu tiên `irq/26`
- Còn từ tuần 5: unbind khi còn fd mở (kref tuần 8); `dropped` gộp nhiều nguồn; kernel debug làm chậm lần đầu mỗi đường khóa
- Lấy `ts` rồi mới đọc burst: thread kẹt hơn một chu kỳ ở giữa thì có thể ra một bản ghi trùng (chưa thấy: 0/9000). FIFO bỏ được

**Dữ kiện chỉ người làm có**: không còn câu nào đang chờ.

**Bẫy mới**
- PulseView mở là giữ USB của LA: `sigrok-cli` báo "Unable to claim USB interface"
- `sigrok-cli` ghi `.sr` thành nhiều khối `logic-1-N` (20480 mẫu); script đọc phải nối đủ (đã sửa `i2c-decode.py`)
- Đầu vào LA để hở đọc ra mức cao; kênh INT đúng chỗ phải thấp khi ngắt tắt
- `/proc/<pid>/stat`: trường 40 là `rt_priority`, 41 là `policy`. Bo không có `chrt`
- `tail_check` trước đây không đếm "burst read failed"/"Arbitration lost" (mẫu grep chỉ có `warning|bug|error`); đã thêm `failed|lost`
- zsh: `B="ssh -i …"; $B 'cmd'` báo "no such file" (không tách từ); dùng script bọc

---

## Tuần 6 — ngắt DATA_RDY, threaded IRQ (2026-10-30) — XONG 2026-10-30

Mốc kiểm tuần 6 đạt hết ở 100 Hz: `/proc/interrupts` tăng đúng nhịp chip; thread `SCHED_FIFO` 50; LA bắt INT + I2C trong cùng một bản ghi, đo cạnh INT → `ts_ns`; không còn bản ghi trùng; 30 phút `stress-ng` không mất mẫu; selftest, `mpu-check.sh`, `mpu-stress.sh` đạt; lockdep im sau mọi đợt. Lệch: 1000 Hz không dùng được khi đọc từng mẫu (bước 4).

**Thiết kế đã chốt (duyệt 2026-10-29)**
- Ngắt xin một lần ở probe, sau `configure`, trước `misc_register`: `devm_request_threaded_irq(…, IRQF_ONESHOT | IRQF_NO_AUTOEN, "mpu6050")`. `irq_set_status_flags(IRQ_DISABLE_UNLAZY)` để `disable_irq` mask ngay ở GPIO (không phát lại cạnh cũ), trả cờ bằng devm action. Kiểu trigger lấy từ overlay, probe chỉ kiểm phải là `IRQ_TYPE_EDGE_RISING`
- Nửa trên `mpu6050_irq`: `atomic64_xchg(&irq_ts, ktime_get_ns())`, giá trị cũ khác 0 → `dropped`++ (thread chưa lấy mẫu trước, chip đã ghi đè). Được vì `gpio-omap` dùng `handle_simple_irq` cho cạnh: không mask, mọi cạnh tới nửa trên
- Thread `mpu6050_irq_thread`: `xchg(&irq_ts, 0)` lấy `ts`, đọc burst 8 byte, `kfifo_put`. Đọc lỗi cũng `dropped`++. Không lấy khóa nào của driver
- `start`: ghi `INT_ENABLE` = `RAW_RDY_EN` (có đọc lại), `enable_irq`. `stop`: `disable_irq` (chờ nửa trên và thread xong, không phụ thuộc I2C), ghi `INT_ENABLE` = 0 (lỗi chỉ cảnh báo). `release`/`remove` chỉ gọi `stop` khi `streaming` (độ sâu `disable_irq`). `open` và reconfig nhận lỗi từ `start`
- Thứ tự khóa không đổi (`cfg_lock` → `read_lock`); thêm: thread không lấy `cfg_lock`/`read_lock`, vì `stop` chờ thread trong `cfg_lock` (lockdep không thấy kiểu chờ này)
- `INT_PIN_CFG` = 0x00 (xung 50 µs, không ANYRD), quyết sau bước 0. `sample_rate` giữ 4..1000 (hướng B, quyết sau bước 4)

**Bước 0: xung INT (LA D2 vào chân INT, `docs/int-pulse-2026-10-30.sr`)**
- Bật `INT_ENABLE` bằng `i2cset -f`, không ai đọc `INT_STATUS`: 50 xung trong 0.5 s, rộng 48.5–49 µs, chu kỳ trung bình 9.9906 ms, không giao dịch I2C. `INT_STATUS` vẫn đọc ra 0x01: cờ dính nhưng không chặn xung → không cần ack mỗi mẫu. Không bật ANYRD vì tuần 7 sẽ xóa mất `FIFO_OFLOW_INT`
- Trước khi kẹp que, mọi kênh D2–D7 đọc mức cao dù `devmem 0x4804C138` bit 28 (P9_12) = 0: đầu vào LA để hở đọc ra cao

**Bước 1: build, nạp (`f7cab6bc…` bản cuối; bản đầu `71ad314e…`)**
- Build không warning. Sau `insmod`: `50: 0 4804c000.gpio 28 Edge mpu6050`, số đếm 0 khi chưa mở, `INT_ENABLE` 0x00
- Mở 10 s: irq 50 +1004, irq 26 +1004 (mỗi cạnh một ngắt GPIO), bản ghi = ngắt, dư 0, CPU rảnh 93%. Đóng: `INT_ENABLE` về 0x00
- `irq/50-mpu6050`: policy 1 (FIFO), rt_priority 50; `irq/26-4804c000.gpio`, `irq/27-4819c000.i2c` cũng FIFO 50

**Bước 2: hồi quy phía đọc**
- Selftest 13/13; `cfg` 100 → 201/s, khe 10.0 ms ở 100 Hz, 5.0–5.5 ms ở 200 Hz (tuần 5: 8.4–11.7 ms); `strace`, `restart` như tuần 5
- `gap`: ghi sysfs lúc stream 6.2–10.6 ms (tuần 5 ~5 ms): thêm ghi `INT_ENABLE` = 0 và ghi + đọc lại khi bật

**Bước 3: bản ghi trùng (`mpu-check.sh dup`)**: 0/3000 ở 100 Hz, 0/6000 ở 333 Hz (tuần 5: 31 ở 333 Hz), `dropped` 0

**Bước 4: quét tốc độ (`mpu-check.sh sweep`, 10 s mỗi mức)**
- 100/200/333/500 Hz: bản ghi = ngắt (dư 0–1), dropped 0–0.2/s, CPU rảnh 94/89/85/78%. 500 Hz 60 s: 498.6/s, 0 lỗi I2C
- 1000 Hz: 103 bản ghi/s, 799 dropped/s, dư 11 = 11 lần đọc lỗi (lúc đó chưa vào `dropped`; đã sửa). dmesg: `-121` → `Arbitration lost` + `-11` → `-110`, lặp lại. LA 6 s: NACK ở `d0 3b Sr d1(N)`, giao dịch kế đủ 101 cạnh, sau đó bus rảnh (SCL/SDA cao, INT vẫn 1 kHz) suốt lúc `omap_i2c` chờ 1 s. Lúc không lỗi thread theo 713–775/s
- Tách giả thuyết: `i2ctransfer` từ userspace, `INT_ENABLE` 0, 1000 vs 100 Hz: 857 giao dịch 0 lỗi, nhưng chỉ 14 giao dịch/s, không đủ so với tần suất ~1/1600. Vị trí cạnh INT so với `Sr`: 29 giao dịch cùng khung 0–25 µs, chỉ 1 NACK. Chưa rõ gốc → hướng B (Tus duyệt): giữ 4..1000, ghi giới hạn

**Bước 5: cạnh INT → `ts_ns` (`scripts/mpu-trace.sh` + LA, `docs/int-i2c-2026-10-30.sr`)**
- ftrace `trace_clock mono`, `irq_handler_entry` (26, 50), `i2c_write` (adapter 2), 1000 mẫu, 0 sự kiện mất. irq 26 → irq 50 (`ts_ns`): trung vị 54, p99 73, max 208 µs. irq 50 → `i2c_write`: 68 / 113 / 121 µs. |`ts_ns` − irq 50 entry| 2–4 µs
- LA: cạnh INT → START trung vị 162, p99/max 208 µs (100/100 cạnh có START). ftrace irq 26 → `i2c_write` trung vị 122 µs; lệch ~40 µs = cạnh → irq 26 + `i2c_write` → START. Suy ra cạnh INT → `ts_ns` ≈ 55–95 µs (kernel debug)
- Burst 1093–1112 µs

**Bước 6: `mpu-stress.sh load 1800`** (stress-ng vm 1072 bogo-ops/s): 180673 bản ghi, `dropped` +0, irq 50 +180674 (≈ 1805 s × 100.09). 101–102/s mọi giây, 0 giây khe > 20 ms (tuần 5: thấp nhất 72/s, 128/602 s khe > 20 ms). Khe lớn nhất 16.5 ms nhưng không mất mẫu: `ts_ns` lấy trễ; 1764/1788 giây khe ≤ 10.5 ms, 24 giây 10.5–16.5 ms. Lockdep 1

**Bước 7: dọn dẹp**
- `oc`: 20 selftest 0 FAIL, 20 cặp: 20 chạy, 20 `EBUSY`
- `mod`: `rmmod` khi mở "in use"; 20 vòng 0 lỗi; `kmalloc-8k` 23 → 23, `4k` 47 → 47. Sau `rmmod` không còn dòng 50 trong `/proc/interrupts`, `/sys/kernel/irq/50/actions` rỗng
- `sysfs` (ghi ngẫu nhiên kể cả 1000 Hz): 0 lần ghi lỗi; `dropped` +1268 rồi +1629; lần hai giữ dmesg: 2 `-110`, 2 `Arbitration lost`, 1 `-121` (giới hạn 1000 Hz, mỗi lần treo ~1 s)
- `shake` (Tus nghiêng module, 15 s): đứng yên `ax -0.984 ay +0.213 az -0.058`; lúc nghiêng az lên +0.94, ay từ −0.32 tới +0.77, |a| ≈ 0.91–1.0 g (thấp hơn khi đang chuyển động); 101/s, khe 10.0–10.1 ms, `dropped` +0

**Script mới/sửa**: `mpu-check.sh` thêm `irq`, `dup [n]`, `sweep`; `mpu-stress.sh load` in số ngắt, `tail_check` đếm thêm `failed|lost`; `mpu-trace.sh` (ftrace); `la-pulses.py` (xung từng kênh); `la-int2start.py` (cạnh INT → START); `i2c-decode.py` nối các khối `logic-1-N`.

**Commit**: chưa (chờ Tus).

---

## Tuần 6 — bàn giao, chưa bắt đầu (ghi 2026-10-29)

Đọc mục này trước khi bắt đầu tuần 6 ở session mới. Tuần 1–5 xong; chi tiết tuần 5 ở mục ngay dưới. Ghi chú kiến thức tuần 5: `~/workspace/VKU/DoAn/week5-knowledge-summary.md`.

**Hệ thống đang chạy (sau tuần 5)**
- Ảnh trên bo không đổi từ tuần 4: `rootfs-20261023085915.wic` (`20fbfbbc…`), kernel `6.12.47-rt6-yocto-preempt-rt-debug` (lockdep), `HZ=100`, `HIGH_RES_TIMERS`
- Driver, app, header vẫn chưa vào ảnh. Trên bo chỉ có trong `/tmp` (tmpfs, reboot là mất): `/tmp/mpu6050.ko` (`02a18d11…`, 15972 byte), `/tmp/mpu6050-test` (`3ad98c68…`), `/tmp/strace` (`985836a4…`, từ `packages-split/strace` của `build-clean`; ảnh không có strace), `/tmp/mpu-check.sh`, `/tmp/mpu-stress.sh`
- Commit cuối của phần code: `94ac4c4` (driver), `f7ca9eb` (app), `1b4e2da` (script), `5b711a3` (ảnh LA)
- Driver hiện có (`driver/mpu6050/mpu6050.c`, 823 dòng): probe như tuần 4 + `struct mpu6050` (devm), misc device `/dev/mpu6050` (10, minor động, 124 trên bo), kfifo 256 bản ghi, `read`/`poll`/`ioctl`, sysfs `sample_rate`, `accel_range`, `dropped`. Nguồn dữ liệu tạm: hrtimer xếp `work_struct`, work đọc burst 8 byte `0x3B..0x42`. Nguồn bật ở `open`, tắt ở `release`
- `docs/boot-serial-2026-10-23-debug.log` đang bị sửa trong cây làm việc (picocom ghi thêm một lần poweroff). Chưa `git restore`; Tus tự chạy nếu muốn bỏ

**Vòng sửa–thử (giữ nguyên, thêm app và strace)**
- Container: như tuần 4; `bitbake mpu6050-mod mpu6050-test 2>&1 | grep -E 'WARNING|ERROR|Tasks Summary'`. Sửa header thì `mpu6050-uapi` và app build lại theo
- Laptop, trong `~/workspace_projectb`:
  - `W=$PWD/poky/build-clean/tmp/work`
  - `KO=$W/beaglebone_yocto-poky-linux-gnueabi/mpu6050-mod/0.1/packages-split/kernel-module-mpu6050-6.12.47-rt6-yocto-preempt-rt-debug/usr/lib/modules/6.12.47-rt6-yocto-preempt-rt-debug/updates/mpu6050.ko`
  - `APP=$W/cortexa8hf-neon-poky-linux-gnueabi/mpu6050-test/0.1/image/usr/bin/mpu6050-test`
  - `STR=$W/cortexa8hf-neon-poky-linux-gnueabi/strace/6.12/packages-split/strace/usr/bin/strace`
  - `sha256sum "${KO:?}" "${APP:?}"`
  - `ssh -i ~/.ssh/id_ed25519_bbb -o IdentitiesOnly=yes root@192.168.50.182 'cat > /tmp/mpu6050.ko && sha256sum /tmp/mpu6050.ko' < $KO`; app thêm `chmod +x`. Script: `… 'cat > /tmp/mpu-stress.sh' < scripts/mpu-stress.sh`
- Bo: `rmmod mpu6050` trước khi `insmod` bản mới (`File exists` là module cũ còn nạp); sau mỗi đợt `dmesg -c`, `grep debug_locks /proc/lockdep_stats`
- Bộ kiểm có sẵn: `/tmp/mpu6050-test -t` (13 check, mã thoát = số FAIL), `/tmp/mpu6050-test -n N` (mỗi giây một dòng: tốc độ, cấu hình, g, °C, max gap, dropped, age), `sh /tmp/mpu-check.sh shake|cfg|strace|restart|gap`, `sh /tmp/mpu-stress.sh oc|sysfs|load [s]|mod`. LA: `python3 scripts/i2c-decode.py <file.sr>`

**Tuần 6 theo kế hoạch (ROADMAP)**: ngắt DATA_RDY, `request_threaded_irq` + `IRQF_ONESHOT`, `ktime_get_ns()` ở primary handler, bỏ hrtimer + work. Mốc: `/proc/interrupts` tăng đúng `sample_rate`; thread `SCHED_FIFO` 50; logic analyzer INT + I2C cùng một bản ghi; 30 phút `stress-ng`; lockdep im. Phía đọc (kfifo, `read`, `poll`, ioctl, sysfs) không đổi: chỉ thay producer.

**Số đo tuần 5 để so ở tuần 6 (cùng phép thử, đổi nguồn)**
- `mpu-stress.sh load 600`, nguồn hrtimer + kworker `SCHED_OTHER`: trung bình 83/s ở 100 Hz, thấp nhất 72/s, khe lớn nhất 31.4 ms, 128/602 giây có khe > 20 ms. `load 120` sau khi sửa đếm: 11876 bản ghi + `dropped` 583 = 12459 ≈ 124.6 s × 100
- LA nhịp 10 ms: phần lớn 9.92–10.06 ms; 2 cặp bản ghi trùng từng byte trong 0.5 s (burst trễ ~7 ms, burst sau cách 2–3 ms đọc lại mẫu cũ); burst 1.11–1.13 ms, có lần bị kéo tới 2.6 và 4.7 ms
- Kỳ vọng tuần 6 phải kiểm: không còn bản ghi trùng; dưới stress-ng tốc độ giữ đúng nhịp chip vì thread IRQ là `SCHED_FIFO` 50

**Điểm cần quyết ở đầu tuần 6 (phân tích rồi khuyến nghị, đừng hỏi Tus chọn)**
- Tốc độ tối đa khi đọc từng mẫu: burst 8 byte ở 100 kHz mất ~1.12 ms, đuôi tới vài ms. 1000 Hz không đạt được (bước 5: ghi 1000 thì app thấy dưới 1000/s). Chọn: giới hạn `sample_rate` theo bus, hay giữ 4..1000 và để `dropped` báo. FIFO chip + 400 kHz là việc tuần 7
- Xóa cờ ngắt: `INT_PIN_CFG` = 0 hiện tại là xung 50 µs, `INT_STATUS` chỉ xóa khi đọc (thêm một giao dịch ~0.5 ms mỗi mẫu). `INT_ANYRD_2CLEAR` cho burst dữ liệu xóa luôn. Phải xem trên LA chân INT có xung đều khi không đọc `INT_STATUS` hay không, trước khi chốt
- Đếm mẫu mất khi thread trễ quá một chu kỳ: chip ghi đè thanh ghi, không còn `schedule_work` trả false để đếm. Cách phát hiện (khoảng `ts_ns` > 1.5 chu kỳ, hay chỉ đếm ở tuần 7 bằng FIFO) phải quyết, vì `dropped` đã hứa "mọi mẫu mất"
- Bật/tắt nguồn: `request_irq` ở probe + `INT_ENABLE` DATA_RDY bật ở `open`/tắt ở `release` (giữ cấu trúc start/stop trong `cfg_lock` như bây giờ), hay `enable_irq`/`disable_irq`. Đổi cấu hình khi đang stream: tắt ngắt, ghi, reset kfifo, bật lại như `mpu6050_reconfig` hiện tại
- `ts_ns`: lấy ở primary handler. Trên RT primary chạy trong thread `irq/26-4804c000.gpio` nên đã gồm độ trễ đánh thức thread GPIO (ROADMAP). Đo bằng LA: cạnh INT so với `ts_ns`
- Trigger: overlay đã khai `IRQ_TYPE_EDGE_RISING`, chip active-high push-pull. Kiểm `/proc/interrupts` tăng đúng 100/s

**Giới hạn đã biết, mang sang**
- Unbind khi còn fd mở: `devm` giải phóng `struct mpu6050` trong khi file còn trỏ vào (rmmod thì bị chặn bởi refcount module). Sửa tuần 8 bằng kref; trước đó đừng unbind khi có fd mở
- `dropped` gộp mất ở phía đọc (kfifo đầy) và phía nguồn (lỡ nhịp). Tách hay không: quyết ở tuần 7 khi có `FIFO_OFLOW` thật
- Kernel debug: lần đầu mỗi đường khóa sau `insmod` chậm 3–5 lần (lockdep kiểm chuỗi mới). Đo thời gian thì chạy làm nóng một lượt, bỏ lượt đó; số chính thức đo trên kernel không debug (tuần 7)

**Dữ kiện chỉ người làm có**
- Ngày nộp đồ án chuyên ngành: 2026-11-25 (Tus trả lời 2026-10-29), giữ ngắt và FIFO, không cắt

**Bẫy mới (thêm vào danh sách các mục bàn giao trước)**
- Biến shell chỉ sống trong terminal đã gán: `sha256sum $APP` với `$APP` rỗng đọc stdin và đứng im. Dùng `"${APP:?}"`
- `insmod: File exists` = module cùng tên đang nạp, không liên quan file `.ko`. Ghi đè `.ko` không đổi module đang chạy
- Kernel không bật `MODULE_SRCVERSION_ALL`: không có `/sys/module/mpu6050/srcversion`, `modinfo -F srcversion` rỗng. Nhận bản bằng hash file trước `insmod`
- Yocto build tái lập: chỉ sửa chú thích trong header thì app build lại ra đúng hash cũ (`3ad98c68…` hai lần)
- Script tạm ở scratchpad của Claude có thể biến mất giữa session; script dùng trên bo để trong `scripts/`
- Bo khởi động lại thì mất `/tmp` (ko, app, strace, script). `dmesg -c` lần đầu sau boot in cả log boot
- Rút điện khi `/boot` (FAT) đang mount rw: dmesg `FAT-fs (mmcblk0p1): Volume was not properly unmounted`. Tắt bằng `poweroff`, chờ `reboot: Power down` trên serial
- `strace` thấy `ERESTARTSYS` chỉ khi tín hiệu đến lúc tiến trình đang ngủ trong syscall; ở 100 Hz `cat` hay ở ngoài `read`, hạ 4 Hz thì thấy chắc
- `kill` mặc định (`SIGTERM`) không có handler: syscall bị ngắt nhưng không bao giờ được gọi lại; muốn thấy restart dùng `SIGSTOP`/`SIGCONT`
- busybox: `od` không có `-A`/`-t` (dùng `hexdump -e`); `sleep 0.2` và `$RANDOM` chạy được; `pidof` có

---

## Tuần 5 — char device, uapi, sysfs (2026-10-28..29) — XONG 2026-10-29

Mốc kiểm tuần 5 đạt hết: lắc (nghiêng) → số đổi; đổi `sample_rate` → tốc độ đổi; `strace` thấy `poll`/`read`/`ioctl`; `EINVAL`, `EAGAIN`, `EBUSY`, `ESPIPE`, `ENOTTY`; `rmmod` khi fd mở bị chặn; lockdep im (`debug_locks: 1` sau mọi đợt). Thêm: LA một burst 8 byte khớp từng byte với `read()` ở userspace.

**Thiết kế đã chốt (duyệt 2026-10-28, ba chỗ Tus sửa đánh dấu (T))**
- Header `driver/mpu6050/include/uapi/projectb/mpu6050.h` (`581d3430…`), SPDX `GPL-2.0-only WITH Linux-syscall-note`: `mpu6050_sample` 16 byte (`__s16 ax, ay, az, temp; __u64 ts_ns`, `ts_ns` ở offset 8 trên mọi ABI, `CLOCK_MONOTONIC`), `mpu6050_config` 28 byte (whoami, sample_rate_hz, accel_range_g, accel_lsb_per_g, accel_bw_hz, temp_sens_x100, temp_offset_mc), `mpu6050_stats` 8 byte (T) (`dropped`). Ioctl magic `'x'`: `GET_CONFIG` `_IOR(1)` = `0x801c7801`, `GET_STATS` `_IOR(2)` = `0x80087802` (T), `FLUSH` `_IO(3)` = `0x7803`. Thứ tự cắt nếu phải cắt: bỏ `FLUSH` trước (T). Kiểm kích thước bằng `_Static_assert` (C) / `static_assert` (C++)
- Header qua sysroot bằng recipe riêng `mpu6050-uapi` (`inherit allarch`, cài `${includedir}/projectb/`), không gắn vào `mpu6050-mod`: recipe module là MACHINE_ARCH, chữ ký theo kernel; app và `sensord` không phải build lại khi đổi RT/non-RT/debug. Kernel include bằng `ccflags-y += -I$(src)/include/uapi`
- Misc device, `misc.parent = &client->dev` (T) (`/sys/class/misc/mpu6050/device` → `2-0068`), thuộc tính sysfs qua `misc.groups` (tạo trước uevent). `misc_open` đặt `private_data` = `&misc` và `fops_get` giữ refcount module (rmmod bị chặn). `stream_open` → `lseek` `ESPIPE`
- Một người mở: `test_and_set_bit_lock` → `EBUSY`; nhả bằng `clear_bit_unlock` sau khi nguồn đã tắt
- kfifo 256 × 16 byte (2.56 s ở 100 Hz). Một producer không khóa; phía đọc khóa `read_lock` (hai luồng cùng fd vẫn đọc song song được). Thứ tự khóa `cfg_lock` → `read_lock`. `wait_event_interruptible` không giữ `read_lock`. `kfifo_reset` chỉ khi producer đã dừng (open, reconfig); `FLUSH` dùng `kfifo_reset_out` dưới `read_lock`, producer cứ chạy. Kfifo đầy: bỏ mẫu mới, `dropped`++ (`atomic64`)
- `read`: < 16 byte `EINVAL`; trả bội số 16, phần lẻ bỏ; `count` cắt về 16 × 256 vì `kfifo_to_user` nhận `unsigned int`; rỗng: `EAGAIN` hoặc ngủ, signal → `-ERESTARTSYS`. `poll`: `EPOLLIN | EPOLLRDNORM`
- Sysfs: `sample_rate` 4..1000 Hz (`SMPLRT_DIV = 1000/hz - 1`, đọc lại tốc độ thật làm tròn xuống: ghi 300 đọc 333), `accel_range` 2/4/8/16, `dropped` RO. Store khi không ai mở chỉ ghi thanh ghi; khi đang stream: dừng nguồn, ghi, `kfifo_reset` dưới `read_lock`, chạy lại (mọi bản ghi sau khi store trả về đều theo cấu hình mới). Lỗi I2C giữa chừng thì ghi lại cấu hình cũ
- DLPF chọn bằng hàm (T): duyệt {184, 92, 41, 20, 10, 5} Hz, lấy mục đầu có 2 × bw ≤ tốc độ thật, không có thì 5 Hz. `CONFIG` và `ACCEL_CONFIG2` cùng giá trị

**Bước 0–1: header, recipe, misc device (2026-10-28)**
- Header biên dịch trên host: 16/28/8 byte; struct sai (`__u64` không ở offset 8) ra 16 trên x86_64, 12 trên i386, assert bắt được. `mpu6050-uapi` ở `all-poky-linux`, header có trong `-dev` và sysroot của app
- Bo: `crw------- 10, 124`; `cat` ngủ ở trạng thái `S`, wchan `mpu6050_read`; mở lần hai `EBUSY`; `rmmod` "in use" (refcount 1); `dd bs=8` `EINVAL`; `readlink /sys/class/misc/mpu6050/device` → `…/i2c-2/2-0068`

**Bước 2: nguồn dữ liệu tạm — `delayed_work` bỏ, thay hrtimer + work**
- `delayed_work` bản 1 (hẹn theo hạn ns): cặp bản ghi giống hệt cách nhau 3.4 ms. Bản 2 (hẹn 1 jiffy): 20 ms, 50 Hz
- Nguyên nhân: `calc_index` (`kernel/time/timer.c:579`) cộng một tick để timer không bao giờ chạy sớm; hẹn N jiffy chạy sau N..N+1 tick. `HZ=100` thì không có cách ra đúng 10 ms
- hrtimer (`hrtimer_forward_now`, mốc tuyệt đối) xếp work, work đọc I2C (callback hrtimer không được ngủ). Trên RT callback chạy trong `ktimers`. Kết quả: 100 Hz, khoảng 8.4–11.7 ms, bản ghi trùng hiếm; `dd` 100 bản ghi 1.047 s. Giữ fd 5 s không đọc: `dropped` 249 (kfifo 256 đầy), giữ nguyên sau khi đóng

**Bước 3: sysfs**
- Tám tốc độ biên 333, 166, 83, 76, 40, 38, 20, 19 Hz: `SMPLRT_DIV`, `CONFIG`, `ACCEL_CONFIG2` đọc bằng `i2cget` khớp hết với hàm DLPF. Ghi sai giá trị → `EINVAL`
- Tốc độ đo: 50 → 50.0, 200 → 199.7, 333 → 330.9 Hz (31 bản ghi trùng: nhịp hrtimer và đồng hồ chip độc lập). `accel_range`: `ACCEL_CONFIG` 0x08/0x10/0x18/0x00, giá trị thô đổi đúng tỉ lệ; đổi khi đang stream thấy một khe ~25 ms, không có bản ghi lẫn hai thang

**Bước 4: ioctl, app test (2026-10-29)**
- `apps/mpu6050-test/` + recipe (`DEPENDS = "mpu6050-uapi"`), build không warning, `PACKAGE_ARCH` `cortexa8hf-neon`. Selftest 13/13 PASS: `EAGAIN` ngay sau open, `EBUSY`, `GET_CONFIG` nhất quán, `GET_STATS`, `ENOTTY`, `ESPIPE`, `write` → `EBADF` (do VFS: fd `O_RDONLY`, driver không có `.write`), `poll` → `POLLIN`, `read` 8 → `EINVAL`, `read` 40 → 32, `ts_ns` đơn điệu và cùng miền `CLOCK_MONOTONIC`, `FLUSH` → `EAGAIN` (hoặc chen một bản ghi), mở lại được
- App 100/s, age (now − `ts_ns` của mẫu cuối) ~1.4 ms. Mỗi dòng gọi lại `GET_CONFIG` nên đổi `accel_range` giữa chừng (±2 → ±4 g) thì cột g không nhảy
- `strace`: `poll` → `read` 16 hoặc 32 → `read` `EAGAIN`; ioctl hiện đúng mã ở trên
- `ERESTARTSYS`: ở 100 Hz lần đầu không thấy (`cat` không ở trong `read` lúc nhận `SIGSTOP`); 4 Hz: `read = ? ERESTARTSYS` → `SIGSTOP` → `SIGCONT` → `read` gọi lại trả các bản ghi dồn; `SIGTERM` cũng `ERESTARTSYS` rồi chết, không gọi lại
- Thời gian một lần ghi sysfs (`strace -T`): ổn định ~5 ms (6 giao dịch ghi + đọc lại), như nhau lúc không mở và lúc stream. Lần đầu mỗi thuộc tính sau `insmod` 15–24 ms; `dependency chains` +209 sau lượt đầu, +74 sau lượt hai, lượt hai mọi lần ghi 4.6–5.2 ms → lockdep kiểm chuỗi khóa mới. Cũng giải thích khe 28–42 ms ở giây đầu app chạy ngay sau `insmod`. Khe khi đổi cấu hình ≈ tối đa một chu kỳ cũ + thời gian ghi + một chu kỳ mới; app `GET_CONFIG` chờ `cfg_lock` trong lúc đó (age 52.9 ms ở dòng có lệnh đổi)

**Bước 5: chạy tải, dọn dẹp (`scripts/mpu-stress.sh`)**
- `mod`: `rmmod` khi app đang mở → `Module mpu6050 is in use`; 20 vòng `insmod`/app/`rmmod` 0 lỗi, `kmalloc-8k` 23 → 23, `kmalloc-4k` 47 → 47 (không có kmemleak; đây là phép so thô)
- `oc`: 20 selftest 0 FAIL; 20 cặp app mở cùng lúc: 20 chạy, 20 `EBUSY`
- `sysfs`: 45 vòng trong 30 s ghi ngẫu nhiên 9 tốc độ × 4 thang khi app đang chạy: 0 lỗi, app 7397 bản ghi, `dropped` 0
- `load 600` (stress-ng cpu + io + vm 64M): lockdep im, nhưng 50172 bản ghi / ~604 s = 83/s, `dropped` 0. Lỗi đếm: kworker `SCHED_OTHER` bị chèn, `schedule_work` trả false khi work cũ còn chờ, nhịp mất không vào đâu. Sửa: `schedule_work` false → `dropped`++; `hrtimer_forward_now` trả N > 1 → `dropped` += N − 1. ABI không đổi, đúng nghĩa đã hứa ("mẫu mất từ lúc probe"); ROADMAP tuần 7 cũng cho FIFO tràn vào `dropped`
- `load 120` sau sửa: 11876 + 583 = 12459 ≈ 124.6 s × 100. Tỉ lệ mất 4.7% (lần 600 s ~17%, stress-ng `vm` lúc đó chạy 1143 bogo-ops/s so với 654: tải nặng hơn). Một lần đo không đủ chốt tỉ lệ

**Logic analyzer (`docs/i2c-record-2026-10-29.*`, `docs/i2c-stream-2026-10-29.*`; 2 MHz, 1 M mẫu, SCL D0/SDA D1)**
- Một bản ghi: `dd if=/dev/mpu6050 bs=16 count=1` → đúng một giao dịch trong 0.5 s (đóng là nguồn dừng): `d1 c1 69 0f 93 fd db 10 13(N)`; `hexdump` cùng lần: `-16023 3987 -549 4115`, ts 673 × 2³² + 3132317271 ns (~2893.6 s từ boot). Khớp từng LSB: dây → `be16_to_cpu` → kfifo → `read()`
- Stream (`mpu6050-test -n 100`): 50 burst trọn, mỗi burst 101 cạnh SCL (`S d0 3b Sr d1` + 8 byte, 7 ACK, byte cuối NACK, `P`), 1.11–1.13 ms. Hai burst bị kéo dài với cùng 101 cạnh: 4730 µs (dừng 3272 µs sau byte `d0`), 2597 µs (dừng 1578 µs giữa `3b` và Sr, bình thường ~108 µs). SCL bị giữ thấp giữa giao dịch; MPU-6500 không kéo giãn xung, nhiều khả năng `omap_i2c` (ngắt chạy trong thread trên RT) chậm phục vụ. Chưa chứng minh bên nào giữ
- Hai cặp burst trùng từng byte (186.6 / 189.7 ms, 417.4 / 419.7 ms): work trễ ~7 ms, nhịp sau vẫn đúng lưới, đọc lại mẫu cũ của chip. Bản ghi trùng có `ts_ns` khác nhau và không vào `dropped`. Tuần 6 (DATA_RDY) bỏ được
- Giải mã độc lập bằng `scripts/i2c-decode.py`, khớp decoder PulseView. File `.pvs` (phiên PulseView) không commit

**Câu hỏi đã đóng**
- Ai gửi NACK ở byte đọc cuối: master (BBB). Bit thứ 9 do bên nhận byte gửi; trong pha đọc bên nhận là master. NACK báo chip thôi gửi và nhả SDA, để master tạo được STOP (SDA lên khi SCL cao); ACK thì chip sẽ đẩy byte 0x43 và có thể giữ SDA thấp. Trên ảnh: ACK sau `d0`, `3b`, `d1` là của chip, ACK sau 7 byte dữ liệu là của BBB. (Tus nhờ giải thích, chưa tự trả lời)

**Commit**: `94ac4c4` driver, `f7ca9eb` app, `1b4e2da` script, `5b711a3` ảnh LA, cùng commit ghi mục này.

---

## Tuần 5 — bàn giao, chưa bắt đầu (ghi 2026-10-23)

Đọc mục này trước khi bắt đầu tuần 5 ở session mới. Tuần 1–4 xong; chi tiết tuần 4 ở mục ngay dưới. Ghi chú kiến thức tuần 4: `~/workspace/VKU/DoAn/week4-knowledge-summary.md`.

**Hệ thống đang chạy (sau tuần 4)**
- Bo chạy ảnh `rootfs-20261023085915.wic` (`20fbfbbc…`): kernel `6.12.47-rt6-yocto-preempt-rt-debug` (lockdep + `DEBUG_ATOMIC_SLEEP`, `zImage` `a9872bda…`), dtb `cd9c1103…`, overlay `1a1a0a24…`. Template đặt `PREFERRED_PROVIDER_virtual/kernel = "linux-yocto-rt"` và `PROJECTB_KERNEL_DEBUG = "1"`
- Driver `driver/mpu6050/mpu6050.c`, recipe `meta-projectb/recipes-kernel/mpu6050/mpu6050-mod_0.1.bb`. Chưa vào ảnh (`IMAGE_INSTALL` chưa có `kernel-module-mpu6050`): reboot là mất `/tmp/mpu6050.ko`, phải chép lại
- Driver hiện có: bảng `mpu6050_chips[]` theo `WHO_AM_I`; `mpu6050_reset()`; `mpu6050_write_check()` (ghi + đọc lại); `mpu6050_configure()` (100 Hz, ±2 g, DLPF 41 Hz, ngắt/FIFO tắt); `mpu6050_dump_sample()` (burst 14 byte, in dmesg, tạm); remove ghi `PWR_MGMT_1` = 0x41. Chưa có struct trạng thái per-device (`i2c_set_clientdata`), chưa khóa
- Mốc ngoài repo: `~/projectb-baseline/rt-20261022/` (RT không debug, mốc benchmark RT), `rt-nodebug-ko/`, `non-rt-20261022/`, `non-rt-20261020/`

**Vòng sửa–thử (đã quen, đừng đổi khi không có lý do)**
- Container: `docker run --rm -it -v ~/workspace_projectb:/workspace yocto-build`, `cd /workspace/poky && source oe-init-build-env build-clean`, `bitbake mpu6050-mod 2>&1 | grep -E 'WARNING|ERROR|Tasks Summary'`
- Laptop, trong `~/workspace_projectb`: `KO=$(echo poky/build-clean/tmp/work/beaglebone_yocto-poky-linux-gnueabi/mpu6050-mod/0.1/packages-split/kernel-module-mpu6050-*/usr/lib/modules/*/updates/mpu6050.ko)`, `sha256sum $KO`, `ssh -i ~/.ssh/id_ed25519_bbb -o IdentitiesOnly=yes root@192.168.50.182 'cat > /tmp/mpu6050.ko' < $KO`
- Bo: so hash, `insmod`, `dmesg`, `rmmod`; sau mỗi đợt `grep debug_locks /proc/lockdep_stats` (phải là 1)
- Đổi recipe ứng dụng thì cũng build bằng bitbake rồi chép như trên; ảnh không có gcc

**Tuần 5 theo kế hoạch (ROADMAP)**: header uapi (`struct { __s16 ax, ay, az, temp; __u64 ts_ns; }` 16 byte trên cả ARM32/x86_64, `static_assert`, mã ioctl), `miscdevice` `/dev/mpu6050`, kfifo + wait queue, `read` chỉ trả trọn bản ghi (`EINVAL` nếu buffer nhỏ hơn một bản ghi), `O_NONBLOCK` (`EAGAIN`), một người mở (`EBUSY`), `poll`; sysfs `sample_rate`, `accel_range`, `dropped` (tên chốt, không đổi ở GĐ2); nguồn dữ liệu tạm thay `mpu6050_dump_sample`; app test dùng `poll()` có recipe. Mốc cuối tuần: lắc → số đổi; đổi `sample_rate` → tốc độ đổi; `strace` thấy `poll`/`read`; `EINVAL`, `EAGAIN`; `rmmod` khi fd đang mở bị chặn; lockdep im.

**Điểm cần quyết ở đầu tuần 5 (phân tích rồi khuyến nghị, đừng hỏi Tus chọn)**
- Nguồn dữ liệu tạm: `delayed_work` chạy theo jiffy (`HZ=100`, 10 ms), không theo được tốc độ > 100 Hz và có jitter (đã đo `msleep`). Phương án khác: `hrtimer` đánh thức work (callback hrtimer không được ngủ, I2C thì ngủ). Tuần 6 thay bằng ngắt DATA_RDY, nên chỉ cần đủ cho demo và test API
- Header uapi đi qua sysroot thế nào: `do_install:append` trong recipe module (module.bbclass đã cài `Module.symvers` vào `${includedir}/${BPN}`) để app `DEPENDS` vào recipe module (recipe phụ thuộc máy), hay recipe header riêng cho cả app và `sensord` GĐ2
- Khóa: mutex cho cấu hình và `read`; kfifo một người ghi một người đọc không cần khóa khi chỉ có một người mở. Trên RT spinlock thường thành lock ngủ được; lockdep đang bật để bắt lỗi
- Đổi `sample_rate` phải đổi DLPF theo (băng thông dưới Nyquist). `accel_range` đổi độ nhạy (16384/8192/4096/2048 LSB/g)
- Header uapi đặt trong `driver/mpu6050/include/uapi/`, dùng `__s16`/`__u64` của `<linux/types.h>`

**Dữ kiện chỉ người làm có, vẫn chưa trả lời**
- Chữ in trên con chip
- Kinh nghiệm viết module kernel trước đây, phần đã làm trong driver model của Nothan-OS
- Ngày nộp và ngày bảo vệ đồ án chuyên ngành
- (Câu tự trả lời) ai gửi NACK ở byte cuối của lần đọc, và vì sao

**Bẫy, thêm vào danh sách các mục bàn giao trước**
- `bitbake -e` với `PREFERRED_PROVIDER` khác (kể cả bằng `-R`) gỡ deploy và work của kernel kia (`ReachableStamps`); lưu mốc trước khi đổi kernel, `sstate-cache` còn để lấy lại
- Bật/tắt `PROJECTB_KERNEL_DEBUG` thì build lại module; đo thời gian (tuần 7) trên kernel không debug
- busybox `dmesg -c`, không có `-C`; khối lệnh dán có thể mất dấu xuống dòng; đường dẫn tương đối chỉ đúng khi đứng trong repo
- Chụp logic analyzer: cửa sổ tính từ trigger, chạy một lệnh chứ không lặp; lưu `.sr` toàn bộ rồi mới đổi tên/ghi đè ảnh
- Ghi chú commit: không tiền tố tuần ("week N ..." ở đầu dòng cũng tính)

---

## Tuần 4 — driver MPU6050 (2026-10-23) — XONG 2026-10-23

Kế hoạch tuần 4–8 và ràng buộc: `docs/ROADMAP.md`, mục "Kế hoạch driver tuần 4–8".

Mốc kiểm tuần 4 đạt hết: `UU`; `0x44E10878` = `0x27`; chip id trong dmesg; |a| ≈ 1 g; 20 vòng `insmod`/`rmmod` + 5 unbind/bind, cấu hình đọc lại đúng, lockdep im; logic analyzer một giao dịch burst 14 byte. Kiểm chéo đầu-cuối: byte giải mã từ dây (`i2c-probe-2026-10-23.sr`) và dòng dmesg của cùng lần probe trùng từng số (`accel -16270 1947 -2380 (|a| 1010 mg), temp 33933 mC, gyro 25 76 94`; |a| 1010 vì `int_sqrt` và phép chia nguyên đều làm tròn xuống, tính tay 1010.6).

**Tuần 5 tiếp theo** (ROADMAP): header uapi (struct 16 byte, `static_assert`, mã ioctl), `miscdevice` `/dev/mpu6050`, kfifo + wait queue, `read` trọn bản ghi, `O_NONBLOCK`, một người mở; sysfs `sample_rate`, `accel_range`, `dropped`; nguồn dữ liệu tạm `delayed_work` (thay `mpu6050_dump_sample`); app test `poll()` có recipe, header qua sysroot. Đổi `sample_rate` thì đổi cả DLPF theo (giữ băng thông dưới Nyquist). Không dùng `msleep` cho nhịp lấy mẫu.

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
- Logic analyzer một lần probe (`docs/i2c-probe-2026-10-23.sr`, 1 M mẫu, 2 MHz, SCL D0/SDA D1; giải mã độc lập bằng script đọc `logic-1-1`, khớp decoder PulseView): 27 giao dịch. 0.64 ms `d0 6b 80` (`DEVICE_RESET`) → +100.8 ms `d0 68 07` → +110.4 ms đọc `PWR_MGMT_1` = 01 → 11 cặp ghi + đọc lại trong 11.3 ms, đọc lại đều đúng → +58.0 ms burst. Đầu file mất nửa giao dịch `WHO_AM_I` (trigger phần mềm của `fx2lafw`, bản ghi bắt đầu giữa giao dịch, `Sr` hiện thành `S`)
- Burst (`docs/i2c-burst-2026-10-23.png`): `S d0 3b Sr d1` + 14 byte, 13 ACK, byte cuối NACK, `P`; 1657.5 µs, 155 cạnh SCL (17 byte × 9 + Sr + P). Ghi 1 thanh ghi 292.5 µs, đọc 1 byte ~473 µs: 14 lần đọc lẻ ~6.6 ms, burst rẻ ~4 lần. Dữ liệu `c072 079b f6b4 10de 0019 004c 005e` → ax −16270, ay 1947, az −2380 (|a| ≈ 1011 mg), 33.93 °C, gyro 25 76 94
- `msleep` trên dây: `msleep(100)` → 100.8 và 110.4 ms, `msleep(50)` → 58.0 ms. `HZ=100`, jiffy 10 ms, `msleep` chỉ bảo đảm tối thiểu. Thời gian cần chính xác (tuần 5–7) không dùng `msleep`

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
7. Kernel RT: `poky/meta/recipes-kernel/linux/linux-yocto-rt_6.12.bb` cùng 6.12.47 (`KBRANCH v6.12/standard/preempt-rt/base`) nhưng `COMPATIBLE_MACHINE` chỉ có qemu, nên cần `.bbappend` trong layer (đây là dòng `.bbappend` kernel dời từ tuần 2) và `PREFERRED_PROVIDER_virtual/kernel`. **Trước khi build RT: lưu ảnh + `zImage` + dtb non-RT hiện tại** làm mốc benchmark (`~/projectb-baseline/`)
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
