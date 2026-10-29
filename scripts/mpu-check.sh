#!/bin/sh
# Kiểm API /dev/mpu6050 trên bo (tuần 5): chép lên rồi sh /tmp/mpu-check.sh <phần>
#   shake    app 8 s, nghiêng module trong lúc chạy
#   cfg      app 14 s; giây 4 đổi 200 Hz, giây 9 đổi +-4 g; cuối trả về 100 Hz +-2 g
#   strace   nhịp poll/read lúc ổn định (bỏ 10 dòng đầu)
#   restart  cat chặn trong read (4 Hz), SIGSTOP rồi SIGCONT: thấy ERESTARTSYS
#   gap      strace -T một lần ghi sysfs, lúc không mở và lúc đang stream
SYS=/sys/class/misc/mpu6050
APP=/tmp/mpu6050-test
ST=/tmp/strace

case "$1" in
shake)
	$APP -n 800
	;;
cfg)
	$APP -n 2400 &
	p=$!
	sleep 4; echo 200 > $SYS/sample_rate; echo "--- sample_rate 200"
	sleep 5; echo 4 > $SYS/accel_range; echo "--- accel_range 4"
	wait $p
	echo 100 > $SYS/sample_rate; echo 2 > $SYS/accel_range
	echo "restored: $(cat $SYS/sample_rate) Hz, $(cat $SYS/accel_range) g"
	;;
strace)
	$ST -e trace=poll,read $APP -n 60 2>&1 | grep -E '^(poll|read)\(' | grep -v ELF | sed -n '11,24p' | cut -c1-100
	;;
restart)
	# 4 Hz: mỗi read chờ ~250 ms, cat gần như chắc đang ngủ trong read lúc nhận
	# SIGSTOP. Ra /dev/null để không kẹt ở write ra terminal.
	echo 4 > $SYS/sample_rate
	$ST -e trace=read -o /tmp/st.txt cat /dev/mpu6050 > /dev/null &
	sleep 1
	c=$(pidof cat)
	kill -STOP $c; sleep 1; kill -CONT $c; sleep 1; kill $c
	sleep 1
	echo 100 > $SYS/sample_rate
	grep -n -B1 -A3 'ERESTART\|SIGSTOP\|SIGCONT' /tmp/st.txt | cut -c1-110 | head -n 20
	;;
gap)
	# Thời gian một lần store sysfs: không ai mở, rồi đang stream 200 Hz.
	t() { $ST -T -e trace=write sh -c "echo $2 > $SYS/$1" 2>&1 | grep '^write(1'; }
	echo "-- idle"
	t sample_rate 200; t sample_rate 100; t accel_range 4; t accel_range 2
	$APP -n 1000 > /dev/null &
	p=$!
	sleep 2
	echo "-- streaming"
	t sample_rate 200; t sample_rate 100; t accel_range 4; t accel_range 2
	wait $p
	;;
*)
	echo "usage: $0 shake|cfg|strace|restart|gap" >&2
	exit 2
	;;
esac
