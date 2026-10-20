# Boot flow: BeagleBone Black + projectb-image

Đo trên bo thật, ngày 2026-10-15, cùng một thẻ SD chứa `projectb-image`.
Hai log serial ghi bằng `picocom -g`:

- log 1: `docs/boot-serial-2026-10-15.log`
- log A: `docs/boot-serial-2026-10-15-A-no-s2.log`, chứa ba lần boot liên tiếp
  (dòng 2-318, 340-390, 393-687)

Số dòng dưới đây trỏ vào hai file đó. Mục nào chưa kiểm chứng được ghi ở cuối.

## Chuỗi tổng quát

nguồn → ROM → SPL → U-Boot → kernel → init → userspace

Bo có hai nơi chứa bootloader: eMMC (đã có sẵn một bản U-Boot) và thẻ SD (`MLO` +
`u-boot.img` do ta build). Giữ nút S2 lúc cấp nguồn hay không quyết định bản nào chạy.

Cập nhật 2026-10-16: đã vô hiệu SPL trên eMMC (ghi 512 byte 0 vào sector 256, xem
`emmc-bootloader-disable.md`). Từ đó mọi lần boot, không cần S2, chạy đường 2 (SPL và U-Boot
2025.01 trên thẻ SD); log `boot-serial-2026-10-16-emmc-off.log` không còn dòng `2019.04` hay
`MMC2` nào. Mục "Hai đường boot" dưới đây ghi lại tình trạng trước khi vô hiệu eMMC, là cơ sở
của quyết định đó.

## Hai đường boot

Đường 1, không giữ S2 (log A dòng 2-318):
- `U-Boot SPL 2019.04`, `Trying to boot from MMC2` (dòng 2-3)
- `U-Boot 2019.04 ... Build: jenkins-github_Bootloader-Builder-137` (dòng 8)
- `Press SPACE to abort autoboot in 0 seconds` (dòng 30): bootdelay 0, gần như không kịp ngắt
- `Scanning mmc 0:1... Found /extlinux/extlinux.conf` (dòng 38-39): U-Boot trên eMMC
  đọc `extlinux.conf` ở thẻ SD rồi nạp kernel từ đó
- `Retrieving file: /zImage` 9046184 byte (dòng 43-44), `am335x-boneblack.dtb` (dòng 46-47)

Đường 2, giữ S2 (log A dòng 393-687, log 1 dòng 57-356):
- `U-Boot SPL 2025.01`, `Trying to boot from MMC1`
- `U-Boot 2025.01`, `Hit any key to stop autoboot: 2 1 0` (cửa sổ 2 giây)
- cùng `extlinux.conf`, cùng `zImage`, cùng dtb

SPL đánh số MMC1 là bộ điều khiển SD (`mmc0` theo kernel, log 1 dòng 320) và MMC2 là eMMC
(`mmc1`, dòng 314-319); suy ra từ đó, SPL không ghi rõ. Từ `extlinux.conf` trở đi hai đường
giống nhau: kernel, dtb, rootfs đều lấy từ thẻ SD. Chỉ bootloader khác nhau.

Hệ quả cho dự án: `MLO` và `u-boot.img` trong ảnh của ta chỉ chạy khi giữ S2. Không giữ
S2 thì bo chạy U-Boot trên eMMC, ta không kiểm soát bản đó. Thay đổi kernel, dtb,
`extlinux.conf` có hiệu lực ở cả hai đường. Thay đổi cấu hình U-Boot chỉ có hiệu lực ở
đường 2.

## Từ U-Boot tới userspace (chung cho hai đường)

Số dòng theo log 1.

1. `bootcmd`, đọc ở U-Boot 2025.01:
   ```
   bootcmd=run findfdt; run init_console; run finduuid; run distro_bootcmd
   boot_targets=mmc0 mmc1 nand0 usb0 pxe dhcp
   ```
   `distro_bootcmd` thử từng thiết bị theo `boot_targets`. Thẻ SD có mặt thì `mmc0`
   thành công, không đi tới `pxe`/`dhcp`.
2. `extlinux.conf` (dòng 78-86):
   ```
   default Yocto
   label Yocto
      kernel /zImage
      fdtdir /
   append root=PARTUUID=076c4a2a-02 rootwait console=ttyS0,115200
   ```
   `fdtdir /` để U-Boot chọn dtb theo tên bo (`am335x-boneblack.dtb`). Dòng `append`
   thành `bootargs`, nên `printenv bootargs` ở dấu nhắc báo chưa định nghĩa.
3. Nạp kernel (dòng 87-94): `zImage` ở `0x82000000`, `0x8a08a8` = 9046184 byte, đúng bằng
   file trên thẻ. dtb ở `0x88000000`. Rồi `Starting kernel ...`.
4. Kernel 6.12.47-yocto-standard (dòng 96 trở đi): command line đúng `append` (113),
   `Machine model: TI AM335x BeagleBone Black` (100). Driver: mdio+cpsw (269-275),
   UART (281), PMIC TPS65217 (285), HDMI `tda998x`/`tilcdc` (286-291), MUSB (295),
   MMC (309-322).
5. `Waiting for root device PARTUUID=076c4a2a-02` (313). `mmc1` là eMMC 3.53 GiB, `mmc0` là
   thẻ SD 29.7 GiB (314-322). Rootfs `mmcblk0p2` ext4 gắn chỉ-đọc (323-326), rồi
   `Run /sbin/init` (328).
6. (Ảnh sysvinit, trước khi chuyển sang systemd; ảnh systemd ở mục cuối phần mạng.) init là
   sysvinit 3.14 (329), do `INIT_MANAGER = "sysvinit"` mặc định của Poky. udev
   (331-333), gắn lại đọc-ghi (334), runlevel 5 (336), mạng, sshd, syslogd/klogd
   (337-347), `login:` trên `ttyS0` (349-351).

## Mạng ở hai đường

| Lần | Đường | SMSC probe | Lúc `ifup` | Kết quả |
|---|---|---|---|---|
| log A boot 1 (228, 295-303) | 1 | lỗi `-5` | `Generic PHY` gắn được, `Link is Up` | lease `192.168.50.182` |
| log A boot 3 (606, 673-679) | 2 | lỗi `-5` | `phy ... not found on slave 0` | `no lease` |
| log 1 (270, 338-343) | 2 | lỗi `-5` | `phy ... not found on slave 0` | `no lease` |
| log A-D1 (228, 296, 301) | 1 | lỗi `-5` | `phy ... not found on slave 0` | `no lease` |
| log emmc-off (215, 286-305) | 2 (sau khi vô hiệu eMMC, không S2) | lỗi `-5` | `not found`; `phy-retry` lần 1 bind SMSC được | `192.168.50.182/24` sau 1 lần thử |

SMSC probe lỗi `-5` ở mọi lần. Khác nhau ở chỗ lúc `ifup` kernel có gắn được PHY
(bằng driver `Generic PHY`) hay không. Đường 1 có một lần tốt (log A boot 1) và một lần
hỏng (log A-D1); đường 2 hỏng cả hai lần. Vậy việc bootloader nào chạy không quyết định
mạng có lên hay không. Lần boot đường 1 tốt là ngoại lệ chưa giải thích được, và kết quả
trông như ngẫu nhiên. Nguyên nhân chưa biết.

## Mạng sau khi có `projectb-phy-retry`

Script `/etc/init.d/projectb-phy-retry` (S03, chạy nền, tối đa 10 lần cách nhau 5 giây)
bind lại PHY và mở lại `eth0` khi `eth0/phydev` chưa có. Năm lần boot liên tiếp, đường 1,
không giữ S2, ghi log `boot-serial-2026-10-15-A-no-s2-T1.log` tới `-T5.log`:

| Lần | Kernel gắn PHY lúc `ifup` | Số lần script phải thử | PHY gắn bằng | Kết quả |
|---|---|---|---|---|
| T1 | không | 3 | SMSC | `192.168.50.182/24` |
| T2 | không | 1 | Generic PHY | `192.168.50.182/24` |
| T3 | không | 1 | SMSC | `192.168.50.182/24` |
| T4 | không | 1 | SMSC | `192.168.50.182/24` |
| T5 | có (Generic PHY) | 0 | Generic PHY | `192.168.50.182/24` |

Cả năm lần `eth0` đều có IPv4 mà không ai can thiệp, không lần nào chạm giới hạn 10. Số
lần phải thử thay đổi từ 0 đến 3. Lỗi không tái hiện theo kiểu xác định, nên không có
số lần thử cố định đảm bảo thành công. Mẫu nhỏ (năm lần, một bo, chỉ đường 1); đường 2
chưa thử với script. Log không có mốc thời gian nên chưa đo được thời gian tới khi có IP.
Mới kiểm `ip addr`, chưa kiểm đăng nhập SSH sau từng lần boot.

## Sau khi chuyển sang systemd (2026-10-16)

Log `boot-serial-2026-10-16-extlinux-d1.log`: SPL và U-Boot 2025.01 từ thẻ SD (2-6), extlinux
(26-30), kernel cùng banner `#1 PREEMPT Mon Sep 15 02:39:40 UTC 2025` (42), cmdline đúng file
trong layer (58). Rootfs `mmcblk0p2` gắn chỉ-đọc rồi `Run /sbin/init` (268-273). init là
systemd 257.6, `-SYSVINIT` (275).

Mạng do `systemd-networkd` cấu hình, không còn `/etc/network/interfaces`:

- SMSC probe vẫn lỗi `-5` (215), như mọi lần trước.
- `Started Network Configuration` (424) mở `eth0`, và chính lúc đó kernel gắn `Generic PHY`
  (425), `Link is Up 100Mbps/Full` (438). `projectb-phy-retry.service` chỉ `Started` ở dòng 464,
  tức sau khi PHY đã gắn.
- Journal của unit: `lần 1: PHY đã gắn`, rồi `eth0 có IPv4: 192.168.50.182/24`, chạy 502 ms,
  thoát 0. Vậy lần này đường "bind lại" của script không được chạy; một mẫu duy nhất, chưa
  chứng minh unit gắn lại được PHY khi kernel không gắn.

`systemctl is-system-running` ngay sau khi đăng nhập trả `starting`, `systemctl --failed` rỗng.
Trong log không có `Reached target Multi-User System` hay `Startup finished` tới lúc ra
`login:` (473-476): còn job chưa xong. Lần kiểm sau đó `is-system-running` ra `running` và `list-jobs` rỗng, nên chỉ là
boot chưa xong lúc gõ lệnh; chưa biết unit nào chậm (cần gói `systemd-analyze`). Journal nằm
trong RAM (không có `/var/log/journal`), mất khi tắt nguồn.

Đồng hồ: RTC của bo không có pin (`setting system clock to 2000-01-01`, 200), systemd đẩy
lên mốc epoch lúc build `2025-05-29` (274), `systemd-timesyncd` có chạy (406) nhưng chưa đồng
bộ; mọi dấu thời gian trong journal lệch ~17 tháng.

## Bất thường khác

- `FAT-fs (mmcblk0p1): Volume was not properly unmounted` (log 1 dòng 330): `/boot` gắn
  đọc-ghi, và có lần bo bị cắt nguồn không qua `poweroff`.
- `tilcdc ... Cannot find any crtc or sizes` (289, 291): chưa nối màn HDMI.
- `failed to disable` của `*-clkctrl`, `ti-sysc ... error -16`: chưa tìm hiểu.

## Chưa kiểm chứng

- Log A dòng 340-390: đường 1 tới `Starting kernel ...` rồi log chuyển ngay sang
  `U-Boot SPL 2025.01`, không có dòng kernel nào. Hiểu là bo bị cắt nguồn rồi cấp lại có
  giữ S2; người làm xác nhận các lần đầu không giữ S2 và lần sau mới giữ, còn việc cắt
  nguồn đúng ở dòng đó thì chưa hỏi lại.

Đã xác nhận (2026-10-15): lần boot đầu không giữ S2 chạy đường 1 (bootloader trên eMMC),
các lần sau giữ S2 chạy đường 2 (bootloader trên thẻ SD).
- Cờ bootable của phân vùng FAT đầu (`--active` trong `beaglebone-yocto.wks`):
  kiểm bằng `fdisk -l /dev/mmcblk0`.
- `findfdt`, `init_console`, `finduuid` làm gì: kiểm bằng `printenv findfdt init_console finduuid`.
- U-Boot trên eMMC là bản của BeagleBoard.org: suy từ dòng build `jenkins-github_...`.
