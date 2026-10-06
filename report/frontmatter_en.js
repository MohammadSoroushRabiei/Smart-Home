// English front matter: cover, acknowledgments, abstract, TOC, lists of figures/tables
const fs = require("fs");
const path = require("path");
const { Paragraph, TextRun, ImageRun, TableOfContents, PageBreak, AlignmentType, Table, TableRow, TableCell, WidthType, BorderStyle } = require("docx");
const { t, P, FrontTitle, FONT_EN } = require("./helpers_en");

// ---------- helpers ----------
function centeredEn(text, size, opts = {}) {
  return new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before: opts.before || 0, after: opts.after || 0, line: Math.ceil((size / 2) * 23), lineRule: "atLeast" },
    children: [t(text, { size, bold: !!opts.bold, color: opts.color })],
  });
}

function pageBreakPara() {
  return new Paragraph({ children: [new PageBreak()] });
}

// ---------- 1) Cover ----------
const logoBuf = fs.existsSync(path.join(__dirname, "figs/logo_1.png"))
  ? fs.readFileSync(path.join(__dirname, "figs/logo_1.png")) : null;

const cover = [];
if (logoBuf) {
  const w = logoBuf.readUInt32BE(16), h = logoBuf.readUInt32BE(20);
  const lw = 120, lh = Math.round(lw * h / w);
  cover.push(new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before: 500, after: 160 },
    children: [new ImageRun({ data: logoBuf, transformation: { width: lw, height: lh }, type: "png" })],
  }));
}
cover.push(
  centeredEn("Faculty of Electrical and Computer Engineering", 32, { bold: true, after: 700 }),
  centeredEn("Design and Implementation of a Smart Home IoT System", 34, { bold: true, after: 160 }),
  centeredEn("with On-Device Edge AI Face Authentication", 32, { bold: true, after: 160 }),
  centeredEn("and a Fully Local, Cloud-Free Infrastructure", 32, { bold: true, after: 800 }),
  centeredEn("Undergraduate Specialized Project", 30, { after: 120 }),
  centeredEn("Computer Engineering", 28, { after: 700 }),
  centeredEn("Supervisor:", 28, { bold: true, after: 100 }),
  centeredEn("Dr. Amir Khorsandi", 30, { after: 400 }),
  centeredEn("Student:", 28, { bold: true, after: 100 }),
  centeredEn("Mohammad Soroush Rabiei", 30, { after: 600 }),
  centeredEn("September 2026", 30),
);

// ---------- 2) Acknowledgments ----------
const thanks = [
  new Paragraph({ spacing: { before: 3000 }, children: [] }),
  FrontTitle("Acknowledgments"),
  P("I thank God, the Most Gracious, for granting me the opportunity to learn and to build."),
  P("I am sincerely grateful to my beloved parents, who have always supported me with kindness along the path of learning and effort."),
  new Paragraph({
    alignment: AlignmentType.JUSTIFIED,
    indent: { firstLine: 360 },
    spacing: { line: 360, after: 60 },
    children: [t("I would also like to express my gratitude to my respected supervisor, Dr. Amir Khorsandi, whose invaluable guidance made this project and this report possible."), new PageBreak()],
  }),
];

// ---------- 3) Abstract ----------
const abstractEn = [
  FrontTitle("Abstract"),
  P("In this project, a complete Internet of Things (IoT) smart-home system was designed and implemented with a distinctive focus on bringing artificial intelligence to low-power edge devices and reusing the hardware the user already owns. Face recognition \u2014 the authentication mechanism of the system \u2014 runs entirely on an ESP32-S3 microcontroller: the camera of the user's smartphone, reached by scanning a QR code shown on the device's touch LCD, acts as the image sensor, so no dedicated camera module is required and the edge hardware is minimized."),
  P("The supporting infrastructure is fully local: an MQTT broker (Mosquitto) and the Home Assistant platform run as Docker containers on a home server. The system therefore operates with nothing more than the home modem, survives both international and domestic Internet outages, and even when the global network is available, no cloud service is required \u2014 user data stays inside the home network, preserving privacy and keeping network latency minimal. Four parallel user interfaces are provided: the on-device touch display (LVGL), a mobile web panel opened via QR scan, the Home Assistant web dashboard, and the Home Assistant companion app, all fed by a single source of state on the device."),
  P("Beyond vision, a lightweight behavior-learning agent (two logistic-regression models with offline pre-training at 94.2% / 93.6% validation accuracy and on-chip online SGD) learns user habits for lighting and fan control in a shadow-then-auto lifecycle; the door lock is deliberately never under model control. A three-layer face-based attendance system (device outbox \u2192 local FastAPI/SQLite server \u2192 Jalali-calendar Google Sheets archive and Bale messenger notifications) was also implemented and deployed. Reliability mechanisms \u2014 task watchdogs, a hardware-reset watchdog for the touch controller, and network-aware Wi-Fi/MQTT reconnection \u2014 were validated through end-to-end scenario testing, and the deployed system is documented with photographs and screen captures of the board, its display, the companion apps and the running services (chapter 11)."),
  new Paragraph({
    alignment: AlignmentType.JUSTIFIED,
    spacing: { before: 200, line: 360 },
    children: [
      t("Keywords: ", { size: 24, bold: true }),
      t("Internet of Things, Edge AI, TinyML, ESP32-S3, Face Recognition, MQTT, Home Assistant, Docker, FreeRTOS, LVGL, Privacy, Local-first", { size: 24 }),
      new PageBreak(),
    ],
  }),
];

// ---------- 4) TOC ----------
const tocPage = [
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before: 200, after: 320, line: Math.ceil(18 * 23), lineRule: "atLeast" },
    children: [t("Table of Contents", { size: 36, bold: true })],
  }),
  new TableOfContents("Table of Contents", { hyperlink: true, headingStyleRange: "1-3" }),
  new Paragraph({
    spacing: { before: 200 },
    children: [t("Note: after any edit, right-click the table and choose Update Field to refresh the page numbers.", { size: 20, italics: true, color: "888888" }), new PageBreak()],
  }),
];

// ---------- 5) Lists of figures / tables ----------
const FIGURES = [
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
];
const TABLES = [
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
];

function loadPages() {
  const f = path.join(__dirname, "lists-en.json");
  if (fs.existsSync(f)) { try { return JSON.parse(fs.readFileSync(f, "utf8")); } catch (e) { return {}; } }
  return {};
}

function listTable(entries, pages) {
  const NB = { style: BorderStyle.NONE, size: 0, color: "FFFFFF" };
  const mkRow = (cap) => new TableRow({
    cantSplit: true,
    children: [
      new TableCell({
        width: { size: 86, type: WidthType.PERCENTAGE },
        borders: { top: NB, bottom: NB, left: NB, right: NB },
        margins: { top: 30, bottom: 30, left: 80, right: 80 },
        children: [new Paragraph({ spacing: { line: 340 }, children: [t(cap, { size: 26 })] })],
      }),
      new TableCell({
        width: { size: 14, type: WidthType.PERCENTAGE },
        borders: { top: NB, bottom: NB, left: NB, right: NB },
        margins: { top: 30, bottom: 30, left: 80, right: 80 },
        children: [new Paragraph({ alignment: AlignmentType.CENTER, spacing: { line: 340 }, children: [t(String(pages[cap] || "\u2014"), { size: 26 })] })],
      }),
    ],
  });
  return new Table({
    width: { size: 100, type: WidthType.PERCENTAGE },
    borders: { top: NB, bottom: NB, left: NB, right: NB, insideHorizontal: NB, insideVertical: NB },
    rows: entries.map(mkRow),
  });
}

const pagesMap = loadPages();
const listOfFigures = [
  FrontTitle("List of Figures"),
  listTable(FIGURES, pagesMap.figures || {}),
];
const listOfTables = [
  pageBreakPara(),
  FrontTitle("List of Tables"),
  listTable(TABLES, pagesMap.tables || {}),
];

module.exports = { cover, thanks, abstractEn, tocPage, listOfFigures, listOfTables, FIGURES, TABLES };
