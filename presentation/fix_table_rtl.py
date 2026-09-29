# Add rtl="1" to every paragraph inside table cells (pptxgenjs does not emit it there)
import zipfile, re, shutil, sys

SRC = "SmartHome-Defense.pptx"
TMP = "SmartHome-Defense_fixed.pptx"
TARGETS = ("slide13.xml", "slide15.xml", "slide22.xml")

z = zipfile.ZipFile(SRC)
items = [(n, z.read(n)) for n in z.namelist()]
z.close()

def fix_table_rtl(xml: str) -> str:
    out, pos = [], 0
    for m in re.finditer(r"<a:tbl>.*?</a:tbl>", xml, flags=re.S):
        out.append(xml[pos:m.start()])
        seg = m.group(0)
        seg = re.sub(r"<a:pPr(?![^>]*\brtl=)", '<a:pPr rtl="1"', seg)
        # paragraphs with no pPr at all
        seg = re.sub(r"<a:p><a:r>", '<a:p><a:pPr rtl="1"/><a:r>', seg)
        out.append(seg)
        pos = m.end()
    out.append(xml[pos:])
    return "".join(out)

changed = 0
with zipfile.ZipFile(TMP, "w", zipfile.ZIP_DEFLATED) as out:
    for name, data in items:
        if name.startswith("ppt/slides/slide") and name.endswith(".xml") and name.split("/")[-1] in TARGETS:
            xml = data.decode("utf-8")
            new = fix_table_rtl(xml)
            if new != xml:
                changed += 1
            data = new.encode("utf-8")
        out.writestr(name, data)

shutil.move(TMP, SRC)
print(f"rtl fix applied to {changed} slide(s)")
