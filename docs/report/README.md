# Báo cáo ĐACN2

- `Bao-cao-DACN2.docx`: báo cáo, điền vào mẫu của khoa (`docs/Do_an_co_so_chuyen_nganh_VN_26.01.02.docx`,
  không commit). Mở bằng Word, chọn Yes khi hỏi cập nhật trường: mục lục, danh mục hình, danh mục bảng
  được dựng lại và số trang theo section của mẫu (phần đầu La Mã, thân bài từ 1).
- `Bao-cao-DACN2-xem-truoc.pdf`: bản xuất bằng LibreOffice để xem nhanh. LibreOffice đánh số trang liền
  từ trang lời cảm ơn, không theo section như Word; nộp thì xuất PDF từ Word.
- `BaoCao_DACN2.pdf`: bản hoàn chỉnh đã chỉnh trong Word, là bản chính để nộp. `Bao-cao-DACN2.docx` và
  `content.py` là bản nháp cũ hơn; dựng lại không có các chỉnh sửa trong Word.
- `De-cuong-DACN2-v2.docx`: đề cương đã sửa cho khớp sản phẩm sau tuần 8. Có số điện thoại, email nên
  không commit (repo public); bản nộp ban đầu `docs/Đề cương DACN2.docx` cũng vậy.

Dựng lại (cần lxml, Pillow, matplotlib, numpy):

    python docs/report/figures.py docs/report/fig
    python docs/report/build_report.py docs/Do_an_co_so_chuyen_nganh_VN_26.01.02.docx docs/report/fig docs/report/Bao-cao-DACN2.docx

Nội dung sửa ở `content.py`; hình ở `figures.py` (sơ đồ, dạng sóng từ `docs/*.sr`, đồ thị từ
`docs/logs-2026-10-30/`). Xem trước bằng LibreOffice trong container: `Dockerfile`, `topdf.py`.
