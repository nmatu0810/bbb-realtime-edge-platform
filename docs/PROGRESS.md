# Nhật ký tiến độ

Mỗi session kết thúc ghi thêm một mục ở đầu file (mới nhất lên trên).
Mục gồm: đã làm, kết quả đo được, việc tiếp theo, câu hỏi còn mở, commit tương ứng.

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
