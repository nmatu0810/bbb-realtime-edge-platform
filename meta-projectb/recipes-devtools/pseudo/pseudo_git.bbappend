FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

# pseudo mặc định thiếu wrapper openat2(); tar >= 1.35 trên glibc >= 2.39
# dùng openat2() nên do_package của linux-libc-headers lỗi
# ("got *at() syscall for unknown directory").
# Ghim revision có wrapper openat2().
SRCREV = "ef54199ef170f8fe5548cd5da2f210680a58d2e2"

# Revision này đã tự xử lý việc mà patch dưới đây làm, nên bỏ patch để
# tránh xung đột.
SRC_URI:remove = "file://0001-configure-Prune-PIE-flags.patch"
