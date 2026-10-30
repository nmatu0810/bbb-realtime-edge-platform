Bắt đầu tuần 7 của project B: FIFO của chip, đọc theo lô trong threaded IRQ, ts_ns theo từng cạnh INT (nội suy lùi chỉ cho mẫu dư), tràn FIFO → dropped + reset, quyết I2C 400 kHz, build non-RT và chạy bộ kiểm trên cả hai kernel.

1. Đọc ngữ cảnh theo thứ tự trong CLAUDE.md:
   - docs/PROGRESS.md: "Tuần 7 — bàn giao" trước (vòng sửa–thử tự động, điểm cần quyết, giới hạn mang sang, bẫy), rồi kết quả tuần 6, bàn giao tuần 6, kết quả tuần 5. Bàn giao tuần 4 và tuần 3 cho phần đổi kernel (PREFERRED_PROVIDER, ReachableStamps, mốc baseline, ghi thẻ).
   - docs/ROADMAP.md: kế hoạch driver tuần 4–8, ràng buộc (giả thuyết "512 chia hết cho 8" chưa kiểm, luôn reset FIFO khi tràn, số đo thời gian trên kernel không debug).
   - Ghi chú kiến thức tuần 5 và tuần 6 ở ~/workspace/VKU/DoAn/.
   - Datasheet ~/workspace/VKU/DoAn/Datasheet/: FIFO_EN (0x23), USER_CTRL (0x6A: FIFO_EN, FIFO_RST), CONFIG bit FIFO_MODE, FIFO_COUNTH/L (0x72–0x73), FIFO_R_W (0x74), INT_STATUS FIFO_OFLOW_INT, và phần FIFO trong Product Spec.
   - driver/mpu6050/mpu6050.c và git diff driver/: cây làm việc đã có bản FIFO viết ở cuối session tuần 6, build sạch, chưa thử trên bo (xem "Đã bắt đầu ở cuối session tuần 6" trong bàn giao tuần 7). Soát lại thiết kế đó, sửa nếu thấy sai, rồi thử; không viết lại từ đầu khi không có lý do.
2. Kiểm trên laptop, chỉ đọc: HEAD và origin/main (2873fef), cây làm việc chỉ có các thay đổi ghi trong bàn giao (driver FIFO, PROGRESS, docs/week7-prompt.md, scripts/bo.sh), hash .ko trong build-clean khớp 0328b42d…, wic/zImage/dtb/dtbo khớp tuần 4. Không chạy bitbake -e với PREFERRED_PROVIDER khác.
3. Kiểm bo qua scripts/bo.sh: uname, /tmp còn gì, lockdep. Bo không lên thì dừng, nhờ tôi kiểm nguồn/mạng.
4. Phân tích các điểm cần quyết trong mục bàn giao tuần 7 (ngắt kích đọc lô, phát hiện tràn, gán ts_ns, chỗ đặt 400 kHz, cách chạy non-RT) và những điểm mới thấy khi đọc datasheet. Mỗi điểm: phương án, đánh giá theo phụ thuộc giữa các tầng, chi phí, rủi ro, khả năng hoàn tác, khớp spec, khuyến nghị. Ghi quyết định và lý do vào báo cáo bước rồi làm theo, không chờ tôi duyệt, trừ khi chạm điều kiện dừng ở dưới.
5. Chia tuần thành các bước có mốc kiểm trên bo và lệnh cụ thể, rồi chạy liền các bước. Mốc phải có ít nhất:
   - dropped = 0 ở tốc độ tối đa đã chọn; bảo toàn Δirq/số lô, bản ghi, dropped so với thời gian × nhịp thật của chip (100.09 Hz ở "100 Hz").
   - Tràn cố ý (chặn thread hoặc dừng đọc) → dropped tăng đúng số mẫu mất, FIFO reset, tự hồi, không lệch bản ghi. Thí nghiệm có và không reset để kiểm giả thuyết "512 chia hết cho 8".
   - ts_ns: khoảng cách giữa các bản ghi đều theo chu kỳ chip, ts mỗi mẫu khớp cạnh INT của nó (ftrace mono + LA như tuần 6), mẫu dư lúc start có ts lùi đúng chu kỳ.
   - Đo lại 1000 Hz (lỗi NACK/Arbitration lost/treo 1 s của tuần 6) trong chế độ đọc lô, ở 100 kHz rồi 400 kHz. LA ở 400 kHz cần 12m–24m mẫu/s.
   - Hồi quy: selftest 13/13, mpu-check.sh (irq, dup, sweep, cfg, strace, restart, gap, shake), mpu-stress.sh (oc, sysfs, load 1800 chạy nền, mod), lockdep im sau mỗi đợt.
   - Cả hai kernel (RT debug và non-RT) qua bộ kiểm; số đo thời gian chính thức trên kernel không debug.
6. Bước cuối: ghi PROGRESS (kết quả tuần 7 + "Tuần 8 — bàn giao"), ~/workspace/VKU/DoAn/week7-knowledge-summary.md, cập nhật CLAUDE.md, viết docs/week8-prompt.md theo mẫu file này. Commit thì chờ tôi bảo.

Cách làm việc (đã ghi trong CLAUDE.md, nhắc lại):
- Bạn tự chạy build, chép, lệnh trên bo, chụp LA bằng sigrok-cli, phân tích. Tôi chỉ làm phần cứng (que LA, nghiêng module, nguồn bo, thẻ SD), sudo, ghi thẻ, xóa dữ liệu, reboot/poweroff, commit/push.
- Mỗi bước báo ngắn, không quá 10 dòng: kỳ vọng và vì sao, số đo chính, khớp hay lệch, đã sửa gì. Lệch thì đổi điều kiện để tách giả thuyết, không đoán.
- Lockdep phải còn 1 sau mỗi bước; mất thì dừng và báo ngay.
- Lệnh dài đưa vào scripts/. Stress 30 phút chạy nền; trong lúc chờ làm việc không đụng bo, không chạy thử khác trên bo.

Chỉ dừng hỏi tôi khi:
- Cần thao tác phần cứng. Nói rõ làm gì, lúc nào, rồi chờ.
- Kết quả làm đổi ABI uapi, tên sysfs, ý nghĩa dropped, thứ tự khóa, hoặc phải bỏ một mốc kiểm. Chọn giữa các phương án đã phân tích thì tự quyết theo khuyến nghị.
- Một mốc kiểm thất bại mà sau hai lần đổi điều kiện vẫn chưa rõ nguyên nhân.
- Trước reboot/poweroff bo, sudo, ghi thẻ, xóa dữ liệu, đổi PREFERRED_PROVIDER (lưu mốc trước, nói rõ sẽ mất gì khỏi deploy).

Văn phong ghi chú kỹ sư, không văn phong AI.
