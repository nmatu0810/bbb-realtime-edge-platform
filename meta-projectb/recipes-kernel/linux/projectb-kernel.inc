# Phần dùng chung cho linux-yocto và linux-yocto-rt: cấu hình của project phải giống
# nhau ở cả hai kernel. Lưu ý ktype standard và preempt-rt của yocto-kernel-cache khác
# nhau ~140 dòng .config ngoài preemption (-Os/-O2, KFENCE, KGDB, BPF_JIT...), xem
# PROGRESS tuần 3; muốn so benchmark công bằng phải san bằng bằng fragment.
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# hid-multitouch: cảm ứng màn 7" (WCH 1a86:e5e3). Không có nó, thiết bị rơi về
# hid-generic: không INPUT_PROP_DIRECT, không ABS_MT_*, libinput không coi là
# touchscreen. Không chữa được việc USB tự ngắt khi chạm (PROGRESS tuần 3).
# Để =m để A/B trên cùng một lần boot bằng modprobe -r.
SRC_URI += "file://hid-multitouch.cfg"

# Build dtb gốc với -@ để có node __symbols__: U-Boot (fdtoverlays trong extlinux)
# cần nó để nối nhãn &i2c2, &gpio1, &am33xx_pinmux trong overlay vào dtb gốc.
# Thiếu thì U-Boot in "Failed to apply overlay" rồi vẫn boot, overlay bị bỏ qua.
KERNEL_DTC_FLAGS = "-@"
