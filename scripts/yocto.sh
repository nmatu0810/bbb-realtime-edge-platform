#!/usr/bin/env bash
# Run a command inside the Yocto build container (Ubuntu 24.04) with the build env sourced.
#   ./scripts/yocto.sh bitbake core-image-minimal   # build
#   ./scripts/yocto.sh                              # interactive shell
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${YOCTO_IMAGE:-yocto-build}"
TTY=(); [ -t 0 ] && TTY=(-it)
CMD=("${@:-bash}")
exec docker run --rm "${TTY[@]}" \
  -v "$ROOT":/workspace -w /workspace/poky \
  "$IMAGE" bash -c 'source oe-init-build-env build-clean >/dev/null && exec "$@"' _ "${CMD[@]}"
