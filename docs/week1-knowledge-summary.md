# Tổng kết kiến thức tuần 1

Ghi lại những gì đã gặp thật trong tuần 1, không phải lý thuyết đọc trước.

## Yocto / BitBake

- Layer nằm trong thư mục khác với layer được bật. Cái quyết định là `BBLAYERS` trong `build/conf/bblayers.conf`.
- Layer `meta` (core) chứa cách build, dùng chung cho mọi board. Layer `meta-yocto-bsp` chỉ chứa lựa chọn/thông số riêng của board (`PREFERRED_PROVIDER`, `UBOOT_MACHINE`, `KERNEL_IMAGETYPE`...).
- `.bbappend` sửa recipe từ ngoài, không đụng `poky/`. Đã làm với `pseudo_git.bbappend`.
- `FILESEXTRAPATHS:prepend` cần có nếu `.bbappend` thêm file cục bộ trùng tên trong `SRC_URI`.
- `BB_NUMBER_THREADS` × `PARALLEL_MAKE` là số tiến trình build cùng lúc, nhân nhau chứ không cộng.
- `source oe-init-build-env build` chỉ sửa PATH/biến môi trường của shell hiện tại, mất khi đóng shell. File trong `conf/` thì ở lại trên đĩa.
- `bitbake -e <target> | grep ^VAR=` để kiểm cấu hình trước khi build, đỡ phải chờ hàng giờ mới biết sai.
- Đổi một công cụ nền tảng như `pseudo` làm `Sstate summary` rớt xuống 0% match — phần lớn task phải build lại.

## Docker

- `ENV a=1 \` rồi dòng sau bắt đầu bằng dấu cách: dấu `\` thoát luôn dấu cách đó, biến nó thành một phần tên biến. Không báo lỗi gì, chỉ lộ ra khi công cụ khác đọc sai giá trị.
- `useradd -m -u $(id -u)` để file tạo trong container đúng chủ trên host, khỏi phải `sudo rm` sau.
- Mỗi `RUN` là một layer cache riêng, sửa dòng nào thì mọi dòng sau nó build lại từ đầu.

## Boot flow AM335x

```
ROM code → ký tự đơn (ví dụ C cho MMC) → MLO (SPL) → U-Boot → zImage + dtb → kernel → init
```

Dừng ở đâu cho biết lỗi ở tầng đó. Chỉ thấy `C` → ROM chưa tải được MLO. Thấy U-Boot đầy đủ rồi im ở `Starting kernel...` → nội dung zImage/dtb sai hoặc CPU không chạy được.

Watchdog: U-Boot tự nuôi watchdog trong lúc nó còn chạy. Nhường quyền cho kernel rồi, nếu kernel crash sớm trước khi driver watchdog kịp probe, sau 60s cả chip bị reset. Trông giống treo máy nhưng có chu kỳ cố định.

`earlycon` in log sớm hơn driver serial bình thường, dùng để nghi crash xảy ra trước cả lúc console kịp lên. Đèn LED heartbeat (D2 trên BBB) cũng là tín hiệu miễn phí để biết kernel có chạy được chút nào không.

## Tính toàn vẹn dữ liệu

Bài học tốn thời gian nhất của tuần này: `dd` báo thành công không có nghĩa dữ liệu đúng. Phải kiểm checksum sau khi ghi, đặc biệt qua USB không ổn định.

- Tốc độ `dd` vượt giới hạn vật lý của USB (báo GB/s) là dấu hiệu dữ liệu đang nằm trong cache, chưa tới phần cứng thật.
- `max_sectors_kb` trong `/sys/block/sdX/queue/` là giới hạn kích thước mỗi lần truyền. Ghi đúng bằng giới hạn này dễ chạm lỗi biên của firmware rẻ.
- Kernel có cơ chế đánh dấu thiết bị SCSI "offline" sau quá nhiều lỗi liên tiếp (`rejecting I/O to offline device`), từ chối mọi I/O sau đó. Không lệnh nào sửa được, phải rút cắm vật lý để tạo lại instance mới. Tăng `/sys/block/sdX/device/timeout` giúp giảm khả năng rơi vào tình trạng này.
- Cách đối phó đường truyền không ổn định: ghi từng khối nhỏ, xác minh ngay, chỉ ghi lại khối sai — không ghi lại cả file. Script ở `scripts/flash-verified.sh`.

## Shell scripting

- `stat -c%s` trên symlink trả về độ dài chuỗi target, không phải size file thật. Cần thêm `-L`.
- `exit` trong script dán trực tiếp vào shell tương tác (không chạy qua file) sẽ đóng cả terminal. Bọc trong `( ... )` hoặc lưu thành file riêng rồi chạy.
- Hai glob trong một lệnh zsh, nếu một mẫu không khớp thì cả lệnh bị hủy trước khi chạy. Tách riêng từng lệnh để biết đúng cái nào không khớp.

## Cách debug chung

1. Xem lỗi là của chương trình nào, tầng nào.
2. Giả thuyết có thể sai, chưa phải kết luận.
3. Kiểm bằng dữ liệu thật trước khi tin, kể cả lời của mình.
4. Thử quy mô nhỏ trước khi tin quy mô lớn.
5. Giả thuyết bị bằng chứng mới bác thì sửa ngay, không giữ.
6. "Chạy xong" khác "chạy đúng".
