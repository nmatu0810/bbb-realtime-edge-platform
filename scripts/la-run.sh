#!/bin/sh
# Ghi bản ghi trên bo và chụp LA cùng lúc, rồi phân tích: scripts/la-run.sh <hz> <số bản ghi> <tên> [mẫu/s LA] [số mẫu LA]
# Ra <scratch>/<tên>.sr, <tên>.bin; in mpu-ts.py, la-fifo.py và số đếm trên bo.
cd "$(dirname "$0")/.." || exit 1
hz=$1; n=$2; out=${OUT:-/tmp}/$3; sr=${4:-2m}; ns=${5:-10m}
scripts/bo.sh "dmesg -c > /dev/null; nohup sh /tmp/mpu-check.sh rec $hz $n > /dev/null 2>&1 &"
sleep 2
timeout 120 sigrok-cli -d fx2lafw --config samplerate=$sr --samples $ns -o "$out.sr" || exit 1
while scripts/bo.sh 'pidof dd > /dev/null'; do sleep 1; done
scripts/bo.sh 'cat /tmp/rec.txt; dmesg -c | grep mpu6050 | sort | uniq -c; grep debug_locks /proc/lockdep_stats'
scripts/bo.sh 'cat /tmp/rec.bin' > "$out.bin"
python3 scripts/mpu-ts.py "$out.bin" $((1000 / hz)) | head -n 8
case $sr in *m) r=$(( ${sr%m} * 1000000 ));; *k) r=$(( ${sr%k} * 1000 ));; *) r=$sr;; esac
python3 scripts/la-fifo.py "$out.sr" "$out.bin" 2 $r
