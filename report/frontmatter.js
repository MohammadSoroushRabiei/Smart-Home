// Front matter: bismillah, cover, acknowledgments, Persian abstract, TOC, lists of figures/tables
const fs = require("fs");
const path = require("path");
const { Paragraph, TextRun, ImageRun, TableOfContents, PageBreak, AlignmentType, Table, TableRow, TableCell, WidthType, BorderStyle } = require("docx");
const { t, en, P, B, FrontTitle, FONT_FA, FONT_EN } = require("./helpers");

// ---------- helpers ----------
function centeredFa(text, size, opts = {}) {
  return new Paragraph({
    bidirectional: true,
    alignment: AlignmentType.CENTER,
    spacing: { before: opts.before || 0, after: opts.after || 0, line: Math.ceil((size / 2) * 23), lineRule: "atLeast" },
    children: [t(text, { size, bold: !!opts.bold, color: opts.color })],
  });
}

function pageBreakPara() {
  return new Paragraph({ children: [new PageBreak()] });
}

// ---------- 1) بسم الله ----------
const bismillah = [
  new Paragraph({ spacing: { before: 4400 }, children: [] }),
  centeredFa("بسم الله الرحمن الرحیم", 36, { bold: true }),
  new Paragraph({ children: [new PageBreak()] }),
];

// ---------- 2) Cover ----------
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
  centeredFa("دانشکده مهندسی برق و کامپیوتر", 32, { bold: true, after: 700 }),
  centeredFa("طراحی و پیاده‌سازی سامانه خانه هوشمند", 36, { bold: true, after: 160 }),
  centeredFa("مبتنی بر اینترنت اشیا با پردازش هوش مصنوعی روی لبه", 32, { bold: true, after: 160 }),
  centeredFa("و زیرساخت کاملاً لوکال بدون نیاز به فضای ابری", 32, { bold: true, after: 800 }),
  centeredFa("پروژه تخصصی کارشناسی", 30, { after: 120 }),
  centeredFa("رشته مهندسی کامپیوتر", 28, { after: 700 }),
  centeredFa("استاد راهنما:", 28, { bold: true, after: 100 }),
  centeredFa("دکتر امیر خورسندی", 30, { after: 400 }),
  centeredFa("دانشجو:", 28, { bold: true, after: 100 }),
  centeredFa("محمد سروش ربیعی", 30, { after: 600 }),
  centeredFa("شهریور ۱۴۰۵", 30),
);

// ---------- 3) قدردانی ----------
const thanks = [
  new Paragraph({ spacing: { before: 3000 }, children: [] }),
  FrontTitle("تشکر و قدردانی"),
  P("سپاس بی‌کران پروردگار را که فرصت آموختن و ساختن به من عطا کرد."),
  P("از پدر و مادر عزیزم که همواره در مسیر علم و تلاش، پشتیبان و مهربانم بوده‌اند، صمیمانه سپاسگزارم."),
  new Paragraph({
    bidirectional: true,
    alignment: AlignmentType.JUSTIFIED,
    indent: { firstLine: 480 },
    spacing: { line: 360, after: 60 },
    children: [t("از استاد راهنمای محترم، جناب آقای دکتر امیر خورسندی، که با راهنمایی‌های ارزشمند خود در طول انجام این پروژه، به تحقق این گزارش یاری رساندند، متشکرم."), new PageBreak()],
  }),
];

// ---------- 4) چکیده ----------
const abstractFa = [
  FrontTitle("چکیده"),
  P("در این پروژه یک سامانه خانه هوشمند مبتنی بر اینترنت اشیا طراحی و پیاده‌سازی شد که تمرکز اصلی آن، اجرای هوش مصنوعی روی دستگاه‌های کم‌توان لبه و به‌کارگیری امکانات موجود کاربر برای احراز هویت است. سازوکار احراز هویت سامانه، تشخیص چهره است که به‌طور کامل روی میکروکنترلر ESP32-S3 اجرا می‌شود: سنسور دوربین، دوربین گوشی هوشمند خود کاربر است که با اسکن کد QR روی نمایشگر لمسی برد در دسترس قرار می‌گیرد و در نتیجه نیازی به ماژول دوربین اختصاصی نیست و سخت‌افزار لبه به حداقل می‌رسد."),
  P("زیرساخت پشتیبان سامانه کاملاً لوکال است: کارگزار MQTT (Mosquitto) و پلتفرم Home Assistant به‌صورت کانتینر Docker روی سرور خانگی اجرا می‌شوند. به همین دلیل سامانه تنها با روشن بودن مودم خانگی کار می‌کند، در شرایط قطعی اینترنت بین‌المللی و حتی داخلی بدون وقفه به فعالیت خود ادامه می‌دهد و با وصل بودن اینترنت نیز به هیچ سرویس ابری وابسته نیست؛ داده‌های کاربر در شبکه محلی باقی می‌ماند که علاوه بر افزایش حریم خصوصی، تاخیر شبکه را نیز به حداقل می‌رساند. تعامل کاربر در چهار کانال موازی فراهم شده است: نمایشگر لمسی رویبرد، پنل وب موبایل با ورود از طریق QR، داشبورد وب Home Assistant و اپ موبایل Home Assistant، که همگی از یک منبع واحد وضعیت تغذیه می‌شوند."),
  P("در بخش هوش مصنوعی، خط پردازش کامل تشخیص چهره (رمزگشایی JPEG سخت‌افزاری، آشکارسازی صورت، استخراج بردار ویژگی با ESP-DL و تطبیق با آستانه ۰٫۷۰) روی میکروکنترلر پیاده‌سازی شد و ثبت چندنمونه‌ای چهره، دقت تطبیق را در شرایط نوری و زاویه‌ای مختلف بهبود داد. همچنین یک عامل یادگیری رفتاری سبک با دو مدل رگرسیون لجستیک (دقت اعتبارسنجی ۹۴٫۲ و ۹۳٫۶ درصد) با ترکیب پیش‌آموزش آفلاین و یادگیری برخط روی تراشه، الگوی کاربر را در کنترل روشنایی و فن یاد می‌گیرد و در چرخه سایه→خودکار عمل می‌کند؛ قفل در به‌طور قطعی خارج از کنترل مدل است. بر بستر همان موتور چهره، سامانه حضور و غیاب سه‌لایه‌ای با صف پایا روی برد، سرور FastAPI و SQLite، آرشیو Google Sheets با تقویم شمسی و اعلان‌های پیام‌رسان بله استقرار شد."),
  P("نتایج آزمون‌های انتهابه‌انتها نشان داد سامانه در قطعی اینترنت به‌طور کامل کار می‌کند، چرخه تصمیم چهره داخل شبکه محلی باقی می‌ماند و سازوکارهای پایداری (سگ‌نگهبان‌ها، ریست سخت‌افزاری لمس و اتصال مجدد شبکه‌آگاه) کارکرد طولانی‌مدت بدون ریست غیرعمدی را تضمین می‌کنند. معماری ماژولار حاصل، بستری آماده برای توسعه‌های آینده از جمله یادگیری فدرال روی میکروکنترلرها فراهم کرده است."),
  new Paragraph({
    bidirectional: true,
    alignment: AlignmentType.JUSTIFIED,
    spacing: { before: 200, line: 360 },
    children: [
      t("واژگان کلیدی: ", { size: 26, bold: true }),
      t("اینترنت اشیا، هوش مصنوعی روی لبه، تشخیص چهره روی میکروکنترلر، ESP32-S3، حریم خصوصی، زیرساخت لوکال، MQTT، Home Assistant، Docker، یادگیری رفتاری", { size: 26 }),
      new PageBreak(),
    ],
  }),
];

// ---------- 5) TOC ----------
const tocPage = [
  new Paragraph({
    bidirectional: true,
    alignment: AlignmentType.CENTER,
    spacing: { before: 200, after: 320, line: Math.ceil(18 * 23), lineRule: "atLeast" },
    children: [t("فهرست مطالب", { size: 36, bold: true })],
  }),
  new TableOfContents("Table of Contents", { hyperlink: true, headingStyleRange: "1-3" }),
  new Paragraph({
    bidirectional: true,
    spacing: { before: 200 },
    children: [t("توجه: برای به‌روزرسانی شماره صفحه‌ها پس از هر ویرایش، روی فهرست راست‌کلیک کرده و گزینه Update Field را انتخاب کنید.", { size: 20, italics: true, color: "888888" }), new PageBreak()],
  }),
];

// ---------- 6) فهرست اشکال / جداول ----------
const FIGURES = [
  "شکل ۴-۱: لایه‌های نرم‌افزاری سامانه",
  "شکل ۵-۱: معماری کلی سامانه و استقرار لوکال",
  "شکل ۶-۱: خط پردازش تشخیص چهره روی میکروکنترلر",
  "شکل ۶-۲: چرخه فاز سایه، گام ارتقا و یادگیری برخط عامل رفتاری",
  "شکل ۷-۱: چهار کانال موازی تعامل کاربر با سامانه",
  "شکل ۸-۱: معماری سه‌لایه سامانه حضور و غیاب",
];
const TABLES = [
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
];

function loadPages() {
  const f = path.join(__dirname, "lists.json");
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
        children: [new Paragraph({ bidirectional: true, spacing: { line: 340 }, children: [t(cap, { size: 26 })] })],
      }),
      new TableCell({
        width: { size: 14, type: WidthType.PERCENTAGE },
        borders: { top: NB, bottom: NB, left: NB, right: NB },
        margins: { top: 30, bottom: 30, left: 80, right: 80 },
        children: [new Paragraph({ bidirectional: true, alignment: AlignmentType.CENTER, spacing: { line: 340 }, children: [t(pages[cap] || "—", { size: 26 })] })],
      }),
    ],
  });
  return new Table({
    visuallyRightToLeft: true,
    width: { size: 100, type: WidthType.PERCENTAGE },
    borders: { top: NB, bottom: NB, left: NB, right: NB, insideHorizontal: NB, insideVertical: NB },
    rows: entries.map(mkRow),
  });
}

const pagesMap = loadPages();
const listOfFigures = [
  FrontTitle("فهرست اشکال"),
  listTable(FIGURES, pagesMap.figures || {}),
];
const listOfTables = [
  pageBreakPara(),
  FrontTitle("فهرست جداول"),
  listTable(TABLES, pagesMap.tables || {}),
];

module.exports = { bismillah, cover, thanks, abstractFa, tocPage, listOfFigures, listOfTables, FIGURES, TABLES };
