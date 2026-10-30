#!/bin/sh
# Sau khi bo boot lại (/tmp trống): chép app, strace, script lên /tmp, so hash, rồi nạp ko (ko-push.sh).
cd "$(dirname "$0")/.." || exit 1
W=poky/build-clean/tmp/work
APP=$W/cortexa8hf-neon-poky-linux-gnueabi/mpu6050-test/0.1/image/usr/bin/mpu6050-test
STR=$W/cortexa8hf-neon-poky-linux-gnueabi/strace/6.12/packages-split/strace/usr/bin/strace
scripts/bo.sh 'cat > /tmp/mpu6050-test; chmod +x /tmp/mpu6050-test' < "$APP" || exit 1
scripts/bo.sh 'cat > /tmp/strace; chmod +x /tmp/strace' < "$STR" || exit 1
for f in mpu-check.sh mpu-stress.sh mpu-trace.sh mpu-sched.sh; do
	scripts/bo.sh "cat > /tmp/$f" < scripts/$f || exit 1
done
echo "app/strace laptop: $(sha256sum "$APP" "$STR" | cut -c1-16 | tr '\n' ' ')"
echo "app/strace bo:     $(scripts/bo.sh 'sha256sum /tmp/mpu6050-test /tmp/strace' | cut -c1-16 | tr '\n' ' ')"
exec scripts/ko-push.sh "$@"
