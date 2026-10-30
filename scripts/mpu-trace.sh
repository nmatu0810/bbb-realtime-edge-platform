#!/bin/sh
# ftrace đường ngắt DATA_RDY (tuần 6): sh /tmp/mpu-trace.sh [số bản ghi, mặc định 500]
# trace_clock mono = CLOCK_MONOTONIC, cùng miền với ts_ns của bản ghi. Mỗi mẫu có ba mốc:
#   irq 26 entry   nửa trên cứng của gpio-omap: INT vừa tới CPU (cạnh INT + vài µs)
#   irq 50 entry   nửa trên của driver, chạy trong thread irq/26: lúc lấy ts_ns
#   i2c_write      thread irq/50-mpu6050 bắt đầu giao dịch đầu trên adapter 2 (tuần 7: đọc FIFO_COUNT)
# In min/trung vị/p99/max (µs) của 26→50, 50→i2c, 26→i2c, và |ts_ns - irq 50 entry|.
T=/sys/kernel/tracing
n=${1:-500}

stats() {
	sort -n | awk '{ a[NR] = $1 } END {
		p = int(NR * 0.99) + 1; if (p > NR) p = NR
		printf "n %d  min %.0f  med %.0f  p99 %.0f  max %.0f us\n", NR, a[1], a[int(NR / 2) + 1], a[p], a[NR] }'
}

echo 0 > $T/tracing_on
echo > $T/trace
echo mono > $T/trace_clock
echo 4096 > $T/buffer_size_kb
echo 'irq == 26 || irq == 50' > $T/events/irq/irq_handler_entry/filter
echo 'adapter_nr == 2' > $T/events/i2c/i2c_write/filter
echo 1 > $T/events/irq/irq_handler_entry/enable
echo 1 > $T/events/i2c/i2c_write/enable
echo 1 > $T/tracing_on
# Bản ghi: ax ay az temp (s16) rồi ts_ns thấp, cao (u32, little-endian)
dd if=/dev/mpu6050 bs=16 count=$n 2> /dev/null |
	hexdump -v -e '4/2 "%d " 2/4 " %u" "\n"' > /tmp/trace-rec.txt
echo 0 > $T/tracing_on
echo 0 > $T/events/irq/irq_handler_entry/enable
echo 0 > $T/events/i2c/i2c_write/enable
echo 0 > $T/events/irq/irq_handler_entry/filter
echo 0 > $T/events/i2c/i2c_write/filter
cat $T/trace > /tmp/trace.txt
echo local > $T/trace_clock
echo "$(wc -l < /tmp/trace-rec.txt) records, $(grep -c ': irq_handler_entry\|: i2c_write' /tmp/trace.txt) events, lost: $(grep -c LOST /tmp/trace.txt)"

# Mốc thời gian (µs) của từng sự kiện: "<t> 26|50|w"
awk '{ for (i = 1; i < NF; i++) if ($i ~ /^[0-9]+\.[0-9]+:$/) {
		t = substr($i, 1, length($i) - 1); e = $(i + 1)
		if (e == "irq_handler_entry:") { split($(i + 2), a, "="); print t * 1e6, a[2] }
		else if (e == "i2c_write:") print t * 1e6, "w"
		break } }' /tmp/trace.txt > /tmp/trace-ev.txt

for k in 1 2 3; do
	awk -v k=$k '$2 == 26 { t26 = $1; t50 = 0 }
		$2 == 50 && t26 { t50 = $1; if (k == 1) print t50 - t26 }
		$2 == "w" && t50 { if (k == 2) print $1 - t50; if (k == 3) print $1 - t26; t26 = 0; t50 = 0 }' \
		/tmp/trace-ev.txt > /tmp/trace-d$k.txt
done
printf "irq26 -> irq50 (ts_ns): "; stats < /tmp/trace-d1.txt
printf "irq50 -> i2c_write:     "; stats < /tmp/trace-d2.txt
printf "irq26 -> i2c_write:     "; stats < /tmp/trace-d3.txt

# Mỗi bản ghi ghép với irq 50 entry gần nhất (hai dãy đều tăng dần).
awk '$2 == 50 { print $1 }' /tmp/trace-ev.txt > /tmp/trace-t50.txt
awk 'NR == FNR { t[++m] = $1; next }
	{ ts = ($6 * 4294967296 + $5) / 1000
	  while (j < m && t[j + 1] <= ts) j++
	  d = 1e18; k = j; if (j >= 1) d = ts - t[j]; if (j < m && t[j + 1] - ts < d) { d = t[j + 1] - ts; k = j + 1 }
	  if (m) print d, k }' /tmp/trace-t50.txt /tmp/trace-rec.txt > /tmp/trace-d4.txt
printf "|ts_ns - irq50 entry|:  "; cut -d' ' -f1 /tmp/trace-d4.txt | stats
# Tuần 7: mỗi bản ghi phải có cạnh riêng (không hai bản ghi chung một cạnh); bản ghi cách
# mọi cạnh > 50 us là mẫu dư có ts nội suy.
awk '{ if ($2 == prev) dup++; prev = $2; if ($1 > 50) far++ }
	END { printf "records sharing an edge %d, farther than 50 us from any edge %d\n", dup, far }' /tmp/trace-d4.txt
