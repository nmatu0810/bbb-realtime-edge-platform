#!/bin/sh
# Chạy trên bo: đo CPU của mpu6050-hmi và dropped trong DUR giây.
# Không có app đang chạy thì tự chạy; đo xong để app chạy tiếp.
#   sh /tmp/hmi-measure.sh [giây] [đường dẫn app]
DUR=${1:-60}
APP=${2:-/tmp/mpu6050-hmi}
LOG=/tmp/hmi.log

pid=$(pidof mpu6050-hmi)
if [ -z "$pid" ]; then
	setsid "$APP" >"$LOG" 2>&1 </dev/null &
	sleep 3
	pid=$(pidof mpu6050-hmi)
fi
[ -n "$pid" ] || { echo "app không chạy"; cat "$LOG"; exit 1; }

ticks() { awk '{print $14 + $15}' /proc/$pid/stat; }
total() { awk '/^cpu /{print $2+$3+$4+$5+$6+$7+$8+$9}' /proc/stat; }
irqs() { awk '/mpu6050/{print $2}' /proc/interrupts; }

t0=$(ticks); c0=$(total); d0=$(cat /sys/class/misc/mpu6050/dropped); i0=$(irqs)
sleep "$DUR"
t1=$(ticks); c1=$(total); d1=$(cat /sys/class/misc/mpu6050/dropped); i1=$(irqs)

echo "pid=$pid dur=${DUR}s rate=$(cat /sys/class/misc/mpu6050/sample_rate)"
echo "cpu_app=$(awk "BEGIN{printf \"%.1f\", ($t1-$t0)*100/($c1-$c0)}")%"
echo "irq_delta=$((i1 - i0)) dropped_delta=$((d1 - d0)) dropped=$d1"
echo "rss_kb=$(awk '/VmRSS/{print $2}' /proc/$pid/status)"
grep -i -E 'warn|error' "$LOG" | sed -n 1,5p
