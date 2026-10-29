SUMMARY = "Userspace API header of the mpu6050 driver (project B)"
LICENSE = "GPL-2.0-with-Linux-syscall-note"
LIC_FILES_CHKSUM = "file://projectb/mpu6050.h;beginline=1;endline=1;md5=3984933679b5478fa7c1231c3b82e066"

# Chỉ có header, không phụ thuộc máy hay kernel. Tách khỏi mpu6050-mod (MACHINE_ARCH,
# chữ ký theo kernel) để app và sensord không phải build lại mỗi lần đổi kernel
# (RT/non-RT, bật/tắt -debug). Nguồn duy nhất là header trong driver/mpu6050/.
inherit allarch

FILESEXTRAPATHS:prepend := "${THISDIR}/../../../driver:"
SRC_URI = "file://mpu6050/include/uapi/projectb/mpu6050.h"

S = "${UNPACKDIR}/mpu6050/include/uapi"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
	install -D -m 0644 ${S}/projectb/mpu6050.h ${D}${includedir}/projectb/mpu6050.h
}

# Header nằm trong ${PN}-dev; gói chính rỗng nhưng phải tồn tại vì ${PN}-dev phụ
# thuộc nó.
ALLOW_EMPTY:${PN} = "1"
