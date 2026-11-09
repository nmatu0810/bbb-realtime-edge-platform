#!/bin/sh
# Chạy trên bo: đo mpu6050-hmi ở các tốc độ, có/không stress-ng. Xong về 100 Hz, để app chạy.
#   sh /tmp/hmi-matrix.sh [giây mỗi điều kiện]
SEC=${1:-30}
RATE=/sys/class/misc/mpu6050/sample_rate

stop_app() {
	p=$(pidof mpu6050-hmi)
	[ -n "$p" ] || return 0
	kill "$p"
	while [ -n "$(pidof mpu6050-hmi)" ]; do sleep 1; done
}

run() {	# rate load
	stop_app
	echo "$1" > "$RATE"
	echo "== rate=$1 stress=$2"
	if [ "$2" = 1 ]; then
		stress-ng --cpu 1 --timeout $((SEC + 6))s >/dev/null 2>&1 &
		s=$!
	fi
	sh /tmp/hmi-measure.sh "$SEC"
	[ "$2" = 1 ] && wait "$s"
}

run 100 1
run 500 0
run 500 1
stop_app
echo "== thoat sach: app da tat, thu mo lai thiet bi"
mpu6050-test -n 5 2>&1 | tail -2
echo 100 > "$RATE"
setsid /tmp/mpu6050-hmi >/tmp/hmi.log 2>&1 </dev/null &
sleep 2
echo "app chay lai pid=$(pidof mpu6050-hmi) rate=$(cat $RATE) dropped=$(cat /sys/class/misc/mpu6050/dropped)"
