#!/bin/sh
# Đường lỗi của driver (tuần 8). Chép lên rồi sh /tmp/mpu-fault.sh <phần>
#   watch <s> [hz]  app chạy <s> giây (log /tmp/fault-app.txt); mỗi giây một dòng: giây,
#                   ngắt/s, dropped tổng, reset FIFO, watchdog khởi tạo lại, số dòng dmesg
#                   mới. Tus rút/cắm dây trong lúc này. Cuối: dmesg mới, dòng cuối app.
#                   Một vòng dài hơn 1 s (dmesg trên kernel debug chậm): cột ngắt là
#                   mỗi vòng. REC=1: đọc bằng dd vào /tmp/fault.bin thay app, để kiểm
#                   từng bản ghi trên laptop (scripts/mpu-ts.py)
#   unbind <n>      n vòng: app đọc nền, unbind, app phải thoát (ENODEV), bind, đọc 50
#                   bản ghi. Event kmem:kmalloc/kfree: mỗi lần probe cấp st (8840 byte)
#                   phải có một kfree cùng ptr; in nơi gọi kfree (kfree là tail call
#                   nên call_site là hàm gọi mpu6050_put/release), meminfo
#   held            mở fd không đọc, đợi fifo có dữ liệu, unbind, rồi đọc bằng fd đó:
#                   phải ra nốt bản ghi còn lại, sau đó lỗi; bind lại
SYS=/sys/class/misc/mpu6050
APP=/tmp/mpu6050-test
DBG=/sys/kernel/debug/mpu6050
DRV=/sys/bus/i2c/drivers/mpu6050
DEV=2-0068
TR=/sys/kernel/tracing

irqcnt() { awk '$NF == "mpu6050" { print $2 }' /proc/interrupts; }
dmesgn() { dmesg | wc -l; }
kmemon() {
	echo 0 > $TR/tracing_on; echo 8192 > $TR/buffer_size_kb; echo > $TR/trace
	echo 1 > $TR/events/kmem/kmalloc/enable; echo 1 > $TR/events/kmem/kfree/enable
	echo 1 > $TR/tracing_on
}
# st cấp ở probe (call_site mpu6050_probe, > 8 KB), ghép kfree theo ptr
kmemst() {
	echo 0 > $TR/tracing_on
	echo 0 > $TR/events/kmem/kmalloc/enable; echo 0 > $TR/events/kmem/kfree/enable
	awk '/kmalloc: call_site=mpu6050_probe/ && /bytes_req=8840/ {
			match($0, /ptr=[0-9a-f]+/); live[substr($0, RSTART, RLENGTH)] = 1; a++ }
		/kfree:/ { match($0, /ptr=[0-9a-f]+/); k = substr($0, RSTART, RLENGTH)
			if (k in live) { delete live[k]; f++; match($0, /call_site=[^+]+/)
				site[substr($0, RSTART + 10, RLENGTH - 10)]++ } }
		END { n = 0; for (k in live) n++
			printf "st: cấp %d, giải phóng %d, còn sống %d;", a, f, n
			for (s in site) printf " %s %d", s, site[s]; print "" }' $TR/trace
}

case "$1" in
watch)
	[ -n "$3" ] && echo $3 > $SYS/sample_rate
	m0=$(dmesgn)
	if [ -n "$REC" ]; then
		dd if=/dev/mpu6050 of=/tmp/fault.bin bs=16 2> /dev/null & p=$!
	else
		$APP > /tmp/fault-app.txt 2>&1 & p=$!
	fi
	i=0; prev=$(irqcnt)
	echo " s    irq  dropped  fifo_reset  recover  dmesg+"
	while [ $i -lt $2 ]; do
		sleep 1; i=$((i + 1))
		c=$(irqcnt)
		printf "%2d %6d %8s %11s %8s %7d\n" $i $((${c:-0} - ${prev:-0})) \
			$(cat $SYS/dropped) $(cat $DBG/fifo_reset) $(cat $DBG/recover) \
			$(($(dmesgn) - m0))
		prev=$c
	done
	# lệnh nền của sh không tương tác bỏ qua SIGINT: dd phải TERM; app tự bắt INT
	if [ -n "$REC" ]; then kill -TERM $p; else kill -INT $p; fi; wait $p
	echo "--- dmesg"
	dmesg | tail -n +$((m0 + 1)) | cut -c1-120 | head -n 40
	[ -n "$REC" ] && { echo "--- $(($(wc -c < /tmp/fault.bin) / 16)) bản ghi"; exit; }
	echo "--- app"
	grep -c 'without data' /tmp/fault-app.txt | sed 's/^/poll timeouts: /'
	tail -n 3 /tmp/fault-app.txt
	;;
unbind)
	kmemon
	grep -E 'MemFree|^Slab' /proc/meminfo | tr -s ' ' | tr '\n' ' '; echo
	k=0; bad=0
	while [ $k -lt $2 ]; do
		k=$((k + 1))
		$APP > /tmp/unbind-app.txt 2>&1 & p=$!
		sleep 1
		echo $DEV > $DRV/unbind
		sleep 0.5
		if kill -0 $p 2> /dev/null; then
			echo "vòng $k: app còn chạy sau unbind"; bad=$((bad + 1)); kill -INT $p
		fi
		wait $p; rc=$?
		echo $DEV > $DRV/bind
		n=$(dd if=/dev/mpu6050 bs=16 count=50 2> /dev/null | wc -c)
		[ $rc -eq 1 ] && [ $n -eq 800 ] || {
			echo "vòng $k: app rc $rc, sau bind đọc $n byte"; bad=$((bad + 1)); }
	done
	grep -E 'MemFree|^Slab' /proc/meminfo | tr -s ' ' | tr '\n' ' '; echo
	echo "$k vòng, lỗi $bad"
	kmemst
	tail -n 2 /tmp/unbind-app.txt
	;;
held)
	kmemon
	exec 3< /dev/mpu6050
	sleep 1
	echo $DEV > $DRV/unbind
	echo "unbind xong, đọc fd cũ:"
	dd bs=16 <&3 2>&1 > /tmp/held.bin | tail -n 2
	echo "$(($(wc -c < /tmp/held.bin) / 16)) bản ghi còn lại đọc được"
	exec 3<&-
	echo $DEV > $DRV/bind
	kmemst
	ls -l /dev/mpu6050
	;;
*)
	sed -n '2,9p' "$0"
	exit 1
	;;
esac
