// Assemble the full report
const {
  Document, Packer, Paragraph, TextRun, Footer, PageNumber, NumberFormat,
  SectionType, AlignmentType, PageOrientation,
} = require("docx");
const fs = require("fs");

const { FONT_FA, FONT_EN } = require("./helpers");
const { ch1, ch2, ch3 } = require("./content1");
const { ch4, ch5, ch6 } = require("./content2");
const { ch7, ch8, ch9 } = require("./content3");
const { ch10, ch11, refs, abstractEN, titleEN } = require("./content4");
const { bismillah, cover, thanks, abstractFa, tocPage, listOfFigures, listOfTables } = require("./frontmatter");

const MARGIN = { top: 1440, bottom: 1440, left: 1417, right: 1701, header: 850, footer: 992 }; // binding edge = right (RTL)
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
        run: { font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "B Nazanin" }, size: 28, color: "000000" },
        paragraph: { spacing: { line: 360 } },
      },
      heading1: {
        run: { font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "B Nazanin" }, size: 40, bold: true, color: "000000" },
        paragraph: { alignment: AlignmentType.CENTER, spacing: { before: 240, after: 300, line: 460 } },
      },
      heading2: {
        run: { font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "B Nazanin" }, size: 32, bold: true, color: "000000" },
        paragraph: { spacing: { before: 300, after: 160, line: 380 } },
      },
      heading3: {
        run: { font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "B Nazanin" }, size: 30, bold: true, color: "000000" },
        paragraph: { spacing: { before: 220, after: 120, line: 360 } },
      },
    },
    paragraphStyles: [
      { id: "TOC1", name: "toc 1", basedOn: "Normal", next: "Normal",
        run: { font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "B Nazanin" }, size: 28, color: "000000" },
        paragraph: { bidirectional: true, spacing: { line: 360, after: 60 } } },
      { id: "TOC2", name: "toc 2", basedOn: "Normal", next: "Normal",
        run: { font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "B Nazanin" }, size: 28, color: "000000" },
        paragraph: { bidirectional: true, indent: { start: 300 }, spacing: { line: 360, after: 40 } } },
      { id: "TOC3", name: "toc 3", basedOn: "Normal", next: "Normal",
        run: { font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "B Nazanin" }, size: 24, color: "000000" },
        paragraph: { bidirectional: true, indent: { start: 600 }, spacing: { line: 340, after: 40 } } },
    ],
  },
  sections: [
    // S1: bismillah + cover (no footer/page number)
    {
      properties: { page: PAGE },
      children: [...bismillah, ...cover],
    },
    // S2: front matter (Roman page numbers)
    {
      properties: {
        type: SectionType.NEXT_PAGE,
        page: { ...PAGE, pageNumbers: { start: 1, formatType: NumberFormat.UPPER_ROMAN } },
      },
      footers: { default: pageNumFooter() },
      children: [...thanks, ...abstractFa, ...tocPage, ...listOfFigures, ...listOfTables],
    },
    // S3: body (Arabic page numbers from 1)
    {
      properties: {
        type: SectionType.NEXT_PAGE,
        page: { ...PAGE, pageNumbers: { start: 1, formatType: NumberFormat.DECIMAL } },
      },
      footers: { default: pageNumFooter() },
      children: [...ch1, ...ch2, ...ch3, ...ch4, ...ch5, ...ch6, ...ch7, ...ch8, ...ch9, ...ch10, ...ch11, ...refs],
    },
    // S4: English abstract (continues numbering)
    {
      properties: { type: SectionType.NEXT_PAGE, page: PAGE },
      footers: { default: pageNumFooter() },
      children: abstractEN,
    },
    // S5: English title page
    {
      properties: { type: SectionType.NEXT_PAGE, page: PAGE },
      footers: { default: pageNumFooter() },
      children: titleEN,
    },
  ],
});

Packer.toBuffer(doc).then(buf => {
  fs.writeFileSync("SmartHome-Project-Report.docx", buf);
  console.log("docx written:", buf.length, "bytes");
});
