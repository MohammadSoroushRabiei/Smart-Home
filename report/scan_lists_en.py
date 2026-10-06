# -*- coding: utf-8 -*-
"""Scan the rendered English PDF for figure/table caption pages and regenerate lists-en.json.
Body display page number = pdf_index - body_start_index + 1 (body restarts at Arabic 1).
"""
import json
import re
import sys
from pypdf import PdfReader

PDF = "render/SmartHome-Project-Report-EN.pdf"
OUT = "lists-en.json"

FIGURES = [
    "Figure 1-1: Overall block diagram of the edge-processing smart-home system",
    "Figure 4-1: Software layers of the system",
    "Figure 5-1: Overall system architecture and local deployment",
    "Figure 6-1: The face-recognition pipeline on the microcontroller",
    "Figure 6-2: The shadow phase, promotion gate and online learning of the behavioral agent",
    "Figure 7-1: The four parallel user-interaction channels of the system",
    "Figure 8-1: The three-tier architecture of the attendance system",
    "Figure 11-1: The deployed system on the workbench \u2014 board, display and companion app",
    "Figure 11-2: The nine LVGL interface pages \u2014 screenshots from the board's real display",
    "Figure 11-3: The system's web dashboard in its mobile view",
    "Figure 11-4: The Home Assistant companion-app dashboard on a phone",
    "Figure 11-5: The Bale bot \u2014 device control and event notifications",
    "Figure 11-6: The system's running containers in Docker Desktop",
    "Figure 11-7: The attendance archive in Google Sheets with Jalali dates",
]
TABLES = [
    "Table 3-1: Functional requirements",
    "Table 3-2: Non-functional requirements",
    "Table 4-1: Hardware pin mapping",
    "Table 4-2: Flash partitioning",
    "Table 4-3: FreeRTOS tasks in the firmware",
    "Table 5-1: Main MQTT topics",
    "Table 5-2: Entities discovered in Home Assistant",
    "Table 6-1: Input features of the behavioral learning agent",
    "Table 6-2: Training parameters and agent performance",
    "Table 7-1: Main web-server endpoints on the board",
    "Table 10-1: Test scenarios and results",
]

def key_of(kind: str, caption: str):
    m = re.match(rf"{kind}\s*(\d+)\s*[-\u2013]\s*(\d+)", caption)
    if not m:
        return None
    return f"{kind}|{m.group(1)}-{m.group(2)}"

KNOWN = {}
for c in FIGURES + TABLES:
    kind = "Figure" if c.startswith("Figure") else "Table"
    k = key_of(kind, c)
    if k:
        KNOWN[k] = c

r = PdfReader(PDF)
pages_text = [(p.extract_text() or "") for p in r.pages]

# body start: the page with the opening sentence of chapter 1 (unique body text)
body_start = None
for i, txt in enumerate(pages_text):
    if "spread rapidly in recent years" in txt:
        body_start = i
        break
if body_start is None:
    print("ERROR: body start not found", file=sys.stderr)
    sys.exit(1)

found = {}
for i in range(body_start, len(pages_text)):
    flat = " ".join(pages_text[i].split())
    for c in FIGURES + TABLES:
        if c not in found and c in flat:
            found[c] = i - body_start + 1

missing = [c for c in FIGURES + TABLES if c not in found]
if missing:
    print("ERROR: captions not found:", missing, file=sys.stderr)
    sys.exit(1)

data = {
    "figures": {c: str(found[c]) for c in FIGURES},
    "tables": {c: str(found[c]) for c in TABLES},
}
with open(OUT, "w", encoding="utf-8") as f:
    json.dump(data, f, ensure_ascii=False, indent=2)
print(f"body_start={body_start}, pages={len(r.pages)}")
for c in FIGURES + TABLES:
    print(f"  {found[c]:>3}  {c}")
print("lists-en.json written")
