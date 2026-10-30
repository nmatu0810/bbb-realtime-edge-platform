#!/bin/sh
# Ai giữ CPU khi cạnh INT bị trễ (tuần 7): sh /tmp/mpu-sched.sh [hz] [số bản ghi]
# ftrace irq 26/50 + sched_switch trong lúc dd đọc; với mỗi khoảng giữa hai irq 50 > 1.5 chu
# kỳ, in các lần đổi tác vụ trong khoảng đó (ai chạy, ưu tiên kernel: 49 = SCHED_FIFO 50).
T=/sys/kernel/tracing
SYS=/sys/class/misc/mpu6050
hz=${1:-1000}; n=${2:-3000}
echo $hz > $SYS/sample_rate
echo 0 > $T/tracing_on; echo > $T/trace; echo mono > $T/trace_clock; echo 8192 > $T/buffer_size_kb
echo 'irq == 26 || irq == 50' > $T/events/irq/irq_handler_entry/filter
echo 1 > $T/events/irq/irq_handler_entry/enable
echo 1 > $T/events/sched/sched_switch/enable
echo 1 > $T/tracing_on
dd if=/dev/mpu6050 bs=16 count=$n of=/dev/null 2> /dev/null
echo 0 > $T/tracing_on
echo 0 > $T/events/irq/irq_handler_entry/enable; echo 0 > $T/events/sched/sched_switch/enable
echo 0 > $T/events/irq/irq_handler_entry/filter
cat $T/trace > /tmp/sched.txt; echo local > $T/trace_clock
echo 100 > $SYS/sample_rate
echo "$(grep -c irq_handler_entry /tmp/sched.txt) irq, $(grep -c sched_switch /tmp/sched.txt) switches, lost $(grep -c LOST /tmp/sched.txt)"
awk -v p=$((1000000 / hz)) '{ for (i = 1; i < NF; i++) if ($i ~ /^[0-9]+\.[0-9]+:$/) { t = substr($i, 1, length($i) - 1) * 1e6; break } }
	/irq_handler_entry: irq=50/ { if (last && t - last > 1.5 * p) { printf "gap %.0f us after %.6f\n", t - last, last / 1e6; for (k = 1; k <= ns; k++) print "   ", sw[k] }
		last = t; ns = 0 }
	/sched_switch/ { if (ns < 12) { s = $0; sub(/.*sched_switch: /, "", s); sw[++ns] = sprintf("+%.0f %s", t - last, s) } }' /tmp/sched.txt | head -n 60
