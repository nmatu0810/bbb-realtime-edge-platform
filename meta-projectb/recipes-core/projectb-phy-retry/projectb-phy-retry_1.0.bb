SUMMARY = "Retry attaching the Ethernet PHY at boot (BBB LAN8710)"
DESCRIPTION = "Vá tạm cho lỗi PHY không ACK MDIO lúc probe đầu (error -5). Xem docs/boot-flow.md."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

COMPATIBLE_MACHINE = "beaglebone-yocto"

SRC_URI = "file://projectb-phy-retry.init"

S = "${WORKDIR}/sources"
UNPACKDIR = "${S}"

inherit update-rc.d

# Chạy sau "networking" (S01), vì ifup eth0 phải thử mở giao diện trước.
INITSCRIPT_NAME = "projectb-phy-retry"
INITSCRIPT_PARAMS = "start 03 2 3 4 5 . stop 80 0 6 1 ."

do_install () {
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${S}/projectb-phy-retry.init ${D}${sysconfdir}/init.d/projectb-phy-retry
}
