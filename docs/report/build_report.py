#!/usr/bin/env python3
"""Ghép nội dung (content.py) vào mẫu báo cáo của khoa.

    python docs/report/build_report.py <mẫu.docx> <thư mục hình> <ra.docx>

Giữ nguyên mẫu: bìa, các section (số trang La Mã/Ả Rập), style DATN_*, Heading 1–3 tự đánh số,
trường TOC (mục lục, danh mục hình, danh mục bảng). Kết quả cũ của các trường TOC bị xóa và
settings.xml bật updateFields: Word dựng lại mục lục khi mở tệp (hỏi một lần, chọn Yes).
Cần lxml và Pillow.
"""
import copy
import os
import re
import shutil
import sys
import tempfile
import zipfile

from lxml import etree
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import content as C  # noqa: E402

W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
R = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'
WP = 'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing'
A = 'http://schemas.openxmlformats.org/drawingml/2006/main'
PIC = 'http://schemas.openxmlformats.org/drawingml/2006/picture'
NS = {'w': W, 'r': R}
q = lambda t: '{%s}%s' % (W, t)
XML_SPACE = '{http://www.w3.org/XML/1998/namespace}space'

TEMPLATE, FIGDIR, OUT = sys.argv[1:4]
work = tempfile.mkdtemp()
with zipfile.ZipFile(TEMPLATE) as z:
    z.extractall(work)
for dp, _, fs in os.walk(work):
    for f in fs:
        if os.path.islink(os.path.join(dp, f)):
            os.remove(os.path.join(dp, f))

doc_path = os.path.join(work, 'word', 'document.xml')
tree = etree.parse(doc_path)
root = tree.getroot()
body = root.find('w:body', NS)
K = list(body)  # chỉ số theo mẫu gốc


# ------------------------------------------------------------------ tiện ích
def text(el):
    return ''.join(t.text or '' for t in el.iter(q('t')))


def text_runs(p):
    return [r for r in p.iter(q('r')) if r.find('w:t', NS) is not None]


def set_text(p, s):
    rs = text_runs(p)
    for r in rs[1:]:
        for t in r.findall('w:t', NS):
            r.remove(t)
    t = rs[0].find('w:t', NS)
    t.text = s
    t.set(XML_SPACE, 'preserve')


def set_last(p, s):
    """Giữ nhãn (các run đầu), thay giá trị ở run có chữ cuối cùng."""
    rs = text_runs(p)
    t = rs[-1].find('w:t', NS)
    t.text = s
    t.set(XML_SPACE, 'preserve')


def expect(i, start):
    assert text(K[i]).startswith(start), (i, text(K[i])[:60])


def el(tag, attrs=None, children=(), txt=None):
    e = etree.Element(q(tag))
    for k, v in (attrs or {}).items():
        e.set(q(k), str(v))
    for c in children:
        e.append(c)
    if txt is not None:
        e.text = txt
        e.set(XML_SPACE, 'preserve')
    return e


def run(s, rpr=None):
    r = el('r')
    if rpr is not None:
        r.append(rpr)
    r.append(el('t', txt=s))
    return r


def para(style, s=None, runs=(), ppr_extra=()):
    ppr = el('pPr', children=[el('pStyle', {'val': style})] + list(ppr_extra))
    p = el('p', children=[ppr])
    if s is not None:
        p.append(run(s))
    for r in runs:
        p.append(r)
    return p


def page_break():
    return el('p', children=[el('r', children=[el('br', {'type': 'page'})])])


# ------------------------------------------------------------------ bìa (hai bản giống nhau)
cv = C.COVER
for off in (0, 30):
    expect(11 + off, 'ĐỒ ÁN')
    set_text(K[11 + off], cv['kind'])
    set_text(K[12 + off], cv['title'])
    expect(17 + off, 'Họ và tên sinh viên')
    set_last(K[17 + off], cv['student'])
    set_text(K[18 + off], f"Mã sinh viên: {cv['id']}")
    set_last(K[19 + off], cv['class'])
    set_last(K[20 + off], cv['major'])
    set_last(K[21 + off], cv['advisor'])
    set_text(K[28 + off], cv['date'])
# Hai bìa nối nhau không có ngắt trang; bìa 1 vừa một trang, bìa 2 tràn ngày sang trang sau vì tên
# đề tài dài 3 dòng thay vì 1: chỉ bớt hai dòng trống trước ngày ở bìa 2.
for i in (56, 57):
    assert not text(K[i]).strip()
    body.remove(K[i])

# ------------------------------------------------------------------ chân trang: tên – lớp
for f in os.listdir(os.path.join(work, 'word')):
    if re.match(r'(header|footer)\d+\.xml$', f):
        fp = os.path.join(work, 'word', f)
        ft = etree.parse(fp)
        for t in ft.getroot().iter(q('t')):
            if t.text == 'Nguyễn Văn A':
                t.text = cv['student']
            elif t.text == ' – 21CE1':
                t.text = f" – {cv['class']}"
        ft.write(fp, xml_declaration=True, encoding='UTF-8', standalone=True)

# ------------------------------------------------------------------ lời cảm ơn, nhận xét
expect(60, 'LỜI CẢM ƠN')
set_text(K[61], C.THANKS[0])
set_text(K[62], C.THANKS[1])
body.remove(K[63])
expect(80, 'Đà Nẵng')
set_last(K[80], ', …/…/2026')
set_last(K[86], cv['advisor'])

# ------------------------------------------------------------------ từ viết tắt
tbl = K[137]
rows = tbl.findall('w:tr', NS)
hdr, proto = rows[0], rows[1]
for r in rows[1:]:
    tbl.remove(r)
for tc, s in zip(hdr.findall('w:tc', NS), ('TỪ VIẾT TẮT', 'Ý NGHĨA')):
    set_text(tc.find('w:p', NS), s)
for a, b in C.ABBR:
    tr = copy.deepcopy(proto)
    cells = tr.findall('w:tc', NS)
    set_text(cells[0].find('w:p', NS), a)
    set_text(cells[1].find('w:p', NS), b)
    jc = cells[1].find('w:p/w:pPr/w:jc', NS)
    if jc is not None:
        jc.set(q('val'), 'left')
    tbl.append(tr)


# ------------------------------------------------------------------ xóa kết quả cũ của trường TOC
def clear_field(instr_part, placeholder):
    runs_all = list(body.iter(q('r')))
    start = next(i for i, r in enumerate(runs_all)
                 if r.find('w:instrText', NS) is not None and instr_part in (r.find('w:instrText', NS).text or ''))
    # lùi về fldChar begin
    b = start
    while runs_all[b].find('w:fldChar', NS) is None or runs_all[b].find('w:fldChar', NS).get(q('fldCharType')) != 'begin':
        b -= 1
    depth, sep, end = 0, None, None
    for i in range(b, len(runs_all)):
        fc = runs_all[i].find('w:fldChar', NS)
        if fc is None:
            continue
        kind = fc.get(q('fldCharType'))
        if kind == 'begin':
            depth += 1
        elif kind == 'separate' and depth == 1 and sep is None:
            sep = i
        elif kind == 'end':
            depth -= 1
            if depth == 0:
                end = i
                break
    r_sep, r_end = runs_all[sep], runs_all[end]
    p_sep, p_end = r_sep.getparent(), r_end.getparent()
    # bỏ mọi thứ giữa separate và end
    for r in runs_all[sep + 1:end]:
        if r.getparent() is not None:
            r.getparent().remove(r)
    if p_sep is not p_end:
        n = p_sep.getnext()
        while n is not None and n is not p_end:
            nxt = n.getnext()
            if n.tag == q('p'):
                body.remove(n)
            n = nxt
        r_end.getparent().remove(r_end)
        p_sep.append(r_end)
        if not text_runs(p_end) and p_end.find('.//w:fldChar', NS) is None:
            body.remove(p_end)
    # các hyperlink/bookmark rỗng còn sót trong đoạn đầu
    for h in list(p_sep.findall('w:hyperlink', NS)):
        p_sep.remove(h)
    r_sep.addnext(run(placeholder))


clear_field('TOC \\h \\z \\t "Heading 1', '(Mục lục: trong Word chọn Update Field, hoặc F9)')
clear_field('TOC \\h \\z \\t "DATN_Caption_hinh', '(Danh mục hình: Update Field)')
clear_field('TOC \\h \\z \\c "Bảng"', '(Danh mục bảng: Update Field)')
# Chú thích bảng ghi số cố định (STYLEREF \\s mỗi trình mở hiểu một kiểu), nên danh mục bảng
# lấy theo style như danh mục hình.
for it in body.iter(q('instrText')):
    if 'TOC \\h \\z \\c "Bảng"' in (it.text or ''):
        it.text = ' TOC \\h \\z \\t "DATN_Caption_bang,1" '

# ------------------------------------------------------------------ hình: media, rels, content types
rels_path = os.path.join(work, 'word', '_rels', 'document.xml.rels')
rels = etree.parse(rels_path)
rid_n = [0]
media_n = [0]
docpr_id = [1000]


def add_image(fname):
    src = os.path.join(FIGDIR, fname)
    media_n[0] += 1
    name = f'report_fig{media_n[0]}.png'
    shutil.copy(src, os.path.join(work, 'word', 'media', name))
    rid_n[0] += 1
    rid = f'rIdRpt{rid_n[0]}'
    rel = etree.SubElement(rels.getroot(), '{http://schemas.openxmlformats.org/package/2006/relationships}Relationship')
    rel.set('Id', rid)
    rel.set('Type', 'http://schemas.openxmlformats.org/officeDocument/2006/relationships/image')
    rel.set('Target', f'media/{name}')
    w, h = Image.open(src).size
    max_w = 15.5  # cm, vừa lề 3 cm trái + 2 cm phải trên A4
    cx = int(max_w * 360000)
    cy = int(cx * h / w)
    if cy > 11 * 360000:  # hình cao: giới hạn chiều cao
        cy = 11 * 360000
        cx = int(cy * w / h)
    docpr_id[0] += 1
    xml = f'''<w:r xmlns:w="{W}" xmlns:wp="{WP}" xmlns:a="{A}" xmlns:pic="{PIC}" xmlns:r="{R}">
<w:rPr><w:noProof/></w:rPr><w:drawing><wp:inline distT="0" distB="0" distL="0" distR="0">
<wp:extent cx="{cx}" cy="{cy}"/><wp:effectExtent l="0" t="0" r="0" b="0"/>
<wp:docPr id="{docpr_id[0]}" name="Hình {docpr_id[0]}"/>
<wp:cNvGraphicFramePr><a:graphicFrameLocks noChangeAspect="1"/></wp:cNvGraphicFramePr>
<a:graphic><a:graphicData uri="http://schemas.openxmlformats.org/drawingml/2006/picture"><pic:pic>
<pic:nvPicPr><pic:cNvPr id="{docpr_id[0]}" name="{name}"/><pic:cNvPicPr><a:picLocks noChangeAspect="1"/></pic:cNvPicPr></pic:nvPicPr>
<pic:blipFill><a:blip r:embed="{rid}"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill>
<pic:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="{cx}" cy="{cy}"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom></pic:spPr>
</pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r>'''
    return etree.fromstring(re.sub(r'>\s+<', '><', xml))


ct_path = os.path.join(work, '[Content_Types].xml')
ct = etree.parse(ct_path)
CT = 'http://schemas.openxmlformats.org/package/2006/content-types'
if not any(d.get('Extension') == 'png' for d in ct.getroot().findall('{%s}Default' % CT)):
    d = etree.SubElement(ct.getroot(), '{%s}Default' % CT)
    d.set('Extension', 'png')
    d.set('ContentType', 'image/png')

# ------------------------------------------------------------------ đánh số hình/bảng theo chương
labels = {}
chap = 0
nf = nt = 0
for b in C.BODY + [('h1', 'PL')] + C.APPENDIX:
    if b[0] == 'h1':
        chap = chap + 1 if b[1] != 'PL' else 'PL'
        nf = nt = 0
    elif b[0] == 'fig':
        nf += 1
        labels['fig:' + b[1]] = f'Hình {chap}.{nf}'
    elif b[0] == 'tab':
        nt += 1
        labels['tab:' + b[1]] = f'Bảng {chap}.{nt}'


def resolve(s):
    def sub(m):
        return labels[m.group(1)]
    out = re.sub(r'\{((?:fig|tab):\w+)\}', sub, s)
    assert not re.search(r'\{(fig|tab):', out), out
    return out


# ------------------------------------------------------------------ dựng khối
TBL_W = 9072             # dxa: 16 cm vùng chữ


def caption_table(num, title):
    return para('DATNCaptionbang', f'{num}. {title}')


def cell(s, width, header=False, align='left'):
    ppr = el('pPr', children=[el('pStyle', {'val': 'DATNText'}), el('keepNext'),
                              el('spacing', {'before': 40, 'after': 40, 'line': 264, 'lineRule': 'auto'}),
                              el('ind', {'firstLine': 0}), el('jc', {'val': 'center' if header else align})])
    rpr = el('rPr', children=[el('b'), el('bCs'), el('sz', {'val': 24})]) if header else \
        el('rPr', children=[el('sz', {'val': 24})])
    p = el('p', children=[ppr, run(s, rpr)])
    tcpr = el('tcPr', children=[el('tcW', {'w': width, 'type': 'dxa'})])
    if header:  # thứ tự schema: tcW, ..., shd, ..., vAlign
        tcpr.append(el('shd', {'val': 'clear', 'color': 'auto', 'fill': 'E7ECF4'}))
    tcpr.append(el('vAlign', {'val': 'center'}))
    return el('tc', children=[tcpr, p])


def table(head, rows, ratios):
    widths = [int(TBL_W * r / sum(ratios)) for r in ratios]
    widths[-1] = TBL_W - sum(widths[:-1])
    border = lambda n: el(n, {'val': 'single', 'sz': 4, 'space': 0, 'color': 'auto'})
    tblpr = el('tblPr', children=[
        el('tblStyle', {'val': 'TableGrid'}), el('tblW', {'w': TBL_W, 'type': 'dxa'}), el('jc', {'val': 'center'}),
        el('tblBorders', children=[border(n) for n in ('top', 'left', 'bottom', 'right', 'insideH', 'insideV')]),
        el('tblLayout', {'type': 'fixed'}), el('tblLook', {'val': '04A0'})])
    t = el('tbl', children=[tblpr, el('tblGrid', children=[el('gridCol', {'w': w}) for w in widths])])
    hr = el('tr', children=[el('trPr', children=[el('tblHeader')])])
    for s, w in zip(head, widths):
        hr.append(cell(s, w, header=True))
    t.append(hr)
    for row in rows:
        tr = el('tr', children=[el('trPr', children=[el('cantSplit')])])
        for k, (s, w) in enumerate(zip(row, widths)):
            tr.append(cell(s, w, align='left' if len(s) > 12 else 'center'))
        t.append(tr)
    return t


MONO = lambda: el('rPr', children=[el('rFonts', {'ascii': 'Courier New', 'hAnsi': 'Courier New', 'cs': 'Courier New'}),
                                   el('sz', {'val': 20})])


def code(lines):
    out = []
    for i, ln in enumerate(lines):
        # thứ tự schema trong pPr: keepNext, shd, spacing, ind, jc
        ppr = [el('shd', {'val': 'clear', 'color': 'auto', 'fill': 'F2F2F2'}),
               el('spacing', {'before': 60 if i == 0 else 0, 'after': 60 if i == len(lines) - 1 else 0,
                              'line': 240, 'lineRule': 'auto'}),
               el('ind', {'left': 567, 'firstLine': 0}), el('jc', {'val': 'left'})]
        if i < len(lines) - 1:
            ppr.insert(0, el('keepNext'))
        out.append(para('DATNText', runs=[run(ln if ln else ' ', MONO())], ppr_extra=ppr))
    return out


def build(blocks, first_title_no_break=False):
    out = []
    first = True
    for b in blocks:
        kind = b[0]
        if kind in ('title', 'h1'):
            if first and first_title_no_break:
                # đầu section mới: pageBreakBefore thay cho đoạn ngắt trang (Word bỏ qua ở đầu trang,
                # LibreOffice không tự ngắt trang ở section nextPage)
                out.append(para('DATNTitle1' if kind == 'title' else 'Heading1', b[1],
                                ppr_extra=[el('pageBreakBefore')]))
            else:
                out.append(page_break())
                out.append(para('DATNTitle1' if kind == 'title' else 'Heading1', b[1]))
        elif kind == 'mo':
            out.append(para('DCCTH1', b[1]))
        elif kind == 'sub':
            out.append(para('DATNTitle2', b[1]))
        elif kind == 'h2':
            out.append(para('Heading2', b[1]))
        elif kind == 'h3':
            out.append(para('Heading3', b[1]))
        elif kind == 'p':
            out.append(para('DATNText', resolve(b[1])))
        elif kind == 'bul':
            for s in b[1]:
                out.append(para('DATNStep1', resolve(s)))
        elif kind == 'code':
            out.extend(code(b[1]))
        elif kind == 'fig':
            _, lab, fname, cap = b
            out.append(el('p', children=[el('pPr', children=[el('pStyle', {'val': 'GTHinh'})]), add_image(fname)]))
            out.append(para('DATNCaptionhinh', f"{labels['fig:' + lab]}. {cap}"))
        elif kind == 'tab':
            _, lab, cap, head, rows, ratios = b
            out.append(caption_table(labels['tab:' + lab], cap))
            out.append(table(head, rows, ratios))
            out.append(el('p', children=[el('pPr', children=[el('pStyle', {'val': 'DATNText'}),
                                                              el('spacing', {'before': 0, 'after': 0})])]))
        else:
            raise ValueError(kind)
        first = False
    return out


# ------------------------------------------------------------------ thân bài: thay 171..250, giữ 170 (ngắt trang) và 251 (sectPr)
expect(171, 'MỞ ĐẦU')
sect_body = K[251].find('.//w:sectPr', NS)
assert sect_body is not None
# section thân bài: mẫu để continuous + một ngắt trang, nên trang 1 bị trống; đổi sang nextPage
for t in sect_body.findall('w:type', NS):
    sect_body.remove(t)
assert not text(K[170]).strip()
for e in K[170:251]:
    body.remove(e)
for r in K[251].findall('w:r', NS):
    K[251].remove(r)
anchor = K[251]
for e in build(C.BODY, first_title_no_break=True):
    anchor.addprevious(e)

# ------------------------------------------------------------------ tài liệu tham khảo: giữ tiêu đề 252, thay 253..270
expect(252, 'DANH MỤC TÀI LIỆU THAM KHẢO')
expect(273, 'PHỤ LỤC')
for e in K[253:271]:
    body.remove(e)
pos = K[252]
for head, items in C.REFS.items():
    p = para('DATNTitle2', head)
    pos.addnext(p)
    pos = p
    for s in items:
        p = para('DATNTLTK', s)
        pos.addnext(p)
        pos = p

# ------------------------------------------------------------------ phụ lục: sau 273, thay 274
body.remove(K[274])
pos = K[273]
appendix = []
for b in C.APPENDIX:
    if b[0] == 'p':
        appendix.append(para('DATNText', b[1], ppr_extra=[el('keepNext'), el('ind', {'firstLine': 0})]))
        appendix[-1].find('w:r', NS).insert(0, el('rPr', children=[el('b')]))
    else:
        appendix.extend(build([b]))
for e in appendix:
    pos.addnext(e)
    pos = e

# ------------------------------------------------------------------ bookmark mồ côi
starts = {b.get(q('id')) for b in root.iter(q('bookmarkStart'))}
ends = {b.get(q('id')) for b in root.iter(q('bookmarkEnd'))}
for b in list(root.iter(q('bookmarkStart'))):
    if b.get(q('id')) not in ends:
        b.getparent().remove(b)
for b in list(root.iter(q('bookmarkEnd'))):
    if b.get(q('id')) not in starts:
        b.getparent().remove(b)

tree.write(doc_path, xml_declaration=True, encoding='UTF-8', standalone=True)
rels.write(rels_path, xml_declaration=True, encoding='UTF-8', standalone=True)
ct.write(ct_path, xml_declaration=True, encoding='UTF-8', standalone=True)

# Word cập nhật trường (mục lục) khi mở
sp = os.path.join(work, 'word', 'settings.xml')
st = etree.parse(sp)
if st.getroot().find('w:updateFields', NS) is None:
    # thứ tự schema: updateFields đứng ngay trước hdrShapeDefaults / footnotePr / ... / rsids
    after = ('hdrShapeDefaults', 'footnotePr', 'endnotePr', 'compat', 'docVars', 'rsids')
    nxt = next((c for c in st.getroot() if etree.QName(c).localname in after), None)
    uf = el('updateFields', {'val': 'true'})
    if nxt is not None:
        nxt.addprevious(uf)
    else:
        st.getroot().append(uf)
st.write(sp, xml_declaration=True, encoding='UTF-8', standalone=True)

if os.path.exists(OUT):
    os.remove(OUT)
with zipfile.ZipFile(OUT, 'w', zipfile.ZIP_DEFLATED) as z:
    # [Content_Types].xml phải đứng đầu
    z.write(ct_path, '[Content_Types].xml')
    for dp, _, fs in os.walk(work):
        for f in sorted(fs):
            full = os.path.join(dp, f)
            arc = os.path.relpath(full, work)
            if arc != '[Content_Types].xml':
                z.write(full, arc)
shutil.rmtree(work)
print('ok', OUT, f'{nf} hình chương cuối;', len(labels), 'nhãn')
