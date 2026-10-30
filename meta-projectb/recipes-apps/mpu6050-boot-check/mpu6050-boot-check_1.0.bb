SUMMARY = "Boot-time self-test of /dev/mpu6050 (project B)"
DESCRIPTION = "udev rule cho /dev/mpu6050 (quyền, kéo unit systemd) và unit chạy \
mpu6050-test -t + một lượt đo khi thiết bị xuất hiện. Kết quả trong journal."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://mpu6050-boot-check.sh \
    file://mpu6050-boot-check.service \
    file://90-mpu6050.rules \
"

S = "${WORKDIR}/sources"
UNPACKDIR = "${S}"

# Không inherit systemd: unit không có [Install], udev kéo nó lên
# (ENV{SYSTEMD_WANTS}), không cần systemctl enable.
do_install () {
    install -D -m 0755 ${S}/mpu6050-boot-check.sh ${D}${bindir}/mpu6050-boot-check
    install -D -m 0644 ${S}/mpu6050-boot-check.service \
        ${D}${systemd_system_unitdir}/mpu6050-boot-check.service
    install -D -m 0644 ${S}/90-mpu6050.rules \
        ${D}${nonarch_base_libdir}/udev/rules.d/90-mpu6050.rules
}

FILES:${PN} += "${systemd_system_unitdir} ${nonarch_base_libdir}/udev/rules.d"

# Driver và app; kernel-module-mpu6050 là RPROVIDES của mpu6050-mod.
RDEPENDS:${PN} = "mpu6050-test kernel-module-mpu6050"
