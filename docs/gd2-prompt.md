Bắt đầu GĐ2 (tuần 9) của project B: san bằng .config RT/non-RT, benchmark độ trễ (cyclictest, IRQ → userspace) ở chế độ Benchmark (1 core + SCHED_FIFO), benchmark runner (fork/exec/waitpid), machine/distro conf riêng, watchdog handoff U-Boot → omap_wdt + RuntimeWatchdogSec. Mốc tuần 11: số liệu PREEMPT_RT sạch, so được với non-RT trên cùng .config ngoài preemption.

1. Đọc ngữ cảnh theo thứ tự trong CLAUDE.md:
   - docs/PROGRESS.md: "GĐ2 — bàn giao" trước (hệ thống đang chạy, ảnh nộp trên thẻ, điểm cần quyết, giới hạn mang sang, bẫy), rồi kết quả tuần 8. Bàn giao tuần 3 cho mốc benchmark RT/non-RT tuần 3 (~140 dòng .config khác nhau ngoài preemption), bàn giao tuần 3–4 cho ghi thẻ, PREFERRED_PROVIDER/ReachableStamps.
   - docs/ROADMAP.md: phân chia GĐ1/GĐ2, benchmark runner GĐ2, điều chỉnh 2026-10-30 (tuần 7, tuần 8).
   - docs/architecture.md (số đo GĐ1, ts lấy ở đâu, độ trễ ts so với cạnh theo kernel).
   - Spec gốc, phần tuần 9–11 và chế độ Benchmark.
   - Ghi chú kiến thức tuần 7 và tuần 8 ở ~/workspace/VKU/DoAn/.
   - recipes-kernel/linux (projectb-kernel.inc, fragment), scripts/templates/local.conf.in, projectb-image.bb.
2. Kiểm trên laptop, chỉ đọc: HEAD và origin/main, cây làm việc có đúng các thay đổi tuần 8 ghi trong PROGRESS (hoặc đã commit/tag nếu tôi đã bảo), conf build-clean như template (RT, PROJECTB_KERNEL_DEBUG "0"), mốc ~/projectb-baseline/rt-submit-20261030/ qua sha256sum -c.
3. Kiểm bo qua scripts/bo.sh: uname (RT không debug), journalctl -u mpu6050-boot-check ("ĐẠT"), lockdep không có (kernel không debug), /tmp còn gì. Host key đổi thì làm như bàn giao. Sau boot: scripts/bo-setup.sh (KO=… nếu cần nạp module khác ảnh).
4. Phân tích các điểm cần quyết trong bàn giao GĐ2 (kernel đo benchmark, san bằng .config bằng fragment nào, tách projectb-image/-dev, user cho sensord, đo lại hằng độ trễ ts, cách đo IRQ → userspace có tính irq26 → irq50) và những điểm mới thấy khi đọc. Mỗi điểm: phương án, đánh giá theo phụ thuộc giữa các tầng, chi phí, rủi ro, khả năng hoàn tác, khớp spec, khuyến nghị. Ghi quyết định và lý do vào báo cáo bước rồi làm theo, không chờ tôi duyệt, trừ khi chạm điều kiện dừng ở dưới.
5. Chia tuần 9–11 thành các bước có mốc kiểm trên bo và lệnh cụ thể, rồi chạy liền các bước. Mốc phải có ít nhất:
   - .config RT và non-RT chỉ còn khác ở preemption (diff có giải thích từng dòng còn lại); cả hai kernel build từ layer, mốc mới trong ~/projectb-baseline/ có SHA256SUMS. Driver qua hồi quy rút gọn (selftest, sweep tới 1000 Hz, oflow, mpu-lag NACK 0) trên cả hai.
   - cyclictest ở chế độ Benchmark (1 core, SCHED_FIFO, không HMI/AI), có và không có tải stress-ng, cùng thời lượng trên hai kernel; histogram và max, chạy lại được bằng một lệnh.
   - Độ trễ IRQ → userspace của /dev/mpu6050: từ cạnh INT (LA hoặc ftrace irq26) tới lúc app nhận bản ghi, phân bố có/không tải, hai kernel.
   - Benchmark runner (C, fork/exec/waitpid): chạy chuỗi bài đo, thu kết quả, mã thoát rõ ràng; có test.
   - Machine conf riêng (bbb-projectb.conf) hoặc ghi rõ lý do dời; watchdog: treo userspace có chủ đích → bo tự reset, log chứng minh.
   - Tài liệu: số liệu benchmark vào docs/ (bảng + cách tái lập).
6. Bước cuối: ghi PROGRESS (kết quả tuần 9–11 + bàn giao tuần 12), ~/workspace/VKU/DoAn/week9-…/week11-knowledge-summary.md theo tuần đã làm, cập nhật CLAUDE.md, viết prompt cho tuần 12 theo mẫu file này. Commit thì chờ tôi bảo.

Cách làm việc (đã ghi trong CLAUDE.md, nhắc lại):
- Bạn tự chạy build, chép, lệnh trên bo, chụp LA bằng sigrok-cli, phân tích. Tôi chỉ làm phần cứng (que LA, rút/cắm dây, nghiêng module, nguồn bo, thẻ SD), sudo, ghi thẻ, xóa dữ liệu, reboot/poweroff, commit/push.
- Mỗi bước báo ngắn, không quá 10 dòng: kỳ vọng và vì sao, số đo chính, khớp hay lệch, đã sửa gì. Lệch thì đổi điều kiện để tách giả thuyết, không đoán. Một thay đổi có ảnh hưởng thời gian thì A/B với bản trước trong cùng lần boot.
- Đo benchmark: cố định điều kiện (governor, core, tải, thời lượng, nhiệt nếu đo được), ghi lại đủ để chạy lại. Không so số đo của hai kernel khác .config.
- Kernel debug (lockdep) chỉ khi sửa driver; đo thời gian luôn trên kernel không debug. Kiểm cả nội dung dữ liệu (temp, |a|), không chỉ đếm.
- Lệnh dài đưa vào scripts/. Benchmark và stress chạy nền; trong lúc chờ làm việc không đụng bo, không chạy thử khác trên bo. Vòng chờ phía laptop không dùng lệnh busybox không có (pgrep).
- Chép log từ /tmp của bo về repo trước khi đưa lệnh ghi thẻ.

Chỉ dừng hỏi tôi khi:
- Cần thao tác phần cứng. Nói rõ làm gì, lúc nào, rồi chờ.
- Kết quả làm đổi ABI uapi, tên sysfs, ý nghĩa dropped, thứ tự khóa, hoặc phải bỏ một mốc kiểm. Chọn giữa các phương án đã phân tích thì tự quyết theo khuyến nghị.
- Một mốc kiểm thất bại mà sau hai lần đổi điều kiện vẫn chưa rõ nguyên nhân.
- Trước reboot/poweroff bo, sudo, ghi thẻ, xóa dữ liệu, đổi PREFERRED_PROVIDER (lưu mốc trước, nói rõ sẽ mất gì khỏi deploy).

Văn phong ghi chú kỹ sư, không văn phong AI.
