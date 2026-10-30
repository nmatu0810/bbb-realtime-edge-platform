#!/bin/sh
# Chạy lệnh trên bo qua ssh bằng khóa riêng: scripts/bo.sh 'lệnh'; chép file: scripts/bo.sh 'cat > /tmp/x' < x
# (zsh không tách "ssh -i ..." trong một biến thành lệnh + đối số, nên bọc bằng script.)
exec ssh -i "$HOME/.ssh/id_ed25519_bbb" -o IdentitiesOnly=yes -o ConnectTimeout=5 root@192.168.50.182 "$@"
