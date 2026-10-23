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

# Kernel debug cho lúc viết driver: lockdep + kiểm ngủ trong ngữ cảnh atomic
# (files/debug-locking.cfg). Bật bằng PROJECTB_KERNEL_DEBUG = "1" trong local.conf.
# Hậu tố -debug vào chuỗi phiên bản để vermagic từ chối module build cho kernel
# không debug (spinlock_t khác kích thước). Dùng :append, không gán =, để giữ
# -yocto-${LINUX_KERNEL_TYPE} mà linux-yocto.inc đặt bằng ??=.
# Chưa dùng CONFIG_MODVERSIONS: bắt được mọi lệch ABI nhưng kéo theo genksyms và
# Module.symvers cho module ngoài cây; hậu tố đủ cho đúng rủi ro này.
PROJECTB_KERNEL_DEBUG ??= "0"
SRC_URI += "${@oe.utils.conditional('PROJECTB_KERNEL_DEBUG', '1', 'file://debug-locking.cfg', '', d)}"
LINUX_VERSION_EXTENSION:append = "${@oe.utils.conditional('PROJECTB_KERNEL_DEBUG', '1', '-debug', '', d)}"
