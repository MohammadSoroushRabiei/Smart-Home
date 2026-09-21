# -*- coding: utf-8 -*-
"""Post-process the generated docx:
1) remove empty <w:pgNumType/> tags (WPS compat)
2) patch footer PAGE fields with explicit \\* ROMAN / \\* arabic switches per section
"""
import re
import shutil
import sys
import zipfile

SRC = "SmartHome-Project-Report.docx"

with zipfile.ZipFile(SRC) as z:
    names = z.namelist()
    data = {n: z.read(n) for n in names}

doc = data["word/document.xml"].decode("utf-8")

# 1) remove empty pgNumType
doc, n_removed = re.subn(r"<w:pgNumType/>", "", doc)

# 2) map sections -> footer rels -> format
rels = data["word/_rels/document.xml.rels"].decode("utf-8")
rel_map = dict(re.findall(r'Id="([^"]+)"[^>]*Target="(footer\d+\.xml)"', rels))

sect_blocks = re.findall(r"<w:sectPr[^>]*>.*?</w:sectPr>", doc, flags=re.S)
footer_fmt = {}  # footer file -> fmt
for block in sect_blocks:
    m = re.search(r'<w:pgNumType[^>]*w:fmt="([^"]+)"', block)
    fmt = m.group(1) if m else None
    for rid in re.findall(r'<w:footerReference[^>]*r:id="([^"]+)"', block):
        f = rel_map.get(rid)
        if f:
            footer_fmt["word/" + f] = fmt

patched = []
for fname, fmt in footer_fmt.items():
    if fname not in data:
        continue
    xml = data[fname].decode("utf-8")
    switch = "ROMAN" if fmt == "upperRoman" else ("arabic" if fmt else None)
    if switch:
        new_xml, n = re.subn(
            r'(<w:instrText[^>]*>)\s*PAGE\s*(</w:instrText>)',
            r'\1 PAGE \\* ' + switch + r' \\* MERGEFORMAT \2',
            xml,
        )
        if n:
            data[fname] = new_xml.encode("utf-8")
            patched.append((fname, switch, n))

data["word/document.xml"] = doc.encode("utf-8")

shutil.copy(SRC, SRC + ".bak")
with zipfile.ZipFile(SRC, "w", zipfile.ZIP_DEFLATED) as z:
    for n in names:
        z.writestr(n, data[n])

print("removed empty pgNumType:", n_removed)
print("footer patches:", patched)
