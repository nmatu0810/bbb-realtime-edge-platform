# Nhật ký tiến độ

Mỗi session kết thúc ghi thêm một mục ở đầu file (mới nhất lên trên).
Mục gồm: đã làm, kết quả đo được, việc tiếp theo, câu hỏi còn mở, commit tương ứng.

---

## Tuần 2 — image riêng, SSH bằng khóa, boot flow (2026-10-15 đến 10-16)

**Đã làm**
- `projectb-image` (kế thừa `core-image-minimal`): `ssh-server-openssh`, `i2c-tools`, `gdbserver`, `stress-ng`, `rt-tests`
- `projectb-ssh-keys`: khóa công khai `id_ed25519_bbb` vào `/home/root/.ssh/authorized_keys` (0700/0600, chủ root). Khóa riêng ở laptop
- `projectb-phy-retry`: script sysvinit (S03, chạy nền) bind lại PHY, tối đa 10 lần cách nhau 5 giây
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
1. Build sạch: `setup-build.sh build-clean` sau khi xóa `tmp`, `sstate-cache`, `cache` cũ; `bitbake projectb-image`; so manifest và `zImage` với bản `build`. Đồng thời kiểm `LAYERDEPENDS` mới
2. extlinux/cmdline trong layer: `.wks` riêng với `bootloader --configfile`, `WKS_FILE` trỏ vào
3. Busybox `devmem`: `busybox_%.bbappend` + config fragment (thay `devmem2`, oe-core không có)
4. Chuyển sang systemd: `INIT_MANAGER = "systemd"` trong template `local.conf`; `ROOT_HOME` đổi thành `/root`; `init-ifupdown` không còn nên phải dùng networkd; viết lại `projectb-phy-retry` thành unit
5. Vô hiệu bootloader trên eMMC, làm trước tuần 3: sao lưu eMMC (vùng người dùng, `boot0`, `boot1`), xác định MLO nằm ở đâu, vô hiệu, kiểm boot không giữ S2 ra U-Boot 2025.01, có đường khôi phục. Kiểm đúng thiết bị trước mỗi lệnh ghi
6. Dọn: bỏ `allow-empty-password` (giữ `empty-root-password` cho serial), quyết định có giữ `g_ncm` không

**Câu hỏi còn mở**
- Vì sao PHY không ACK MDIO lúc probe đầu (cần đo MDIO/nRST bằng logic analyzer)
- Vì sao gadget NCM không truyền dữ liệu
- `.wic` khác giữa hai build tuần 1 (chưa điều tra)
- Layer build độc lập: chưa chạy kiểm
- Sau khi sang systemd, mạng lên bằng cơ chế nào (chưa kiểm `systemd-networkd`)

**Commit**: `ad450a6` (recipes). Tài liệu và log ở commit liền sau.

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

**Commit**: `6425e78` (`scripts/setup-build.sh`, `scripts/templates/`, README)

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
