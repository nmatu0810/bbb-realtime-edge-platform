#!/bin/sh
# Chép mpu6050-hmi vừa build lên /tmp của bo, so hash hai đầu, chạy nền.
# /tmp mất khi tắt bo: mỗi lần boot chạy lại script này. Bật màn trước khi bật bo.
# Tắt app trước khi chạy demo.sh/mpu6050-test: scripts/bo.sh 'kill $(pidof mpu6050-hmi)'
# Lấy từ sstate (không có package/): HMI=<đường dẫn binary> scripts/hmi-push.sh
cd "$(dirname "$0")/.." || exit 1
HMI=${HMI:-$(ls poky/build-clean/tmp/work/cortexa8hf-neon-poky-linux-gnueabi/mpu6050-hmi/0.1/package/usr/bin/mpu6050-hmi)} || exit 1
a=$(sha256sum "$HMI" | cut -c1-16)
b=$(scripts/bo.sh 'cat > /tmp/mpu6050-hmi && chmod +x /tmp/mpu6050-hmi && sha256sum /tmp/mpu6050-hmi' < "$HMI" | cut -c1-16)
echo "hmi $a (laptop) $b (bo), $(stat -c %s "$HMI") bytes"
[ "$a" = "$b" ] || { echo "hash mismatch" >&2; exit 1; }
scripts/bo.sh 'kill $(pidof mpu6050-hmi) 2>/dev/null; sleep 1; setsid /tmp/mpu6050-hmi >/tmp/hmi.log 2>&1 </dev/null & sleep 2; echo "pid=$(pidof mpu6050-hmi)"; cat /tmp/hmi.log'
