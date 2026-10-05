# Nhật ký tiến độ

Mỗi session kết thúc ghi thêm một mục ở đầu file (mới nhất lên trên).
Mục gồm: đã làm, kết quả đo được, việc tiếp theo, câu hỏi còn mở, commit tương ứng.

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

**Commit**: chưa commit `scripts/setup-build.sh`, `scripts/templates/`, README

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
