#!/usr/bin/env bash
# Ghi image xuống thẻ SD theo từng khối 1MB, đọc lại ngay và so khớp SHA256
# với file nguồn; ghi lại khối nào sai tới khi khớp (tối đa 10 lần/khối).
# Dùng khi đầu đọc SD qua USB không đáng tin (dd một lệnh báo "thành công"
# nhưng dữ liệu bị hỏng âm thầm) — xem mục "Sự cố đã gặp" trong README.
#
# Dùng: ./scripts/flash-verified.sh <image.wic> /dev/sdX
#
# Trước khi chạy, nếu gặp lỗi "No such device or address" dai dẳng, kiểm tra
# `journalctl -k | grep -i offline`. Nếu thấy "rejecting I/O to offline
# device", RÚT CẮM VẬT LÝ đầu đọc rồi chạy lại — không có lệnh nào sửa được
# trạng thái offline từ phần mềm. Sau khi cắm lại, tăng timeout SCSI trước:
#   echo 180 | sudo tee /sys/block/sdX/device/timeout

set -u

IMG="${1:?Usage: $0 <image.wic> </dev/sdX>}"
DEV="${2:?Usage: $0 <image.wic> </dev/sdX>}"
CHUNK=1048576   # 1 MiB

SIZE=$(stat -L -c%s "$IMG")   # -L bắt buộc nếu $IMG là symlink
echo "IMG=$IMG SIZE=$SIZE DEV=$DEV CHUNK=$CHUNK"
COUNT=$(( (SIZE + CHUNK - 1) / CHUNK ))
echo "COUNT=$COUNT"

for ((i = 0; i < COUNT; i++)); do
  # Khối cuối thường ngắn hơn CHUNK — phải dùng đúng kích thước còn lại,
  # nếu không khi đọc lại từ DEV (lớn hơn file) sẽ dính thêm dữ liệu rác
  # phía sau, hai bên lệch độ dài và luôn báo sai dù ghi đúng.
  offset=$(( i * CHUNK ))
  remain=$(( SIZE - offset ))
  bs=$CHUNK
  if [ "$remain" -lt "$CHUNK" ]; then
    bs=$remain
  fi

  ok=0
  for attempt in 1 2 3 4 5 6 7 8 9 10; do
    while [ ! -e "$DEV" ]; do
      echo "cho $DEV xuat hien..."
      sleep 2
    done
    sudo dd if="$IMG" of="$DEV" bs="$bs" skip="$i" seek="$i" count=1 \
      status=none conv=notrunc
    sync
    a=$(dd if="$IMG" bs="$bs" skip="$i" count=1 status=none 2>/dev/null \
      | sha256sum | cut -d' ' -f1)
    b=$(sudo dd if="$DEV" bs="$bs" skip="$i" count=1 status=none 2>/dev/null \
      | sha256sum | cut -d' ' -f1)
    if [ "$a" = "$b" ]; then
      echo "chunk $i/$COUNT OK"
      ok=1
      break
    fi
    echo "chunk $i/$COUNT MISMATCH, thu lai lan $attempt"
    sleep 2
  done

  if [ "$ok" -ne 1 ]; then
    echo "chunk $i THAT BAI sau 10 lan - dung lai"
    echo "Kiem tra: journalctl -k | grep -i offline"
    exit 1
  fi
done

echo "Tat ca $COUNT khoi da ghi dung"
