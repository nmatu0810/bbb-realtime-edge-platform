#!/bin/sh
# Tự kiểm MPU6050 lúc boot (tuần 8), không cần thao tác tay. systemd chạy khi
# /dev/mpu6050 xuất hiện (udev rule 90-mpu6050.rules), tức driver đã tự nạp và probe.
# Kết quả: journalctl -u mpu6050-boot-check, và /var/log/mpu6050-boot-check.log
# (volatile, mất khi reboot). Mã thoát khác 0 = unit failed: systemctl --failed thấy.
#
# Một lượt: selftest API (mpu6050-test -t, mã thoát = số FAIL), rồi đo 1000 bản ghi ở
# 100 Hz; đạt khi selftest 0 FAIL, đủ 1000 bản ghi, dropped không tăng.
LOG=/var/log/mpu6050-boot-check.log
SYS=/sys/class/misc/mpu6050
N=1000

check() {
	echo "kernel $(uname -r), $(cut -d' ' -f1 /proc/uptime) s sau boot"
	echo "driver $(basename "$(readlink /sys/bus/i2c/devices/2-0068/driver)")," \
		"/dev/mpu6050 $(stat -c '%A %U:%G' /dev/mpu6050)"
	mpu6050-test -t
	fails=$?
	echo "selftest: $fails FAIL"

	echo 100 > $SYS/sample_rate || return 1
	d0=$(cat $SYS/dropped)
	out=$(mpu6050-test -n $N)
	echo "$out" | tail -n 2
	d1=$(cat $SYS/dropped)
	got=$(echo "$out" | awk '/^total/ { print $2 }')
	echo "stream: ${got:-0}/$N bản ghi, dropped +$((d1 - d0))"

	[ "$fails" -eq 0 ] && [ "${got:-0}" -ge $N ] && [ "$d1" -eq "$d0" ]
}

mkdir -p "$(dirname $LOG)"
check > $LOG 2>&1
rc=$?
[ $rc -eq 0 ] && echo "KẾT QUẢ: ĐẠT" >> $LOG || echo "KẾT QUẢ: KHÔNG ĐẠT" >> $LOG
cat $LOG
exit $rc
