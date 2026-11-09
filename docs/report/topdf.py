# Mở docx bằng LibreOffice (UNO), cập nhật mục lục/danh mục và trường, xuất PDF. Chỉ để xem trước.
import subprocess, sys, time, uno
from com.sun.star.beans import PropertyValue
src, dst = sys.argv[1], sys.argv[2]
p = subprocess.Popen(['soffice', '--headless', '--invisible', '--norestore',
                      '--accept=socket,host=127.0.0.1,port=2002;urp;'])
ctx = uno.getComponentContext()
res = ctx.ServiceManager.createInstanceWithContext('com.sun.star.bridge.UnoUrlResolver', ctx)
for _ in range(60):
    try:
        c = res.resolve('uno:socket,host=127.0.0.1,port=2002;urp;StarOffice.ComponentContext'); break
    except Exception:
        time.sleep(0.5)
desk = c.ServiceManager.createInstanceWithContext('com.sun.star.frame.Desktop', c)
def pv(n, v):
    x = PropertyValue(); x.Name = n; x.Value = v; return x
doc = desk.loadComponentFromURL('file://' + src, '_blank', 0, (pv('Hidden', True),))
doc.getTextFields().refresh()
idx = doc.getDocumentIndexes()
for _ in range(2):
    for i in range(idx.getCount()):
        idx.getByIndex(i).update()
doc.storeToURL('file://' + dst, (pv('FilterName', 'writer_pdf_Export'),))
doc.close(True)
p.terminate()
