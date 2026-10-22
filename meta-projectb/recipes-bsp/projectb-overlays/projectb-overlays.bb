SUMMARY = "Device tree overlays for Project B (BBB + MPU6050)"

# devicetree.bbclass: dịch .dts bằng cpp + dtc với include của kernel (dt-bindings,
# am33xx.h), file có "/plugin/;" thành .dtbo, deploy vào DEPLOY_DIR_IMAGE/devicetree/.
inherit devicetree

COMPATIBLE_MACHINE = "beaglebone-yocto"

SRC_URI = "file://mpu6050.dts"
