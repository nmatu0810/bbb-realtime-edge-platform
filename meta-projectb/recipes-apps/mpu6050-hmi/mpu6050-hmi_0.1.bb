SUMMARY = "Thin LVGL screen for /dev/mpu6050: X/Y/Z values and a line chart on fbdev (project B)"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302 \
                    file://${UNPACKDIR}/lvgl/LICENCE.txt;md5=bf1198c89ae87f043108cea62460b03a"

DEPENDS = "mpu6050-uapi"

# LVGL link tĩnh với lv_conf.h của app, không dùng recipe lvgl của meta-oe: khỏi thêm
# layer, tự chọn font, và binary chỉ cần libc nên chạy được trên ảnh không có libdrm.
# SRCREV là v9.2.2, cùng commit meta-oe nhánh walnascar ghim.
FILESEXTRAPATHS:prepend := "${THISDIR}/../../../apps:"
SRC_URI = "file://mpu6050-hmi \
           git://github.com/lvgl/lvgl;protocol=https;branch=release/v9.2;destsuffix=lvgl"
SRCREV = "7f07a129e8d77f4984fff8e623fd5be18ff42e74"

S = "${UNPACKDIR}/mpu6050-hmi"

EXTRA_OEMAKE = "'CC=${CC}' 'CFLAGS=${CFLAGS}' 'LDFLAGS=${LDFLAGS}' 'LVGL_DIR=${UNPACKDIR}/lvgl'"

do_compile() {
	oe_runmake
}

do_install() {
	oe_runmake install DESTDIR=${D} bindir=${bindir}
}

# LV_LOG dùng __FILE__ và LVGL nằm ngoài ${S}, Yocto không tự ánh xạ đường dẫn này:
# thiếu dòng dưới thì binary chứa đường dẫn máy build, do_package_qa báo [buildpaths].
DEBUG_PREFIX_MAP:append = " -ffile-prefix-map=${UNPACKDIR}/lvgl=${TARGET_DBGSRC_DIR}/lvgl"
