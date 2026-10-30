Bắt đầu tuần 8 của project B: đường lỗi của driver (I2C hỏng, unbind khi stream), module + app + overlay 400 kHz vào ảnh, tài liệu kiến trúc, ảnh LA và log cho báo cáo, diễn tập demo, tag v1.0-chuyennganh. Mốc cuối: thẻ ghi mới, driver tự nạp, test chạy không thao tác tay.

1. Đọc ngữ cảnh theo thứ tự trong CLAUDE.md:
   - docs/PROGRESS.md: "Tuần 8 — bàn giao" trước (hệ thống đang chạy, hai nhãn extlinux sửa tay trên thẻ, vòng sửa–thử, điểm cần quyết, giới hạn mang sang, bẫy), rồi kết quả tuần 7, bàn giao tuần 7. Bàn giao tuần 3–4 cho ghi thẻ (flash-verified.sh, timeout 180), PREFERRED_PROVIDER/ReachableStamps.
   - docs/ROADMAP.md: kế hoạch driver tuần 4–8 (mốc tuần 8, thứ tự cắt), điều chỉnh 2026-10-30 (500 Hz cam kết, san bằng .config dời GĐ2, 400 kHz).
   - Ghi chú kiến thức tuần 6 và tuần 7 ở ~/workspace/VKU/DoAn/.
   - driver/mpu6050/mpu6050.c (đường lỗi hiện có: đọc count lỗi giữ cạnh, đọc khung lỗi/count lẻ/đầy → reset, reset_pending; start/stop; reconfig ghi lại cấu hình cũ khi lỗi), recipe mpu6050-mod, mpu6050-test, mpu6050-uapi, projectb-overlays, projectb-image, wic/extlinux trong layer.
2. Kiểm trên laptop, chỉ đọc: HEAD và origin/main, cây làm việc có đúng các thay đổi tuần 7 ghi trong PROGRESS (hoặc đã commit nếu tôi đã bảo), conf build-clean là RT debug như template, mốc ~/projectb-baseline/rt-debug-20261030/ và nonrt-20261030/ qua sha256sum -c.
3. Kiểm bo qua scripts/bo.sh: uname (RT debug, nhãn Yocto), /boot/extlinux/extlinux.conf, /tmp còn gì, lockdep. Bo không lên thì dừng, nhờ tôi kiểm nguồn/mạng. Sau boot: scripts/bo-setup.sh (KO=… nếu module lấy từ sstate).
4. Phân tích các điểm cần quyết trong bàn giao tuần 8 (kernel trong ảnh nộp, đưa 400 kHz + nhãn extlinux vào layer, đường lỗi I2C, kref cho unbind, giữ hay bỏ tham số thử/debugfs, khung hỏng ở 1000 Hz, test không thao tác tay) và những điểm mới thấy khi đọc code. Mỗi điểm: phương án, đánh giá theo phụ thuộc giữa các tầng, chi phí, rủi ro, khả năng hoàn tác, khớp spec, khuyến nghị. Ghi quyết định và lý do vào báo cáo bước rồi làm theo, không chờ tôi duyệt, trừ khi chạm điều kiện dừng ở dưới.
5. Chia tuần thành các bước có mốc kiểm trên bo và lệnh cụ thể, rồi chạy liền các bước. Mốc phải có ít nhất:
   - Bus hỏng khi đang stream (tôi rút/cắm dây SDA hoặc nguồn module theo lệnh của bạn): không treo kernel, không treo app quá thời gian đã định, log có giới hạn tốc độ, dropped tăng đúng theo thời gian hỏng × nhịp chip, cắm lại thì tự hồi không cần rmmod; ghi sysfs lúc hỏng trả lỗi, không treo; lockdep im.
   - Unbind khi còn fd mở và khi đang stream: không use-after-free (kref), read trả lỗi rõ ràng, bind lại dùng được; 20 vòng sạch, slab không tăng.
   - Ảnh mới: kernel-module-mpu6050, mpu6050-test, overlay 400 kHz trong ảnh; driver tự nạp bằng udev (modalias), /dev/mpu6050 có quyền đã định; test chạy không thao tác tay (selftest + một lượt đo) và kết quả đọc được qua journal hoặc file.
   - Ghi thẻ bằng scripts/flash-verified.sh (tôi chạy lệnh sudo), boot từ thẻ mới, chạy lại hồi quy rút gọn (selftest, sweep tới 500 Hz, oflow, cfg, load 600) trên kernel của ảnh nộp.
   - Tài liệu: docs/architecture.md (tầng phần cứng → DT/overlay → driver → uapi → app; luồng dữ liệu, khóa, đường lỗi, số đo chính), chọn ảnh LA + log cho báo cáo, kịch bản demo đã diễn tập một lần đầy đủ.
   - Tag v1.0-chuyennganh: chỉ khi tôi bảo (commit, tag, push là việc tôi duyệt).
6. Bước cuối: ghi PROGRESS (kết quả tuần 8 + "GĐ2 — bàn giao"), ~/workspace/VKU/DoAn/week8-knowledge-summary.md, cập nhật CLAUDE.md, viết docs/gd2-prompt.md theo mẫu file này. Commit thì chờ tôi bảo.

Cách làm việc (đã ghi trong CLAUDE.md, nhắc lại):
- Bạn tự chạy build, chép, lệnh trên bo, chụp LA bằng sigrok-cli, phân tích. Tôi chỉ làm phần cứng (que LA, rút/cắm dây, nghiêng module, nguồn bo, thẻ SD), sudo, ghi thẻ, xóa dữ liệu, reboot/poweroff, commit/push.
- Mỗi bước báo ngắn, không quá 10 dòng: kỳ vọng và vì sao, số đo chính, khớp hay lệch, đã sửa gì. Lệch thì đổi điều kiện để tách giả thuyết, không đoán. Một thay đổi có ảnh hưởng thời gian thì A/B với bản trước trong cùng lần boot.
- Lockdep phải còn 1 sau mỗi bước trên RT debug; mất thì dừng và báo ngay. Kiểm cả nội dung dữ liệu (temp, |a|), không chỉ đếm.
- Lệnh dài đưa vào scripts/. Stress chạy nền; trong lúc chờ làm việc không đụng bo, không chạy thử khác trên bo.

Chỉ dừng hỏi tôi khi:
- Cần thao tác phần cứng. Nói rõ làm gì, lúc nào, rồi chờ.
- Kết quả làm đổi ABI uapi, tên sysfs, ý nghĩa dropped, thứ tự khóa, hoặc phải bỏ một mốc kiểm. Chọn giữa các phương án đã phân tích thì tự quyết theo khuyến nghị.
- Một mốc kiểm thất bại mà sau hai lần đổi điều kiện vẫn chưa rõ nguyên nhân.
- Trước reboot/poweroff bo, sudo, ghi thẻ, xóa dữ liệu, đổi PREFERRED_PROVIDER (lưu mốc trước, nói rõ sẽ mất gì khỏi deploy).

Văn phong ghi chú kỹ sư, không văn phong AI.
