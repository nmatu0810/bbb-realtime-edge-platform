#!/bin/sh
# Kịch bản demo GĐ1 (tuần 8), chạy trên bo từ ảnh nộp:
#   ssh -t -i ~/.ssh/id_ed25519_bbb root@192.168.50.182 sh /tmp/demo.sh
# (chép lên trước: scripts/bo.sh 'cat > /tmp/demo.sh' < scripts/demo.sh)
# Mỗi bước in tiêu đề, chờ Enter. Khoảng 6 phút. Bước 5 cần rút/cắm SDA, bỏ qua được (s).
SYS=/sys/class/misc/mpu6050
DRV=/sys/bus/i2c/drivers/mpu6050

step() {
	printf '\n=== %s\n' "$1"
	printf '[Enter: chạy, s: bỏ qua] '
	read a
	[ "$a" != s ]
}

step "1. Ảnh và kernel: RT không debug, overlay 400 kHz, driver tự nạp" && {
	uname -r
	grep -E 'fdtoverlays|append' /boot/extlinux/extlinux.conf 2> /dev/null ||
		echo "(phân vùng boot không mount; extlinux.conf trong layer: meta-projectb/wic/)"
	# thuộc tính DT là u32 big-endian; od của busybox thiếu -A/-t, dùng hexdump -e
	echo "i2c2 clock-frequency: $(printf %d 0x$(hexdump -v -e '1/1 "%02x"' /sys/bus/i2c/devices/i2c-2/of_node/clock-frequency)) Hz"
	lsmod | grep mpu6050
	dmesg | grep 'mpu6050 2-0068: MPU'
	ls -l /dev/mpu6050
}

step "2. Tự kiểm lúc boot (udev kéo unit systemd khi /dev/mpu6050 xuất hiện)" && {
	# Không dùng systemctl status: bo không có RTC, unit chạy trước khi timesyncd chỉnh
	# giờ nên mốc "since" lệch cả năm.
	systemctl show mpu6050-boot-check -p ActiveState -p Result -p ExecMainStatus
	journalctl -b -u mpu6050-boot-check --no-pager -o cat | grep -vE '^PASS'
}

step "3. Đọc liên tục 10 s ở 100 Hz: nghiêng module trong lúc chạy" && {
	echo 100 > $SYS/sample_rate
	mpu6050-test -n 1000
}

step "4. Đổi cấu hình qua sysfs lúc đang đọc: giây 3 lên 500 Hz, giây 6 ±8 g" && {
	mpu6050-test -n 4000 & p=$!
	sleep 3; echo 500 > $SYS/sample_rate; echo "--- sample_rate $(cat $SYS/sample_rate)"
	sleep 3; echo 8 > $SYS/accel_range; echo "--- accel_range $(cat $SYS/accel_range)"
	wait $p
	echo 100 > $SYS/sample_rate; echo 2 > $SYS/accel_range
}

step "5. Bus hỏng khi đang đọc: rút SDA ~5 s rồi cắm lại; dropped tăng ~5 s × 100, tự hồi" && {
	echo "dropped trước: $(cat $SYS/dropped)"
	mpu6050-test & p=$!
	printf 'Rút SDA bây giờ, cắm lại, rồi Enter để dừng app. '
	read a
	kill -INT $p; wait $p
	echo "dropped sau: $(cat $SYS/dropped)"
	dmesg | grep mpu6050 | tail -n 4
}

step "6. Gỡ driver khi app đang đọc (unbind): app nhận ENODEV và thoát, bind lại" && {
	mpu6050-test > /tmp/demo-unbind.txt 2>&1 & p=$!
	sleep 2
	echo 2-0068 > $DRV/unbind
	wait $p; echo "app thoát, mã $?"; tail -n 2 /tmp/demo-unbind.txt
	echo 2-0068 > $DRV/bind
	ls -l /dev/mpu6050; mpu6050-test -t | tail -n 1
}

echo
echo "Hết demo."
