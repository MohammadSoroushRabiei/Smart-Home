// Assemble the full English report
const {
  Document, Packer, Paragraph, TextRun, Footer, PageNumber, NumberFormat,
  SectionType, AlignmentType,
} = require("docx");
const fs = require("fs");

const { FONT_EN } = require("./helpers_en");
const { ch1, ch2, ch3 } = require("./content-en1");
const { ch4, ch5, ch6 } = require("./content-en2");
const { ch7, ch8, ch9 } = require("./content-en3");
const { ch10, ch11real, ch12, refs } = require("./content-en4");
const { cover, thanks, abstractEn, tocPage, listOfFigures, listOfTables } = require("./frontmatter_en");

const MARGIN = { top: 1440, bottom: 1440, left: 1701, right: 1417, header: 850, footer: 992 }; // binding edge = left (LTR)
const PAGE = { size: { width: 11906, height: 16838 }, margin: MARGIN };

function pageNumFooter() {
  return new Footer({
    children: [new Paragraph({
      alignment: AlignmentType.CENTER,
      children: [new TextRun({ children: [PageNumber.CURRENT], size: 21, font: FONT_EN })],
    })],
  });
}

const doc = new Document({
  features: { updateFields: true },
  styles: {
    default: {
      document: {
        run: { font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "Times New Roman" }, size: 24, color: "000000" },
        paragraph: { spacing: { line: 360 } },
      },
      heading1: {
        run: { font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "Times New Roman" }, size: 40, bold: true, color: "000000" },
        paragraph: { alignment: AlignmentType.CENTER, spacing: { before: 240, after: 300, line: 460 } },
      },
      heading2: {
        run: { font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "Times New Roman" }, size: 32, bold: true, color: "000000" },
        paragraph: { spacing: { before: 300, after: 160, line: 380 } },
      },
      heading3: {
        run: { font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "Times New Roman" }, size: 30, bold: true, color: "000000" },
        paragraph: { spacing: { before: 220, after: 120, line: 360 } },
      },
    },
    paragraphStyles: [
      { id: "TOC1", name: "toc 1", basedOn: "Normal", next: "Normal",
        run: { font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "Times New Roman" }, size: 26, color: "000000" },
        paragraph: { spacing: { line: 360, after: 60 } } },
      { id: "TOC2", name: "toc 2", basedOn: "Normal", next: "Normal",
        run: { font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "Times New Roman" }, size: 26, color: "000000" },
        paragraph: { indent: { start: 300 }, spacing: { line: 360, after: 40 } } },
      { id: "TOC3", name: "toc 3", basedOn: "Normal", next: "Normal",
        run: { font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "Times New Roman" }, size: 24, color: "000000" },
        paragraph: { indent: { start: 600 }, spacing: { line: 340, after: 40 } } },
    ],
  },
  sections: [
    // S1: cover (no footer/page number)
    {
      properties: { page: PAGE },
      children: [...cover],
    },
    // S2: front matter (Roman page numbers)
    {
      properties: {
        type: SectionType.NEXT_PAGE,
        page: { ...PAGE, pageNumbers: { start: 1, formatType: NumberFormat.UPPER_ROMAN } },
      },
      footers: { default: pageNumFooter() },
      children: [...thanks, ...abstractEn, ...tocPage, ...listOfFigures, ...listOfTables],
    },
    // S3: body (Arabic page numbers from 1)
    {
      properties: {
        type: SectionType.NEXT_PAGE,
        page: { ...PAGE, pageNumbers: { start: 1, formatType: NumberFormat.DECIMAL } },
      },
      footers: { default: pageNumFooter() },
      children: [...ch1, ...ch2, ...ch3, ...ch4, ...ch5, ...ch6, ...ch7, ...ch8, ...ch9, ...ch10, ...ch11real, ...ch12, ...refs],
    },
  ],
});

Packer.toBuffer(doc).then(buf => {
  fs.writeFileSync("SmartHome-Project-Report-EN.docx", buf);
  console.log("docx written:", buf.length, "bytes");
});
