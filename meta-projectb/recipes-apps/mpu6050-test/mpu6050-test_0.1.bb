SUMMARY = "Test program for /dev/mpu6050: poll() + binary records, ioctl (project B)"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# Header uapi qua sysroot từ recipe riêng (allarch), không phụ thuộc mpu6050-mod:
# đổi kernel không làm app build lại.
DEPENDS = "mpu6050-uapi"

# Mã nguồn ở apps/mpu6050-test/ của repo, như driver ở driver/.
FILESEXTRAPATHS:prepend := "${THISDIR}/../../../apps:"
SRC_URI = "file://mpu6050-test"

S = "${UNPACKDIR}/mpu6050-test"

EXTRA_OEMAKE = "'CC=${CC}' 'CFLAGS=${CFLAGS}' 'LDFLAGS=${LDFLAGS}'"

do_compile() {
	oe_runmake
}

do_install() {
	oe_runmake install DESTDIR=${D} bindir=${bindir}
}
