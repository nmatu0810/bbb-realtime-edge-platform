#!/bin/sh
# Gắn lại PHY Ethernet nếu kernel không gắn được lúc boot.
#
# Vì sao: LAN8710 trên BBB đôi khi không ACK MDIO ở lần probe đầu
# ("SMSC LAN8710/LAN8720 ... probe ... failed with error -5"), rồi lúc mở eth0
# cpsw báo "phy ... not found on slave 0" và eth0 không có link. Thử lại
# bind + mở lại giao diện thì có lần gắn được (xem docs/boot-flow.md).
# Nguyên nhân gốc chưa rõ; script chỉ thử lại có giới hạn, không sửa nguyên nhân.
#
# Chạy ở foreground, thoát 0 khi eth0 có IPv4, 1 khi bỏ cuộc. Do projectb-phy-retry.service
# khởi động; stdout/stderr vào journal (journalctl -u projectb-phy-retry).
#
# Giá trị riêng của BBB (chính sách), tách khỏi logic phía dưới (cơ chế).
IFACE=eth0
PHY_DEV="4a101000.mdio:00"
PHY_DRV="/sys/bus/mdio_bus/drivers/SMSC LAN8710!LAN8720"
TRIES=10        # số lần thử tối đa
GAP=5           # giây chờ sau mỗi lần thử
IP_WAIT=40      # giây chờ DHCP sau khi PHY gắn được

NAME=projectb-phy-retry

log() {
    echo "$NAME: $*"
}

attached() {
    [ -e "/sys/class/net/$IFACE/phydev" ]
}

has_ip() {
    ip -4 addr show "$IFACE" 2>/dev/null | grep -q 'inet '
}

n=1
while [ "$n" -le "$TRIES" ]; do
    if attached; then
        log "lần $n: PHY đã gắn"
        break
    fi
    log "lần $n/$TRIES: PHY chưa gắn, bind lại rồi mở lại $IFACE"
    # Sau probe lỗi PHY không còn driver; bind lại rồi để cpsw gắn khi mở giao diện.
    if [ ! -e "/sys/bus/mdio_bus/devices/$PHY_DEV/driver" ]; then
        echo "$PHY_DEV" > "$PHY_DRV/bind" 2>/dev/null || log "bind lỗi (sẽ thử lại)"
    fi
    ip link set "$IFACE" down
    ip link set "$IFACE" up
    sleep "$GAP"
    n=$((n + 1))
done

if ! attached; then
    log "bỏ cuộc sau $TRIES lần, PHY vẫn chưa gắn"
    exit 1
fi

i=0
while [ "$i" -lt "$IP_WAIT" ]; do
    if has_ip; then
        log "$IFACE có IPv4: $(ip -4 addr show "$IFACE" | grep 'inet ' | tr -s ' ' | cut -d' ' -f3)"
        exit 0
    fi
    sleep 1
    i=$((i + 1))
done
log "PHY gắn được nhưng sau ${IP_WAIT}s $IFACE vẫn chưa có IPv4"
exit 1
