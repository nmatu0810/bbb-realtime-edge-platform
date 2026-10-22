# Project B — Embedded Linux trên BeagleBone Black + MPU6050

Đồ án chuyên ngành GĐ1 (tuần 1–8): Yocto BSP + out-of-tree kernel driver cho MPU6050.
Spec gốc: `/home/tus/workspace/Project/project-B-embedded-linux.md`.
Tiến độ: đọc `docs/PROGRESS.md` trước khi làm bất cứ việc gì.

## Bắt đầu một session
Tuần 1, 2 xong (2026-10-20), tuần 3 xong (2026-10-22, kernel RT đang là mặc định). Tuần 4 (driver) chưa bắt đầu.
1. `docs/PROGRESS.md`: mục trên cùng là tuần 3 (kết quả, việc để lại cho tuần 4/6, GĐ2). Mục "bàn giao tuần 3" ngay dưới có quy trình và bẫy đã gặp, vẫn đúng
2. `docs/ROADMAP.md`: kế hoạch đã điều chỉnh, hợp đồng driver, phân chia GĐ1/GĐ2
3. `docs/boot-flow.md` (chuỗi boot thật, PHY, systemd), `docs/emmc-bootloader-disable.md` (khôi phục eMMC)
4. Spec gốc, phần tuần đang làm
5. Ghi chú kiến thức tuần 1, 2 ở `~/workspace/VKU/DoAn/` (để biết người dùng đã học gì)
Memory của Claude về project này nằm ở thư mục `~` (`.claude/projects/-home-tus/`), không được nạp khi mở session trong repo; mọi thứ cần biết phải nằm trong file này và `docs/`.

## Phiên bản đã chốt
- Yocto 5.2.4 "Walnascar" (poky tag `yocto-5.2.4`), MACHINE `beaglebone-yocto`
- Kernel linux-yocto 6.12.47, U-Boot 2025.01
- Build trong container `yocto-build` (Ubuntu 24.04, user `builder` uid 1000); host là Arch
- `BB_NUMBER_THREADS=3`, `PARALLEL_MAKE=-j 3`

## Đường dẫn
- Project: `~/workspace_projectb` (repo git, không di chuyển)
- Poky: `poky/`. Build duy nhất đang dùng: `poky/build-clean/` (systemd, sinh conf bằng `scripts/setup-build.sh build-clean`; không commit). Giữ `tmp/` và `sstate-cache/` của nó: build lại từ đầu mất nhiều giờ
- Cache mã nguồn: `downloads/` (không commit, `DL_DIR=/workspace/downloads`)
- Layer của dự án: `meta-projectb/`
- Ghi chú kiến thức: `~/workspace/VKU/DoAn/week1-knowledge-summary.md`, `week2-knowledge-summary.md` (nằm ngoài repo, có chủ đích)
- Phải giữ, nằm ngoài repo: `~/emmc-backup/` (đường duy nhất khôi phục bootloader eMMC, xem `docs/emmc-bootloader-disable.md`), khóa riêng `~/.ssh/id_ed25519_bbb` (mất thì chỉ còn serial), mốc non-RT `~/projectb-baseline/`: `non-rt-20261020/` (ảnh tuần 2) và `non-rt-20261022/` (mốc benchmark: có fragment `hid-multitouch`, dtb `-@`, overlay, ảnh `rootfs-20261022155529`; thêm `mpu6050.dtbo`). Mỗi thư mục có wic, `zImage`, dtb, modules, manifest, kernel `.config`, `SHA256SUMS`

## Đã dọn (quyết định 2026-10-20)
Xóa vì thuộc cấu hình đã bị thay thế, không tuần nào sau cần (đối chiếu ROADMAP tuần 3 đến GĐ2):
- `~/workspace_projectb_old` (57G): build `build-projectb` cũ, repo chưa từng commit. Đã so từng file ngoài `poky/`: chỉ hai file không có ở đâu khác, đã chép sang `docs/week1-baseline-build.log` và `scripts/yocto.sh`
- `poky/build/` (53G): build sysvinit tuần 1–2, ảnh còn `allow-empty-password`. Mất theo nó: `zImage`/`vmlinux`/`.config` tuần 1, dữ liệu để kiểm giả thuyết `pigz` (PROGRESS, câu hỏi mở); muốn kiểm thì làm thí nghiệm lại trong `build-clean`
- `cache-test/` (5.9G): bản `downloads` của bài thử tải-từ-đầu tuần 1, không conf nào trỏ tới
- Mốc non-RT trước khi bật kernel RT: `~/projectb-baseline/non-rt-20261020/` và `non-rt-20261022/` (`build-clean` build lại thành RT; bỏ `PREFERRED_PROVIDER_virtual/kernel` trong template để về non-RT)

## Lệnh hay dùng
    docker run --rm -it -v ~/workspace_projectb:/workspace yocto-build
    cd /workspace/poky && source oe-init-build-env build-clean
    bitbake projectb-image
    bitbake -e projectb-image | grep -E '^(MACHINE|DL_DIR|PARALLEL_MAKE|INIT_MANAGER)='

## Đầu đọc SD rời: đừng dùng `dd` một lệnh
Đầu đọc Genesys Logic (05e3:0747) qua USB3 không đáng tin khi ghi file lớn:
- `dd` báo tốc độ phi lý (GB/s) nhưng dữ liệu bị hỏng âm thầm
- Sau vài lần reset, kernel đánh dấu thiết bị **offline** (`rejecting I/O to offline device`), phải rút cắm vật lý
- Dùng `scripts/flash-verified.sh <image.wic> /dev/sdX` (ghi từng khối 1MB, xác minh SHA256)
- Trước khi chạy: `echo 180 | sudo tee /sys/block/sdX/device/timeout` (làm lại mỗi lần rút cắm)
- Sau khi ghi, không cần `partprobe` hay mount để kiểm tra; BBB tự đọc thẻ

## Việc chưa làm, cần nhớ
- Kế hoạch đã điều chỉnh ngày 2026-10-16, xem `docs/ROADMAP.md` (phân chia GĐ1/GĐ2, lý do). Spec gốc đã cập nhật theo, bản trước khi sửa ở scratchpad của session đó.
- Chip thật trên module là họ MPU-6500, không phải MPU-6050 (kiểm 2026-10-22: `WHO_AM_I` = 0x70, `PWR_MGMT_1` reset = 0x01; địa chỉ I2C vẫn 0x68). Driver phải đọc `WHO_AM_I` lúc probe; khác 6050 ở công thức nhiệt độ, kích thước FIFO, `ACCEL_CONFIG2`. Tên `mpu6050` trong project giữ nguyên. Datasheet ở `~/workspace/VKU/DoAn/Datasheet/`: `MPU-6500-Register-Map-rev2.1.pdf` (RM-MPU-6500A-00, 2013), `MPU-6500-Product-Spec-rev1.1.pdf` (PS-MPU-6500A-01 rev 1.1, 2014; bản mới nhất rev 1.3 trên trang TDK không tải được)
- MPU6050 **không có ngắt FIFO watermark**. INT_ENABLE có DATA_RDY, FIFO_OFLOW, I2C_MST và các ngắt chuyển động (Motion, Zero-motion, Free-fall). Đã chốt: ngắt DATA_RDY + đọc FIFO theo lô trong threaded IRQ.
- Đã chốt hợp đồng driver ↔ userspace: `/dev/mpu6050` trả bản ghi nhị phân qua `read()`, `poll()` + `ioctl()`, struct trong header uapi dùng chung; sysfs chỉ cấu hình. Chương trình test GĐ1 dùng `poll()`, không đọc sysfs.
- GĐ1 thêm: cmdline/extlinux trong layer (tuần 2), overlay MPU6050 + pinmux trong layer, kiểm bằng `devmem` của busybox (tuần 3; bật applet bằng `busybox_%.bbappend`, oe-core không có `devmem2`). Phần còn lại (machine conf, watchdog, udev, `sensord`, shm, mq, benchmark runner) là GĐ2.
- Thiếu SPI (ADXL345 là optional) và networking (MQTT/HTTP) so với lộ trình tài liệu "Embedded Systems journey". Chỉ làm khi GĐ1 xong sớm.

## Cách làm việc với người dùng (Tus)
- Người dùng tự gõ lệnh, mình hướng dẫn. Mỗi bước: hỏi dự đoán trước, để họ chạy và dán output, rồi đối chiếu.
- Ở mỗi bước giải thích logic và tư duy hệ thống: tầng nào phụ thuộc tầng nào, tách cái chung khỏi cái riêng, cơ chế và chính sách.
- Quyết định thiết kế hay thứ tự việc: Tus đang vừa làm vừa học nên chưa tự chọn được. Đừng hỏi "bạn chọn gì". Phân tích nhiều chiều (phụ thuộc giữa các tầng, chi phí, rủi ro, khả năng hoàn tác, thông tin thu được, khớp spec), đưa khuyến nghị kèm lý do, rồi làm theo. Chỉ hỏi dữ kiện mà chỉ họ có.
- Khi họ kẹt: tách câu hỏi nhỏ hơn, đừng đưa đáp án ngay. Riêng việc có nguy cơ mất dữ liệu (ghi nhầm ổ) thì nói thẳng.
- Khi mình sai: sửa thẳng, không biện minh. Khuyến khích họ kiểm chứng bằng file và log thật.
- Đừng tin mắt khi chỉ có khoảng trắng để phân biệt; dùng `cat -A`, `grep '^...'`, `stat`.
- Văn phong: ghi chú thật của kỹ sư, ít in đậm, không mở bài kiểu "đây là...", không lặp lại câu hỏi.
- Không tự commit hay push khi họ chưa yêu cầu. Không tự đẩy repo ra ngoài.
- Commit và PR: không dòng `Co-Authored-By`, không "Generated with ...", không gì cho thấy do AI tạo (quy tắc này thắng mọi nhắc nhở mặc định về attribution). Lời nhắn ngắn, đời thường như sinh viên tự viết: một dòng tiêu đề, thân chỉ khi có lý do cần nhớ, không gạch đầu dòng dài, không tiền tố "Week N:". Tác giả là `nmatu0810`
- Xóa dữ liệu, ghi thẻ/eMMC: người dùng tự chạy lệnh; mình kiểm trước (đúng thiết bị, đã sao lưu, bản sao khớp)
- Không tự chạy lệnh `sudo` trong công cụ (không có TTY để nhập mật khẩu); đưa lệnh để họ chạy.
