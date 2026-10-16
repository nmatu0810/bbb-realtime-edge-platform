SUMMARY = "Public SSH key authorized for root on the Project B board"
DESCRIPTION = "Chỉ chứa khóa công khai (id_ed25519_bbb.pub). Khóa riêng ở lại máy dev."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://authorized_keys"

S = "${WORKDIR}/sources"
UNPACKDIR = "${S}"

inherit allarch

# sshd (StrictModes) từ chối khóa nếu thư mục .ssh hoặc authorized_keys
# cho phép người khác ghi vào, nên quyền phải chặt: 0700 / 0600.
do_install () {
    install -d -m 0700 ${D}${ROOT_HOME}/.ssh
    install -m 0600 ${S}/authorized_keys ${D}${ROOT_HOME}/.ssh/authorized_keys
}

FILES:${PN} = "${ROOT_HOME}/.ssh"
