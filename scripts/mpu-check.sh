#!/bin/sh
# Kiểm API /dev/mpu6050 trên bo (tuần 5): chép lên rồi sh /tmp/mpu-check.sh <phần>
#   shake    app 8 s, nghiêng module trong lúc chạy
#   cfg      app 14 s; giây 4 đổi 200 Hz, giây 9 đổi +-4 g; cuối trả về 100 Hz +-2 g
#   strace   nhịp poll/read lúc ổn định (bỏ 10 dòng đầu)
#   restart  cat chặn trong read (4 Hz), SIGSTOP rồi SIGCONT: thấy ERESTARTSYS
#   gap      strace -T một lần ghi sysfs, lúc không mở và lúc đang stream
# Tuần 6 (ngắt DATA_RDY):
#   irq      thread IRQ (tên, chính sách, ưu tiên), rồi đo 10 s ở 100 Hz
#   dup [n]  dd n bản ghi (mặc định 3000), đếm bản ghi liền nhau trùng 8 byte dữ liệu
#   sweep    đo 10 s mỗi tốc độ 100 200 333 500 1000 Hz
# Dòng đo: bản ghi/s app nhận, dropped/s, ngắt/s (irq 50), dư = ngắt - bản ghi - dropped
# (phải ~0; vài bản ghi còn trong kfifo lúc đóng), CPU rảnh %.
SYS=/sys/class/misc/mpu6050
APP=/tmp/mpu6050-test
ST=/tmp/strace

irqcnt() { awk '$NF == "mpu6050" { print $2 }' /proc/interrupts; }
cpuidle() { awk '/^cpu / { print $5, $2 + $3 + $4 + $5 + $6 + $7 + $8 }' /proc/stat; }
uptime_cs() { awk '{ split($1, a, "."); print a[1] * 100 + a[2] }' /proc/uptime; }

# measure <hz> <giây>: app chạy nền trong khoảng đó, so ba bộ đếm.
measure() {
	echo $1 > $SYS/sample_rate
	hz=$(cat $SYS/sample_rate)
	sleep 1
	i0=$(irqcnt); d0=$(cat $SYS/dropped); c0=$(cpuidle); t0=$(uptime_cs)
	$APP > /tmp/measure.txt 2>&1 & p=$!
	sleep $2
	kill -INT $p; wait $p
	i1=$(irqcnt); d1=$(cat $SYS/dropped); c1=$(cpuidle); t1=$(uptime_cs)
	n=$(awk '/^total/ { print $2 }' /tmp/measure.txt)
	echo "$hz $n $i0 $i1 $d0 $d1 $t0 $t1 $c0 $c1" | awk '{
		s = ($8 - $7) / 100; di = $4 - $3; dd = $6 - $5
		printf "%4d Hz %5.1f s: %7.1f rec/s %6.1f drop/s %7.1f irq/s  residual %d  idle %.0f%%\n",
			$1, s, $2 / s, dd / s, di / s, di - $2 - dd, 100 * ($11 - $9) / ($12 - $10) }'
}

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
irq)
	t=$(pidof irq/50-mpu6050)
	# /proc/<pid>/stat: trường 40 rt_priority, 41 policy (1 = SCHED_FIFO), man 5 proc
	echo "thread $t: $(cat /proc/$t/comm), policy $(awk '{ print $41 }' /proc/$t/stat), rt_priority $(awk '{ print $40 }' /proc/$t/stat)"
	grep -E '^ *(26|50):' /proc/interrupts
	measure 100 10
	grep -E '^ *(26|50):' /proc/interrupts
	;;
dup)
	n=${2:-3000}
	dd if=/dev/mpu6050 bs=16 count=$n 2> /dev/null | hexdump -v -e '16/1 "%02x" "\n"' |
		awk '{ d = substr($0, 1, 16) } NR > 1 && d == prev { dup++ } { prev = d }
			END { printf "%d records, %d consecutive duplicates\n", NR, dup }'
	;;
sweep)
	for r in 100 200 333 500 1000; do measure $r 10; done
	echo 100 > $SYS/sample_rate
	;;
*)
	echo "usage: $0 shake|cfg|strace|restart|gap|irq|dup [n]|sweep" >&2
	exit 2
	;;
esac
