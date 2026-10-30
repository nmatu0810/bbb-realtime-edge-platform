#!/bin/sh
# Chép mpu6050.ko vừa build lên bo, so hash hai đầu, nạp lại: scripts/ko-push.sh [tham số insmod]
# Hậu tố kernel lấy từ uname trên bo (RT debug, RT, non-RT dùng chung script).
# Module lấy từ sstate (setscene) thì không có packages-split: KO=<đường dẫn .ko> scripts/ko-push.sh
cd "$(dirname "$0")/.." || exit 1
R=$(scripts/bo.sh 'uname -r') || exit 1
KO=${KO:-$(ls poky/build-clean/tmp/work/beaglebone_yocto-poky-linux-gnueabi/mpu6050-mod/0.1/packages-split/kernel-module-mpu6050-$R/usr/lib/modules/$R/updates/mpu6050.ko)} || exit 1
a=$(sha256sum "$KO" | cut -c1-16)
b=$(scripts/bo.sh 'cat > /tmp/mpu6050.ko && sha256sum /tmp/mpu6050.ko' < "$KO" | cut -c1-16)
echo "ko $a (laptop) $b (bo), $(stat -c %s "$KO") bytes"
[ "$a" = "$b" ] || { echo "hash mismatch" >&2; exit 1; }
scripts/bo.sh "rmmod mpu6050 2>/dev/null; dmesg -c > /dev/null; insmod /tmp/mpu6050.ko $*; dmesg -c; grep debug_locks /proc/lockdep_stats 2>/dev/null; true"
