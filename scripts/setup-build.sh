#!/usr/bin/env bash
# Tạo một thư mục build Yocto từ cấu hình nằm trong repo.
# Không copy từ build dir khác, nên kết quả chỉ phụ thuộc vào repo + poky.
#
# Chạy trong container yocto-build, với /workspace là thư mục repo:
#   cd /workspace/poky
#   /workspace/scripts/setup-build.sh <tên-build>
#   source oe-init-build-env <tên-build>
#   bitbake core-image-minimal
set -eo pipefail

BUILD="${1:?Usage: $0 <build-dir-name>}"
REPO=/workspace
TPL="$REPO/scripts/templates"

if [ ! -d /workspace/poky/meta ]; then
  echo "Không thấy /workspace/poky. Clone trước:" >&2
  echo "  git clone --branch yocto-5.2.4 --depth 1 https://git.yoctoproject.org/poky" >&2
  exit 1
fi

mkdir -p "/workspace/poky/$BUILD/conf"
cd "/workspace/poky/$BUILD"

for f in local.conf bblayers.conf; do
  if [ -e "conf/$f" ]; then
    cp "conf/$f" "conf/$f.bak"
    echo "Đã sao lưu conf/$f -> conf/$f.bak"
  fi
  sed "s#@WORKSPACE@#$REPO#g" "$TPL/$f.in" > "conf/$f"
done

echo "Xong: /workspace/poky/$BUILD/conf/ (từ $TPL)"
echo "Tiếp: cd /workspace/poky && source oe-init-build-env $BUILD && bitbake core-image-minimal"
