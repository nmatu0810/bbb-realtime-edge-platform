SUMMARY = "Project B image: core-image-minimal + SSH server + dev tools for BBB/MPU6050"

require recipes-core/images/core-image-minimal.bb

# Ảnh SD dựng bằng wks và extlinux.conf nằm trong layer (meta-projectb/wic/), không dùng
# beaglebone-yocto.wks của meta-yocto-bsp. Kernel cmdline sửa ở projectb-extlinux.conf.
WKS_FILE = "projectb.wks"

# SSH server qua image feature (kéo packagegroup-core-ssh-openssh),
# thay vì ghi tay "openssh" vào IMAGE_INSTALL.
IMAGE_FEATURES += "ssh-server-openssh"

# Quyết định tuần 2: giữ một image duy nhất cho tới khi có sản phẩm cần giao.
# Khi đó tách công cụ dev (gdbserver, stress-ng, rt-tests, devmem) và root login
# không mật khẩu sang projectb-image-dev, image gốc chỉ giữ phần thiết bị cần chạy.
#
# Gói phát triển. rt-tests và stress-ng dùng cho giai đoạn 2, thêm sớm cho đỡ build lại.
# devmem2 chưa có: không nằm trong poky/meta (cần meta-openembedded hoặc recipe riêng).
IMAGE_INSTALL:append = " \
    i2c-tools \
    gdbserver \
    stress-ng \
    rt-tests \
    kernel-module-g-ncm \
    kernel-module-usb-f-ncm \
    projectb-ssh-keys \
    projectb-phy-retry \
"

# kernel-module-g-ncm: đường SSH dự phòng qua USB gadget (usb0), vì PHY Ethernet
# thường fail probe -EIO ở boot lạnh (xem ghi chú điều tra PHY). Tạm thời, bỏ khi
# Ethernet ổn định.
# kernel-module-usb-f-ncm phải khai báo tay: g_ncm gọi usb_get_function_instance("ncm")
# -> request_module("usbfunc:ncm") lúc chạy, depmod không thấy phụ thuộc này nên
# bitbake không tự kéo gói vào.
