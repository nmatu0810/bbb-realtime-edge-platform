# Bật applet devmem của busybox (đọc/ghi địa chỉ vật lý qua /dev/mem).
# Dùng để kiểm pinmux ở tuần 3 (control module 0x44E10000); oe-core không có gói devmem2.
FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += "file://devmem.cfg"
