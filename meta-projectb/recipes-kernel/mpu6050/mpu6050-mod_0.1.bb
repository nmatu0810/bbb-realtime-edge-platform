SUMMARY = "Out-of-tree kernel driver for the MPU6050/MPU-6500 on I2C2 (project B)"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://COPYING;md5=12f884d2ae1ff87c09e5b7ccc2c4ca7e"

inherit module

# Mã nguồn driver nằm ngoài layer, ở driver/mpu6050/ của repo (spec quy tắc 1: driver
# ở thư mục riêng). Layer vì thế không tách khỏi repo được; hai thứ luôn đi cùng nhau.
# file:// một thư mục: bitbake tính checksum từng file bên trong, sửa .c là build lại.
FILESEXTRAPATHS:prepend := "${THISDIR}/../../../driver:"
SRC_URI = "file://mpu6050"

S = "${UNPACKDIR}/mpu6050"

# module.bbclass tự đặt tên gói kernel-module-mpu6050-<phiên bản kernel>.
RPROVIDES:${PN} += "kernel-module-mpu6050"
