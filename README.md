# Project B — Embedded Linux edge platform (BeagleBone Black + MPU6050)

## Pinned versions
| Thành phần | Giá trị |
|---|---|
| Yocto release | 5.2.4 "Walnascar" (poky tag `yocto-5.2.4`) |
| MACHINE | `beaglebone-yocto` |
| Kernel | linux-yocto 6.12 |
| Build host | container `yocto-build` (Ubuntu 24.04, user `builder` uid 1000), host Arch Linux |
| Threads | `BB_NUMBER_THREADS=3`, `PARALLEL_MAKE=-j 3` |

## Layout
- `docker/Dockerfile` — môi trường build (Ubuntu 24.04 + gói Yocto + user uid khớp host)
- `meta-projectb/` — layer của dự án
- `poky/` — upstream, **không commit** (`.gitignore`); clone tag `yocto-5.2.4`:
  `git clone --branch yocto-5.2.4 --depth 1 https://git.yoctoproject.org/poky`
- `downloads/` — cache mã nguồn (`DL_DIR`), **không commit**, có thể tải lại từ đầu

## Build

    docker build --build-arg UID=$(id -u) -t yocto-build docker/
    docker run --rm -it -v ~/workspace/VKU/DoAn:/workspace yocto-build
    # trong container:
    cd poky && source oe-init-build-env build
    bitbake core-image-minimal

Output: `poky/build/tmp/deploy/images/beaglebone-yocto/`

## Sự cố đã gặp và cách xử lý

**pseudo + tar >= 1.35 / glibc >= 2.39 (Ubuntu 24.04 container):**
`do_package` của `linux-libc-headers` lỗi `got *at() syscall for unknown directory`
vì `pseudo` mặc định của poky 5.2.4 thiếu wrapper cho `openat2()`.
Sửa bằng `meta-projectb/recipes-devtools/pseudo/pseudo_git.bbappend`:
ghim `SRCREV` mới hơn (có wrapper `openat2`) + patch `older-glibc-symbols.patch`
đã rebase theo revision đó (patch gốc của poky không áp được lên source mới).

**Đầu đọc SD rời qua USB không đáng tin cho việc flash (đã gặp trên Genesys
Logic 05e3:0747, USB3 SuperSpeed):**
`dd` một lệnh cho image lớn (~70MB) báo tốc độ phi lý (1–2 GB/s, vượt cả giới
hạn lý thuyết của USB3) và dữ liệu ghi xuống bị hỏng âm thầm — `dd` báo thành
công, checksum sau khi ghi lại sai, luôn **cùng một giá trị sai** (không phải
random). Thiết bị cũng tự reset theo chu kỳ ~30s (`usb X-Y: reset SuperSpeed
USB device`); sau đủ số lần lỗi, kernel đánh dấu **offline**
(`sd 0:0:0:0: rejecting I/O to offline device`) và từ chối mọi I/O tiếp theo —
lúc đó không lệnh `dd`/`sudo` nào cứu được, **phải rút cắm vật lý** để kernel
tạo lại SCSI instance mới.

Cách khắc phục:
1. Tăng timeout SCSI cho đúng instance hiện tại (phải làm lại mỗi lần rút
   cắm, không giữ qua các lần enumerate):
   ```
   echo 180 | sudo tee /sys/block/sdX/device/timeout
   ```
2. Ghi **từng khối 1MB** (không ghi cả file một lệnh), đọc lại ngay và so
   sánh SHA256 với đúng khối tương ứng trong file nguồn, ghi lại khối nào
   sai tới khi khớp. Xem `scripts/flash-verified.sh` (nếu có) hoặc lịch sử
   chat để lấy lại script đầy đủ.
3. Nếu gặp lỗi `No such device or address` liên tục không đổi dù đã chờ →
   kiểm tra `journalctl -k | grep -i offline`; nếu có, rút cắm lại, không
   cố `dd`/`partprobe` tiếp.
4. Sau khi ghi xong, `lsblk` trên máy build có thể **không** tự cập nhật
   bảng phân vùng mới (vẫn hiện layout cũ). Không quan trọng — BBB tự đọc
   thẻ bằng bộ điều khiển MMC riêng, độc lập hoàn toàn với máy build.

Bẫy khi tự viết script `dd` theo khối: `stat -c%s` trên một **symlink**
(ví dụ `.../rootfs.wic -> rootfs-<ts>.wic`) trả về độ dài chuỗi target,
không phải size file thật — phải dùng `stat -L -c%s`. Nếu dán script nhiều
dòng có `exit 1` trực tiếp vào shell tương tác (không qua file), `exit` sẽ
đóng luôn terminal — bọc toàn bộ script trong `( ... )` để `exit` chỉ thoát
subshell.

## Flash

    sudo bmaptool copy <image>.wic /dev/sdX   # kiểm tra kỹ bằng lsblk trước

Nếu đầu đọc không ổn định (xem mục trên), dùng cách ghi từng khối + xác minh
thay cho `bmaptool`/`dd` một lệnh.
