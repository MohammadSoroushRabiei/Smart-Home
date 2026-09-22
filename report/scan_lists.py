# -*- coding: utf-8 -*-
"""Scan rendered PDF for figure/table caption pages and regenerate lists.json.
Body display page number = pdf_index - body_start_index + 1 (body restarts at Arabic 1).
"""
import json
import re
import sys
from pypdf import PdfReader

PDF = "render/SmartHome-Project-Report.pdf"
OUT = "lists.json"

FA_DIGITS = "۰۱۲۳۴۵۶۷۸۹"

def to_fa(n: int) -> str:
    return "".join(FA_DIGITS[int(d)] for d in str(n))

def norm_digits(s: str) -> str:
    return "".join(str(FA_DIGITS.index(c)) if c in FA_DIGITS else c for c in s)

FIGURES = [
    "شکل ۱-۱: بلوک دیاگرام کلی سامانه خانه هوشمند مبتنی بر پردازش روی لبه",
    "شکل ۴-۱: لایه‌های نرم‌افزاری سامانه",
    "شکل ۵-۱: معماری کلی سامانه و استقرار لوکال",
    "شکل ۶-۱: خط پردازش تشخیص چهره روی میکروکنترلر",
    "شکل ۶-۲: چرخه فاز سایه، گام ارتقا و یادگیری برخط عامل رفتاری",
    "شکل ۷-۱: چهار کانال موازی تعامل کاربر با سامانه",
    "شکل ۸-۱: معماری سه‌لایه سامانه حضور و غیاب",
]
TABLES = [
    "جدول ۳-۱: نیازمندی‌های عملکردی",
    "جدول ۳-۲: نیازمندی‌های غیرعملکردی",
    "جدول ۴-۱: نگاشت پین‌های سخت‌افزار",
    "جدول ۴-۲: پارتیشن‌بندی فلش",
    "جدول ۴-۳: وظایف FreeRTOS در فریمور",
    "جدول ۵-۱: موضوعات (Topic) اصلی MQTT",
    "جدول ۵-۲: موجودیت‌های کشف‌شده در Home Assistant",
    "جدول ۶-۱: ویژگی‌های ورودی عامل یادگیری رفتاری",
    "جدول ۶-۲: پارامترهای آموزش و عملکرد عامل",
    "جدول ۷-۱: نقطه‌های پایانی اصلی وب‌سرور روی برد",
    "جدول ۱۰-۱: سناریوهای آزمون و نتایج",
]

def key_of(kind: str, caption: str):
    m = re.match(rf"{kind}\s*([۰-۹]+)\s*[-–]\s*([۰-۹]+)", caption.replace(" ", " "))
    if not m:
        return None
    ch, no = norm_digits(m.group(1)), norm_digits(m.group(2))
    return f"{kind}|{ch}-{no}"

KNOWN = {}
for c in FIGURES + TABLES:
    kind = "شکل" if c.startswith("شکل") else "جدول"
    k = key_of(kind, c)
    if k:
        KNOWN[k] = c

r = PdfReader(PDF)
pages_text = [(p.extract_text() or "") for p in r.pages]

# body start: first page holding the actual chapter-1 heading (TOC pages also list
# the heading, so exclude pages carrying the فهرست header)
body_start = None
for i, txt in enumerate(pages_text):
    if ("فصل" in txt and "مقدمه" in txt and "بیان مسئله" in txt and "فهرست" not in txt
            and "عنوان فهرست" not in txt):
        body_start = i
        break
if body_start is None:
    print("ERROR: body start not found", file=sys.stderr)
    sys.exit(1)

found = {}
for i in range(body_start, len(pages_text)):
    for line in pages_text[i].splitlines():
        s = line.strip()
        for kind in ("شکل", "جدول"):
            if not s.startswith(kind):
                continue
            k = key_of(kind, s)
            if not k:
                continue
            ch, no = k.split("|")[1].split("-")
            # extraction can reverse multi-digit chapter numbers (۱۰ -> ۰۱); try both
            cand = [k]
            if len(ch) > 1:
                cand.append(f"{kind}|{ch[::-1]}-{no}")
            if len(no) > 1:
                cand.append(f"{kind}|{ch}-{no[::-1]}")
            if len(ch) > 1 and len(no) > 1:
                cand.append(f"{kind}|{ch[::-1]}-{no[::-1]}")
            for c in cand:
                if c in KNOWN and KNOWN[c] not in found:
                    found[KNOWN[c]] = i - body_start + 1

missing = [c for c in FIGURES + TABLES if c not in found]
if missing:
    print("ERROR: captions not found:", missing, file=sys.stderr)
    sys.exit(1)

data = {
    "figures": {c: to_fa(found[c]) for c in FIGURES},
    "tables": {c: to_fa(found[c]) for c in TABLES},
}
with open(OUT, "w", encoding="utf-8") as f:
    json.dump(data, f, ensure_ascii=False, indent=2)
print(f"body_start={body_start}, pages={len(r.pages)}")
for c in FIGURES + TABLES:
    print(f"  {to_fa(found[c]):>3}  {c}")
print("lists.json written")
