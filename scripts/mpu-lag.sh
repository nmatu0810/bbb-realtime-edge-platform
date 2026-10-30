#!/bin/sh
# Độ trễ cạnh INT → ts → lần đọc FIFO_COUNT (tuần 8): sh /tmp/mpu-lag.sh [hz] [số bản ghi]
# ftrace (clock mono): irq 26 (hardirq bank gpio1, coi như cạnh), irq 50 (nửa trên của
# driver: chỗ lấy ts), i2c_write (bắt đầu giao dịch, chỉ lần ghi byte thanh ghi 0x72) và
# i2c_result (ret < 0 là NACK). In phân bố cạnh → ts và cạnh → bắt đầu đọc count, tách
# lần đọc bị NACK. Dùng để chọn MPU6050_TS_LAG_MIN_NS cho từng kernel.
T=/sys/kernel/tracing
SYS=/sys/class/misc/mpu6050
hz=${1:-500}; n=${2:-5000}
echo $hz > $SYS/sample_rate
echo 0 > $T/tracing_on; echo > $T/trace; echo mono > $T/trace_clock; echo 8192 > $T/buffer_size_kb
echo 'irq == 26 || irq == 50' > $T/events/irq/irq_handler_entry/filter
echo 'adapter_nr == 2' > $T/events/i2c/i2c_write/filter
echo 'adapter_nr == 2' > $T/events/i2c/i2c_result/filter
for e in irq/irq_handler_entry i2c/i2c_write i2c/i2c_result; do echo 1 > $T/events/$e/enable; done
echo 1 > $T/tracing_on
dd if=/dev/mpu6050 bs=16 count=$n of=/dev/null 2>&1 | tail -n 1
echo 0 > $T/tracing_on
for e in irq/irq_handler_entry i2c/i2c_write i2c/i2c_result; do
	echo 0 > $T/events/$e/enable; echo 0 > $T/events/$e/filter
done
cat $T/trace > /tmp/lag.txt; echo local > $T/trace_clock
echo 100 > $SYS/sample_rate
echo "$(uname -r), $hz Hz: $(grep -c 'irq=26' /tmp/lag.txt) irq26, lost $(grep -c LOST /tmp/lag.txt)"
# Trích số (µs) theo loại rồi sort -n: sắp trong awk của busybox chậm (O(n²)).
awk '{ for (i = 1; i < NF; i++) if ($i ~ /^[0-9]+\.[0-9]+:$/) { t = substr($i, 1, length($i) - 1) * 1e6; break } }
	/irq=26/ { e = t; next }
	/irq=50/ { if (e) printf "L %.0f\n", t - e; next }
	# lần đầu của một giao dịch đọc count là ghi 1 byte 0x72
	/i2c_write:/ && /\[72\]/ { s = t - e; pend = 1; next }
	/i2c_result:/ && pend { pend = 0; printf "%s %.0f\n", $0 ~ /ret=-/ ? "N" : "A", s }' /tmp/lag.txt > /tmp/lag.num
for k in "L cạnh -> ts (irq26 -> 50)" "A cạnh -> đọc count, ACK" "N cạnh -> đọc count, NACK"; do
	awk -v k=${k%% *} '$1 == k { print $2 }' /tmp/lag.num | sort -n > /tmp/lag.one
	awk -v name="${k#* }" '{ a[NR - 1] = $1 } END { n = NR
		if (n) printf "%-26s n %5d  min %4d  p1 %4d  p50 %4d  p99 %5d  max %6d us\n", name, n,
			a[0], a[int(n / 100)], a[int(n / 2)], a[int(n * 99 / 100)], a[n - 1]
		else printf "%-26s n 0\n", name }' /tmp/lag.one
done
