// Shared builders for the English LTR report (Times New Roman)
const {
  Paragraph, TextRun, Table, TableRow, TableCell, ImageRun,
  AlignmentType, HeadingLevel, WidthType, BorderStyle, VerticalAlign,
} = require("docx");
const fs = require("fs");
const path = require("path");

const EN = "Times New Roman";
const FONT_EN = { ascii: EN, hAnsi: EN, cs: EN };

const NB = { style: BorderStyle.NONE, size: 0, color: "FFFFFF" };
const LINE = { style: BorderStyle.SINGLE, size: 6, color: "000000" };

function t(text, opts = {}) {
  return new TextRun({
    text: String(text),
    font: FONT_EN,
    size: opts.size || 24,
    bold: !!opts.bold,
    italics: !!opts.italics,
    color: opts.color || "000000",
  });
}

// Body paragraph (justified, first-line indent)
function P(text, opts = {}) {
  return new Paragraph({
    alignment: AlignmentType.JUSTIFIED,
    indent: { firstLine: opts.noIndent ? 0 : 360 },
    spacing: { line: 360, after: 60 },
    children: [t(text, opts)],
  });
}

// Bullet item
function B(text, level = 0) {
  return new Paragraph({
    alignment: AlignmentType.JUSTIFIED,
    indent: { left: 360 + level * 360, firstLine: 0 },
    spacing: { line: 360, after: 40 },
    children: [t("\u2022  " + text)],
  });
}

// Numbered item (manual numbering written in the text)
function NItem(text) {
  return new Paragraph({
    alignment: AlignmentType.JUSTIFIED,
    indent: { left: 360, firstLine: 0 },
    spacing: { line: 360, after: 40 },
    children: [t(text)],
  });
}

// Headings — H1 centered ("Chapter 1: Introduction"), H2/H3 left-aligned
function H1(text, opts = {}) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_1,
    alignment: AlignmentType.CENTER,
    pageBreakBefore: !!opts.pageBreakBefore,
    spacing: { before: 240, after: 300, line: Math.ceil(18 * 23), lineRule: "atLeast" },
    children: [t(text, { size: 40, bold: true })],
  });
}
function H2(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 300, after: 160, line: Math.ceil(15 * 23), lineRule: "atLeast" },
    children: [t(text, { size: 32, bold: true })],
  });
}
function H3(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_3,
    spacing: { before: 220, after: 120, line: Math.ceil(14 * 23), lineRule: "atLeast" },
    children: [t(text, { size: 30, bold: true })],
  });
}

// Front-matter unnumbered section title (centered, NOT a Heading -> stays out of TOC)
function FrontTitle(text) {
  return new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before: 200, after: 320, line: Math.ceil(18 * 23), lineRule: "atLeast" },
    children: [t(text, { size: 40, bold: true })],
  });
}

// Image dimensions: PNG header (bytes 16..24) or JPEG SOF scan
function imgDims(buf) {
  if (buf[0] === 0x89 && buf[1] === 0x50) return { w: buf.readUInt32BE(16), h: buf.readUInt32BE(20) };
  if (buf[0] === 0xFF && buf[1] === 0xD8) {
    let i = 2;
    while (i < buf.length - 9) {
      if (buf[i] !== 0xFF) { i++; continue; }
      const marker = buf[i + 1];
      if (marker >= 0xC0 && marker <= 0xCF && marker !== 0xC4 && marker !== 0xC8 && marker !== 0xCC) {
        return { h: buf.readUInt16BE(i + 5), w: buf.readUInt16BE(i + 7) };
      }
      i += 2 + buf.readUInt16BE(i + 2);
    }
  }
  return { w: 1000, h: 750 };
}

function imgType(p) {
  const ext = p.toLowerCase().split(".").pop();
  return ext === "gif" ? "gif" : (ext === "jpg" || ext === "jpeg" ? "jpg" : "png");
}

// Figure with caption below. widthPx = display width in px (1px = 0.75pt => ~620px fits 15.9cm)
function FIG(imgPath, caption, widthPx = 600) {
  const buf = fs.readFileSync(path.join(__dirname, imgPath));
  const { w, h } = imgDims(buf);
  const displayH = Math.round(widthPx * h / w);
  return [
    new Paragraph({
      alignment: AlignmentType.CENTER,
      keepNext: true,
      spacing: { before: 160, after: 40 },
      children: [new ImageRun({ data: buf, transformation: { width: widthPx, height: displayH }, type: imgType(imgPath) })],
    }),
    new Paragraph({
      alignment: AlignmentType.CENTER,
      spacing: { after: 200, line: 312 },
      children: [t(caption, { size: 24, bold: true })],
    }),
  ];
}

// Captioned grid of images (3 per row, borderless) — for screenshot galleries
function FIGGRID(caption, images, widthPx = 620) {
  const cols = 3;
  const cellW = Math.floor(widthPx / cols);
  const rows = [];
  for (let i = 0; i < images.length; i += cols) {
    const cells = [];
    for (let j = 0; j < cols; j++) {
      const p = images[i + j];
      if (p) {
        const buf = fs.readFileSync(path.join(__dirname, p));
        const { w, h } = imgDims(buf);
        const dw = Math.floor(cellW * 0.94), dh = Math.round(dw * h / w);
        cells.push(new TableCell({
          width: { size: Math.floor(100 / cols), type: WidthType.PERCENTAGE },
          borders: { top: NB, bottom: NB, left: NB, right: NB },
          margins: { top: 40, bottom: 40, left: 40, right: 40 },
          verticalAlign: VerticalAlign.CENTER,
          children: [new Paragraph({
            alignment: AlignmentType.CENTER,
            children: [new ImageRun({ data: buf, transformation: { width: dw, height: dh }, type: imgType(p) })],
          })],
        }));
      } else {
        cells.push(new TableCell({
          width: { size: Math.floor(100 / cols), type: WidthType.PERCENTAGE },
          borders: { top: NB, bottom: NB, left: NB, right: NB },
          children: [new Paragraph({ children: [] })],
        }));
      }
    }
    rows.push(new TableRow({ cantSplit: true, children: cells }));
  }
  const grid = new Table({
    width: { size: 100, type: WidthType.PERCENTAGE },
    borders: { top: NB, bottom: NB, left: NB, right: NB, insideHorizontal: NB, insideVertical: NB },
    rows,
  });
  const capPara = new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before: 80, after: 200, line: 312 },
    children: [t(caption, { size: 24, bold: true })],
  });
  return [grid, capPara];
}

// Academic three-line LTR table with caption above.
function TBL(caption, headers, rows, widths, opts = {}) {
  const cellSize = opts.cellSize || 24;
  const capPara = new Paragraph({
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
      alignment: opts.align || AlignmentType.CENTER,
      spacing: { line: 300 },
      children: [t(content, { size: cellSize, bold: isHeader })],
    })],
  });

  const table = new Table({
    width: { size: 100, type: WidthType.PERCENTAGE },
    borders: {
      top: LINE, bottom: LINE, left: NB, right: NB,
      insideHorizontal: NB, insideVertical: NB,
    },
    rows: [
      new TableRow({ tableHeader: true, cantSplit: true, children: headers.map((h, i) => mkCell(h, i, true)) }),
      ...rows.map((r) => new TableRow({
        cantSplit: true,
        children: r.map((c, i) => mkCell(c, i, false)),
      })),
    ],
  });

  const after = new Paragraph({ spacing: { after: 120 }, children: [] });
  return [capPara, table, after];
}

module.exports = { EN, FONT_EN, NB, LINE, t, P, B, NItem, H1, H2, H3, FrontTitle, FIG, FIGGRID, TBL };
