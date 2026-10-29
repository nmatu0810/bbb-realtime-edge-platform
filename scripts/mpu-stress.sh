#!/bin/sh
# Chạy tải và dọn dẹp cho driver (tuần 5, dùng lại để so ở tuần 6–7): sh /tmp/mpu-stress.sh <phần>
#   oc        20 lần selftest, 20 lần hai app mở cùng lúc (đúng một bên EBUSY)
#   sysfs     app chạy nền, 30 s ghi sample_rate/accel_range ngẫu nhiên mỗi 0.2 s
#   load [s]  app + stress-ng (cpu, io, vm) trong s giây, mặc định 600
#   mod       rmmod khi đang mở phải lỗi; 20 vòng insmod/app/rmmod, so slab trước/sau
# Phần nào cũng kết thúc bằng debug_locks và số dòng dmesg mới có WARNING/BUG/error.
SYS=/sys/class/misc/mpu6050
APP=/tmp/mpu6050-test
KO=/tmp/mpu6050.ko

tail_check() {
	grep debug_locks /proc/lockdep_stats
	echo "dmesg WARNING/BUG/error/failed/lost: $(dmesg -c | grep -ciE 'warning|bug|error|circular|lockdep|failed|lost')"
}

# Tóm tắt output của app: tốc độ min/max, max gap lớn nhất, số giây gap > 20 ms.
summarize() {
	awk '/\/s \[/ { r = $1 + 0; n++
		for (i = 1; i <= NF; i++) { if ($i == "gap") g = $(i + 1) + 0
			if ($i == "dropped") d = $(i + 1) }
		if (n == 1 || r < rmin) rmin = r; if (r > rmax) rmax = r
		if (g > gmax) gmax = g; if (g > 20) slow++ }
	/^total/ { tot = $0 }
	END { printf "%d lines, rate %d..%d/s, worst gap %.1f ms, %d s with gap > 20 ms\n%s\n",
		n, rmin, rmax, gmax, slow, tot }' "$1"
}

case "$1" in
oc)
	f=0
	for i in $(seq 20); do
		$APP -t > /dev/null
		f=$((f + $?))
	done
	echo "selftest x20: $f failed checks"
	ok=0; busy=0; other=0
	for i in $(seq 20); do
		$APP -n 20 > /dev/null 2> /tmp/oc1.txt & a=$!
		$APP -n 20 > /dev/null 2> /tmp/oc2.txt & b=$!
		wait $a; ra=$?; wait $b; rb=$?
		for r in $ra $rb; do [ $r -eq 0 ] && ok=$((ok + 1)); done
		busy=$((busy + $(cat /tmp/oc1.txt /tmp/oc2.txt | grep -c busy)))
	done
	other=$((40 - ok - busy))
	echo "pairs x20: $ok ran, $busy EBUSY, $other other"
	tail_check
	;;
sysfs)
	$APP > /tmp/stress-sysfs.txt 2>&1 & p=$!
	sleep 1
	rates="4 20 50 100 166 200 333 500 1000"
	n=0; bad=0; seen=
	end=$(($(date +%s) + 30))
	while [ $(date +%s) -lt $end ]; do
		r=$(echo $rates | tr ' ' '\n' | sed -n "$((RANDOM % 9 + 1))p")
		g=$(echo 2 4 8 16 | tr ' ' '\n' | sed -n "$((RANDOM % 4 + 1))p")
		echo $r > $SYS/sample_rate || bad=$((bad + 1))
		echo $g > $SYS/accel_range || bad=$((bad + 1))
		cat $SYS/sample_rate $SYS/accel_range $SYS/dropped > /dev/null || bad=$((bad + 1))
		n=$((n + 1)); seen="$seen $r"
		sleep 0.2
	done
	echo 100 > $SYS/sample_rate; echo 2 > $SYS/accel_range
	sleep 2
	kill -INT $p; wait $p
	echo "$n rounds, $bad failed writes/reads; app exit $?"
	echo "restored: $(cat $SYS/sample_rate) Hz, $(cat $SYS/accel_range) g"
	echo "rates written:" $(echo $seen | tr ' ' '\n' | sort -n | uniq -c | awk '{ print $2 "x" $1 }')
	tail -n 3 /tmp/stress-sysfs.txt
	tail_check
	;;
load)
	s=${2:-600}
	# Tuần 6: thêm số ngắt irq 50 (mpu6050) trong lúc app mở, so với bản ghi + dropped
	i0=$(awk '$NF == "mpu6050" { print $2 }' /proc/interrupts)
	$APP > /tmp/stress-load.txt 2>&1 & p=$!
	sleep 2
	stress-ng --cpu 1 --io 1 --vm 1 --vm-bytes 64M --timeout ${s}s --metrics-brief 2>&1 | tail -n 6
	sleep 2
	kill -INT $p; wait $p
	i1=$(awk '$NF == "mpu6050" { print $2 }' /proc/interrupts)
	summarize /tmp/stress-load.txt
	echo "irq 50: +$((i1 - i0))"
	tail_check
	;;
mod)
	insmod $KO
	$APP -n 300 > /dev/null & p=$!
	sleep 1
	rmmod mpu6050 2>&1 | head -n 1
	wait $p
	rmmod mpu6050
	grep -E '^kmalloc-(4k|8k) ' /proc/slabinfo | awk '{ print "before", $1, $2 }'
	f=0
	for i in $(seq 20); do
		insmod $KO || f=$((f + 1))
		$APP -n 10 > /dev/null || f=$((f + 1))
		rmmod mpu6050 || f=$((f + 1))
	done
	[ -e /dev/mpu6050 ] && f=$((f + 1))
	grep -E '^kmalloc-(4k|8k) ' /proc/slabinfo | awk '{ print "after ", $1, $2 }'
	echo "20 loops: $f failures"
	tail_check
	;;
*)
	echo "usage: $0 oc|sysfs|load [s]|mod" >&2
	exit 2
	;;
esac
