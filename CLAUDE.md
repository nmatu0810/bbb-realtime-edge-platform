# Project B — Embedded Linux trên BeagleBone Black + MPU6050

Đồ án chuyên ngành GĐ1 (tuần 1–8): Yocto BSP + out-of-tree kernel driver cho MPU6050.
Spec gốc: `/home/tus/workspace/Project/project-B-embedded-linux.md`.
Tiến độ: đọc `docs/PROGRESS.md` trước khi làm bất cứ việc gì.

## Phiên bản đã chốt
- Yocto 5.2.4 "Walnascar" (poky tag `yocto-5.2.4`), MACHINE `beaglebone-yocto`
- Kernel linux-yocto 6.12.47, U-Boot 2025.01
- Build trong container `yocto-build` (Ubuntu 24.04, user `builder` uid 1000); host là Arch
- `BB_NUMBER_THREADS=3`, `PARALLEL_MAKE=-j 3`

## Đường dẫn
- Project: `~/workspace_projectb` (repo git, không di chuyển)
- Poky + build: `poky/`, `poky/build/` (không commit)
- Cache mã nguồn: `downloads/` (không commit, `DL_DIR=/workspace/downloads`)
- Layer của dự án: `meta-projectb/`
- Ghi chú tuần 1: `~/workspace/VKU/DoAn/week1-knowledge-summary.md` (nằm ngoài repo, có chủ đích)
- Build cũ tham chiếu: `~/workspace_projectb_old` (chỉ đọc, không dùng để build lại)

## Lệnh hay dùng
    docker run --rm -it -v ~/workspace_projectb:/workspace yocto-build
    cd poky && source oe-init-build-env build
    bitbake core-image-minimal
    bitbake -e core-image-minimal | grep -E '^(MACHINE|DL_DIR|PARALLEL_MAKE)='

## Đầu đọc SD rời: đừng dùng `dd` một lệnh
Đầu đọc Genesys Logic (05e3:0747) qua USB3 không đáng tin khi ghi file lớn:
- `dd` báo tốc độ phi lý (GB/s) nhưng dữ liệu bị hỏng âm thầm
- Sau vài lần reset, kernel đánh dấu thiết bị **offline** (`rejecting I/O to offline device`), phải rút cắm vật lý
- Dùng `scripts/flash-verified.sh <image.wic> /dev/sdX` (ghi từng khối 1MB, xác minh SHA256)
- Trước khi chạy: `echo 180 | sudo tee /sys/block/sdX/device/timeout` (làm lại mỗi lần rút cắm)
- Sau khi ghi, không cần `partprobe` hay mount để kiểm tra; BBB tự đọc thẻ

## Việc chưa làm, cần nhớ
- Kế hoạch đã điều chỉnh ngày 2026-10-16, xem `docs/ROADMAP.md` (phân chia GĐ1/GĐ2, lý do). Spec gốc đã cập nhật theo, bản trước khi sửa ở scratchpad của session đó.
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
- Không tự chạy lệnh `sudo` trong công cụ (không có TTY để nhập mật khẩu); đưa lệnh để họ chạy.
