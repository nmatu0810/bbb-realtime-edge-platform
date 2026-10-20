SUMMARY = "Retry attaching the Ethernet PHY at boot (BBB LAN8710)"
DESCRIPTION = "Vá tạm cho lỗi PHY không ACK MDIO lúc probe đầu (error -5). Xem docs/boot-flow.md."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

COMPATIBLE_MACHINE = "beaglebone-yocto"

SRC_URI = " \
    file://projectb-phy-retry.sh \
    file://projectb-phy-retry.service \
"

S = "${WORKDIR}/sources"
UNPACKDIR = "${S}"

inherit systemd

SYSTEMD_SERVICE:${PN} = "projectb-phy-retry.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install () {
    install -d ${D}${sbindir}
    install -m 0755 ${S}/projectb-phy-retry.sh ${D}${sbindir}/projectb-phy-retry

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${S}/projectb-phy-retry.service ${D}${systemd_system_unitdir}/projectb-phy-retry.service
}
