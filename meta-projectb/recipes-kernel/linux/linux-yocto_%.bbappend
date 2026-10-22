# Config fragment của project. Để trong files/ (không phải ${PN}/) để
# linux-yocto-rt dùng lại đúng các fragment này: RT và non-RT chỉ khác preemption.
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# hid-multitouch: cảm ứng màn 7" (WCH 1a86:e5e3). Không có nó, thiết bị rơi về
# hid-generic: không INPUT_PROP_DIRECT, không ABS_MT_*, libinput không coi là
# touchscreen. Không chữa được việc USB tự ngắt khi chạm (PROGRESS tuần 3).
# Để =m để A/B trên cùng một lần boot bằng modprobe -r.
SRC_URI += "file://hid-multitouch.cfg"
