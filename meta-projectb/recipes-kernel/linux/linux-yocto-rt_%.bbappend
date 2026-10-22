# linux-yocto-rt trong oe-core chỉ khai COMPATIBLE_MACHINE cho qemu. Cùng 6.12.47,
# nhánh v6.12/standard/preempt-rt/base. yocto-kernel-cache có sẵn
# bsp/beaglebone/beaglebone-preempt-rt.scc; KMACHINE chọn nó (meta-yocto-bsp chỉ
# đặt KMACHINE cho linux-yocto). Bật bằng PREFERRED_PROVIDER_virtual/kernel trong local.conf.
COMPATIBLE_MACHINE:beaglebone-yocto = "beaglebone-yocto"
KMACHINE:beaglebone-yocto ?= "beaglebone"

require projectb-kernel.inc
