// Shared builders for the Persian RTL report (Vazir for Persian, Times New Roman for Latin)
const {
  Paragraph, TextRun, Table, TableRow, TableCell, ImageRun, PageBreak,
  AlignmentType, HeadingLevel, WidthType, BorderStyle, ShadingType, VerticalAlign,
} = require("docx");
const fs = require("fs");
const path = require("path");

const FA = "B Nazanin";
const EN = "Times New Roman";

const FONT_FA = { ascii: EN, hAnsi: EN, cs: FA };
const FONT_EN = { ascii: EN, hAnsi: EN, cs: EN };

const NB = { style: BorderStyle.NONE, size: 0, color: "FFFFFF" };
const LINE = { style: BorderStyle.SINGLE, size: 6, color: "000000" };

function safeText(v, ph) {
  if (v === undefined || v === null || v === "" || String(v) === "NaN" || String(v) === "undefined") {
    return ph || "【تکمیل شود】";
  }
  return String(v);
}

// Persian run (cs font, rtl)
function t(text, opts = {}) {
  return new TextRun({
    text: safeText(text),
    rightToLeft: true,
    font: FONT_FA,
    size: opts.size || 28,
    bold: !!opts.bold,
    italics: !!opts.italics,
    color: opts.color || "000000",
  });
}

// English run (Times New Roman, LTR) — for standalone Latin paragraphs
function en(text, opts = {}) {
  return new TextRun({
    text: safeText(text),
    font: FONT_EN,
    size: opts.size || 24,
    bold: !!opts.bold,
    italics: !!opts.italics,
    color: opts.color || "000000",
  });
}

// Body paragraph (RTL, justified, first-line indent)
function P(text, opts = {}) {
  return new Paragraph({
    bidirectional: true,
    alignment: AlignmentType.JUSTIFIED,
    indent: { firstLine: opts.noIndent ? 0 : 480 },
    spacing: { line: 360, after: 60 },
    children: [t(text, opts)],
  });
}

// Body paragraph built from multiple segments: strings -> Persian runs; {en:"..."} -> Latin runs
function Pm(segs, opts = {}) {
  const runs = segs.map(s => {
    if (typeof s === "object" && s.en !== undefined) return en(s.en, { size: opts.size || 24, bold: s.bold });
    return t(s, { size: opts.size || 28, bold: s.bold });
  });
  return new Paragraph({
    bidirectional: true,
    alignment: AlignmentType.JUSTIFIED,
    indent: { firstLine: opts.noIndent ? 0 : 480 },
    spacing: { line: 360, after: 60 },
    children: runs,
  });
}

// Bullet item (RTL) — manual bullet to keep RTL-safe
function B(text, level = 0) {
  return new Paragraph({
    bidirectional: true,
    alignment: AlignmentType.JUSTIFIED,
    indent: { start: 360 + level * 360, firstLine: 0 },
    spacing: { line: 360, after: 40 },
    children: [t("•  " + text)],
  });
}

// Numbered item (manual Persian numbering written in the text)
function NItem(text) {
  return new Paragraph({
    bidirectional: true,
    alignment: AlignmentType.JUSTIFIED,
    indent: { start: 360, firstLine: 0 },
    spacing: { line: 360, after: 40 },
    children: [t(text)],
  });
}

// Headings — H1 centered ("فصل ۱: مقدمه"), H2/H3 start-aligned (right in RTL)
function H1(text, opts = {}) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_1,
    bidirectional: true,
    alignment: AlignmentType.CENTER,
    pageBreakBefore: !!opts.pageBreakBefore,
    spacing: { before: 240, after: 300, line: Math.ceil(18 * 23), lineRule: "atLeast" },
    children: [t(text, { size: 40, bold: true })],
  });
}
function H2(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_2,
    bidirectional: true,
    spacing: { before: 300, after: 160, line: Math.ceil(15 * 23), lineRule: "atLeast" },
    children: [t(text, { size: 32, bold: true })],
  });
}
function H3(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_3,
    bidirectional: true,
    spacing: { before: 220, after: 120, line: Math.ceil(14 * 23), lineRule: "atLeast" },
    children: [t(text, { size: 30, bold: true })],
  });
}

// Front-matter unnumbered section title (centered, NOT a Heading -> stays out of TOC)
function FrontTitle(text) {
  return new Paragraph({
    bidirectional: true,
    alignment: AlignmentType.CENTER,
    spacing: { before: 200, after: 320, line: Math.ceil(18 * 23), lineRule: "atLeast" },
    children: [t(text, { size: 40, bold: true })],
  });
}

// Figure with caption below. widthPx = display width in px (1px = 0.75pt => ~620px fits 15.9cm)
function FIG(imgPath, caption, widthPx = 600) {
  const buf = fs.readFileSync(path.join(__dirname, imgPath));
  // aspect ratio from PNG header (bytes 16..24)
  const w = buf.readUInt32BE(16), h = buf.readUInt32BE(20);
  const displayH = Math.round(widthPx * h / w);
  return [
    new Paragraph({
      alignment: AlignmentType.CENTER,
      keepNext: true,
      spacing: { before: 160, after: 40 },
      children: [new ImageRun({ data: buf, transformation: { width: widthPx, height: displayH }, type: "png" })],
    }),
    new Paragraph({
      bidirectional: true,
      alignment: AlignmentType.CENTER,
      spacing: { after: 200, line: 312 },
      children: [t(caption, { size: 24, bold: true })],
    }),
  ];
}

// Academic three-line RTL table with caption above.
// headers: array of strings; rows: array of arrays; widths: percentage array (logical order: first = rightmost)
function TBL(caption, headers, rows, widths, opts = {}) {
  const cellSize = opts.cellSize || 24;
  const capPara = new Paragraph({
    bidirectional: true,
    alignment: AlignmentType.CENTER,
    keepNext: true,
    spacing: { before: 200, after: 80, line: 312 },
    children: [t(caption, { size: 24, bold: true })],
  });

  const mkCell = (content, i, isHeader) => new TableCell({
    width: { size: widths[i], type: WidthType.PERCENTAGE },
    borders: isHeader
      ? { top: LINE, bottom: { style: BorderStyle.SINGLE, size: 4, color: "000000" }, left: NB, right: NB }
      : { top: NB, bottom: NB, left: NB, right: NB },
    margins: { top: 40, bottom: 40, left: 80, right: 80 },
    verticalAlign: VerticalAlign.CENTER,
    children: [new Paragraph({
      bidirectional: !(opts.ltrCols || []).includes(i),
      alignment: opts.align || AlignmentType.CENTER,
      spacing: { line: 300 },
      children: [t(content, { size: cellSize, bold: isHeader })],
    })],
  });

  const table = new Table({
    visuallyRightToLeft: true,
    width: { size: 100, type: WidthType.PERCENTAGE },
    borders: {
      top: LINE, bottom: LINE, left: NB, right: NB,
      insideHorizontal: NB, insideVertical: NB,
    },
    rows: [
      new TableRow({ tableHeader: true, cantSplit: true, children: headers.map((h, i) => mkCell(h, i, true)) }),
      ...rows.map((r, ri) => new TableRow({
        cantSplit: true,
        children: r.map((c, i) => mkCell(c, i, false)),
      })),
    ],
  });

  const after = new Paragraph({ spacing: { after: 120 }, children: [] });
  return [capPara, table, after];
}

module.exports = { FA, EN, FONT_FA, FONT_EN, NB, LINE, t, en, P, Pm, B, NItem, H1, H2, H3, FrontTitle, FIG, TBL, safeText };
