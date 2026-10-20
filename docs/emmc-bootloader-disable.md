# Vô hiệu bootloader trên eMMC

Mục tiêu: bo boot ra U-Boot 2025.01 trên thẻ SD mà không cần giữ S2, để `MLO`, `u-boot.img`
và cấu hình U-Boot do ta build có hiệu lực ở mọi lần boot (xem `boot-flow.md`, hai đường boot).
Làm trước tuần 3, vì overlay device tree dựa vào U-Boot nạp qua `extlinux`.

## Hiện trạng đo được (2026-10-16)

Dữ liệu thô nằm ngoài repo, ở `~/emmc-backup/` (ngoài git, có chủ đích).

- eMMC là `mmcblk1`: `type=MMC`, 7405568 sector (3.53 GiB). Thẻ SD là `mmcblk0`: `type=SD`,
  62333952 sector. Định danh dựa vào loại và số sector, không chỉ tên.
- `ext_csd` (`/sys/kernel/debug/mmc1/mmc1:0001/ext_csd`): `PARTITION_CONFIG` = 0x00, tức
  `BOOT_PARTITION_ENABLE` = 0. eMMC không khởi động từ hai phân vùng boot cứng, và `boot0`,
  `boot1` (mỗi cái 2 MiB) toàn byte 0.
- Bảng phân vùng (MBR): một phân vùng Linux (`83`, cờ boot) bắt đầu ở sector 8192, không có
  phân vùng FAT. ROM không tìm thấy `MLO` dưới dạng file.
- SPL nằm dạng raw ở offset 0x20000 (128 KiB, sector 256). 32 byte đầu của khối này giống hệt
  32 byte đầu của `MLO` ta build (`40000000 0c000000 ... CHSETTINGS`), nên đó là header ROM
  cùng loại.
- `u-boot.img` nằm raw ở offset 0x60000 (384 KiB): magic `27051956` (legacy uImage), thời
  điểm đóng gói 2020-03-06 (khớp banner `U-Boot 2019.04 (Mar 06 2020)`), kích thước 476360 byte,
  địa chỉ nạp `0x80800000`.
- Khối 0x40000 và vùng từ 0xe0000 trở đi trong 4 MiB đầu toàn byte 0.

## Thay đổi tối thiểu

Ghi 512 byte 0 vào sector 256 của `/dev/mmcblk1` (offset 0x20000), tức làm hỏng header SPL.
Không động vào sector 0 (bảng phân vùng), không động vào phân vùng 1, không động vào
`u-boot.img` ở 0x60000. ROM không tìm thấy ảnh hợp lệ trên eMMC nên chuyển sang thiết bị kế
tiếp trong thứ tự khởi động, là thẻ SD.

Chưa kiểm chứng: ROM bỏ qua khối 0x60000 (không phải header ROM hợp lệ) và chuyển sang thẻ SD.
Bước 3 sẽ cho biết.

## Quy trình

Mọi lệnh ghi xuống eMMC chỉ chạy sau khi bước 1 xong và checksum khớp. Hàm `bbb` dùng ở dưới:

    bbb() { ssh -i ~/.ssh/id_ed25519_bbb -o IdentitiesOnly=yes root@192.168.50.182 "$@"; }

1. Sao lưu toàn bộ vùng người dùng của eMMC ra laptop, rồi so checksum hai phía. Tách riêng
   sector 256 ra file nhỏ để khôi phục nhanh.
2. Kiểm tra trước khi ghi: định danh thiết bị (loại, số sector, không được mount), nội dung
   sector 256 trên eMMC đúng bằng bản sao lưu. Ghi 512 byte 0, đọc lại, so với bản sao lưu:
   chỉ sector 256 được khác.
3. Kiểm boot: `poweroff`, mở `picocom -g` ghi log, cấp nguồn không giữ S2. Mong đợi dòng đầu
   là `U-Boot SPL 2025.01` và `Trying to boot from MMC1`.
4. Ghi kết quả vào `boot-flow.md` và `PROGRESS.md`.

## Hoàn tác

Chạy từ Linux đang boot bằng thẻ SD (hoạt động cả khi không giữ S2 sau khi vô hiệu eMMC):
ghi lại 512 byte đã sao lưu vào sector 256 của `/dev/mmcblk1`. Nếu thẻ SD cũng không boot
được: ROM còn thử UART0 rồi USB, và thẻ tuần 1 (giữ nguyên) là đường vào Linux để khôi phục.

## Rủi ro đã cân nhắc

- Hệ điều hành cũ trên eMMC không boot được nữa. Đây là mục đích, có bản sao lưu đầy đủ.
- Thẻ SD hỏng thì không còn bootloader dự phòng trên eMMC. Giảm bằng thẻ tuần 1.
- Ghi sai thiết bị. Giảm bằng cách kiểm loại và số sector ngay trước lệnh ghi, và chỉ ghi
  vào `/dev/mmcblk1` với `seek` đã xác định.

## Kết quả (2026-10-16)

Đã làm đủ bốn bước, theo thứ tự, mỗi bước kiểm bằng dữ liệu đọc lại.

1. Sao lưu: `~/emmc-backup/emmc-user.img`, 3791650816 byte (= 7405568 × 512). `sha256`
   đọc thẳng từ `/dev/mmcblk1` và từ file trùng nhau:
   `bfad3d8e7ed11eb02b57e086a8f80aa5011a38932148b2d2924347bec6cbb926`.
   Kèm `boot0.bin`, `boot1.bin` (toàn byte 0), `ext_csd.txt`, `head-4MiB.bin`, và
   `sector256.bin` (512 byte gốc để khôi phục, `sha256` `21a54243...3868`).
2. Kiểm tra trước khi ghi: `type=MMC`, 7405568 sector, không mount, sector 256 đúng bằng
   bản sao lưu. Lần ghi đầu không chạy được vì `dd` của busybox không có `conv=fsync`
   (in cách dùng rồi thoát trước khi mở thiết bị; đã đọc lại chứng minh chưa ghi gì).
3. Ghi 512 byte 0 vào sector 256, rồi `sync`. Đọc lại sau `drop_caches`: `sha256` của sector là
   của 512 byte 0 (`076a27c7...6560`); trong 4 MiB đầu chỉ 49 byte đổi, vị trí 131073 đến
   131142, tức nằm trong sector 256.
4. Kiểm boot, không giữ S2: log `boot-serial-2026-10-16-emmc-off.log` mở đầu bằng
   `U-Boot SPL 2025.01`, `Trying to boot from MMC1` (dòng 2-3), rồi `U-Boot 2025.01`,
   `extlinux`, kernel, `login:`. Không có `2019.04` hay `MMC2`. Đã kiểm chứng điều chưa
   biết ở trên: ROM bỏ qua khối `u-boot.img` ở 0x60000 và chuyển sang thẻ SD.

Khôi phục khi cần, từ Linux đang chạy. Phải đưa `sector256.bin` lên BBB trước (chưa làm), ví dụ
`scp -i ~/.ssh/id_ed25519_bbb ~/emmc-backup/sector256.bin root@192.168.50.182:/root/`:

    dd if=sector256.bin of=/dev/mmcblk1 bs=512 seek=256 count=1 && sync

Kiểm trước đó: `type=MMC`, 7405568 sector, không mount `mmcblk1`. Chưa thử khôi phục thật.
