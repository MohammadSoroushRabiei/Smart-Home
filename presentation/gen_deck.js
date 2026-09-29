// Smart-Home defense deck — Persian RTL, 13.33x7.5, Pinar Black / Sahel
const pptxgen = require("pptxgenjs");

const W = 13.33, H = 7.5, M = 0.5;
const BG_DARK = "16283F", BG = "FFFFFF", SURFACE = "F2F6FA", SURF2 = "EAF0F7";
const PRIMARY = "1E3A5F", PRIMARY_MID = "2F5D8C", ACCENT = "C9962E";
const TEXT = "1A2433", MUTED = "5B6B7E", HAIR = "D8E0EA", LIGHT_ON_DARK = "C7D4E3";
const TF = "Pinar Black", BF = "Sahel";
const FIGS = "D:/Project/Smart-Home/report/figs";

const pres = new pptxgen();
pres.layout = "LAYOUT_WIDE";
pres.rtlMode = true;
pres.author = "Mohammad Soroush Rabiei";
pres.title = "طراحی و پیاده‌سازی سامانه خانه هوشمند مبتنی بر اینترنت اشیا با هوش مصنوعی روی لبه";
pres.theme = { headFontFace: TF, bodyFontFace: BF };

const TOTAL = 25;
const sh = () => ({ type: "outer", color: "1E3A5F", blur: 7, offset: 2, angle: 90, opacity: 0.14 });

function T(s, txt, o = {}) {
  s.addText(txt, { fontFace: BF, color: TEXT, rtlMode: true, align: "right", margin: 0, ...o });
}
function card(s, x, y, w, h, fill = SURFACE, shadow = false, line = null) {
  s.addShape("roundRect", { x, y, w, h, fill: { color: fill }, rectRadius: 0.07,
    line: line ? { color: line, width: 0.75 } : { color: fill, width: 0 },
    ...(shadow ? { shadow: sh() } : {}) });
}
function imgCard(s, path, x, y, w, h) {
  s.addShape("rect", { x: x - 0.08, y: y - 0.08, w: w + 0.16, h: h + 0.16, fill: { color: "FFFFFF" }, line: { color: HAIR, width: 1 }, shadow: sh() });
  s.addImage({ path, x, y, w, h });
}
function header(s, kicker, title, titleSize = 29) {
  T(s, kicker, { x: M, y: 0.38, w: W - 2 * M, h: 0.34, fontSize: 13, color: ACCENT, bold: true });
  T(s, title, { x: M, y: 0.72, w: W - 2 * M, h: 0.78, fontSize: titleSize, fontFace: TF, color: PRIMARY });
}
function pageNum(s, n) {
  s.addText(`${fa(n)} / ${fa(TOTAL)}`, { x: 0.45, y: 7.08, w: 1.2, h: 0.32, fontSize: 12, fontFace: BF, color: MUTED, align: "left", margin: 0 });
}
function srcNote(s, txt, x, y, w) {
  T(s, txt, { x, y, w, h: 0.3, fontSize: 12, color: MUTED });
}
// numbered row: gold numeral on the right, text to its left
function numRow(s, n, title, desc, x, y, w, opt = {}) {
  const numW = 0.62, gap = 0.18;
  T(s, fa(n), { x: x + w - numW, y: y - 0.02, w: numW, h: 0.55, fontSize: 21, fontFace: TF, color: ACCENT, align: "center" });
  const tw = w - numW - gap;
  if (desc) {
    T(s, title, { x, y, w: tw, h: 0.4, fontSize: opt.ts || 15.5, bold: true, color: PRIMARY });
    T(s, desc, { x, y: y + (opt.dy || 0.38), w: tw, h: opt.dh || 0.62, fontSize: opt.ds || 13, color: MUTED, lineSpacingMultiple: 1.12 });
  } else {
    T(s, title, { x, y, w: tw, h: opt.th || 0.7, fontSize: opt.ts || 15, lineSpacingMultiple: 1.15 });
  }
}
const fa = (n) => String(n).replace(/\d/g, d => "۰۱۲۳۴۵۶۷۸۹"[d]);

/* ---------------- S1 — Cover ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG_DARK };
  s.addImage({ path: `${FIGS}/logo_1.png`, x: W / 2 - 0.62, y: 0.5, w: 1.24, h: 1.24 });
  T(s, "دانشگاه صنعتی اصفهان", { x: 1, y: 1.92, w: W - 2, h: 0.42, fontSize: 17, bold: true, color: ACCENT, align: "center" });
  T(s, "دانشکده مهندسی برق و کامپیوتر", { x: 1, y: 2.32, w: W - 2, h: 0.36, fontSize: 14, color: LIGHT_ON_DARK, align: "center" });
  T(s, "طراحی و پیاده‌سازی سامانه خانه هوشمند", { x: 0.7, y: 2.95, w: W - 1.4, h: 0.85, fontSize: 37, fontFace: TF, color: "FFFFFF", align: "center" });
  T(s, "مبتنی بر اینترنت اشیا با پردازش هوش مصنوعی روی لبه و زیرساخت کاملاً لوکال بدون نیاز به فضای ابری", { x: 1.7, y: 3.85, w: W - 3.4, h: 0.75, fontSize: 19, color: LIGHT_ON_DARK, align: "center", lineSpacingMultiple: 1.2 });
  s.addShape("line", { x: W / 2 - 1.1, y: 4.85, w: 2.2, h: 0, line: { color: ACCENT, width: 1 } });
  T(s, "پروژه تخصصی کارشناسی — رشته مهندسی کامپیوتر", { x: 1, y: 5.05, w: W - 2, h: 0.36, fontSize: 14, color: LIGHT_ON_DARK, align: "center" });
  T(s, "استاد راهنما: دکتر امیر خورسندی", { x: 1, y: 5.5, w: W - 2, h: 0.4, fontSize: 16, bold: true, color: "FFFFFF", align: "center" });
  T(s, "دانشجو: محمد سروش ربیعی", { x: 1, y: 5.95, w: W - 2, h: 0.4, fontSize: 16, bold: true, color: "FFFFFF", align: "center" });
  T(s, "مهر ۱۴۰۵", { x: 1, y: 6.55, w: W - 2, h: 0.35, fontSize: 13, color: "8FA3BC", align: "center" });
  s.addNotes("سلام و احترام. عنوان پروژه: سامانه خانه هوشمند مبتنی بر IoT با هوش مصنوعی روی لبه و زیرساخت کاملاً لوکال. ارائه در ~۲۰ دقیقه؛ بخش اصلی: هوش مصنوعی روی لبه.");
}

/* ---------------- S2 — Agenda ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "نقشه ارائه", "فهرست مطالب");
  const items = [
    ["بیان مسئله و اهداف", "چرا خانه هوشمند بدون ابر؟"],
    ["معماری، سخت‌افزار و فریمور", "ESP32-S3، FreeRTOS، نمایشگر لمسی"],
    ["زیرساخت لوکال", "MQTT و Home Assistant با Docker"],
    ["هوش مصنوعی روی لبه", "تشخیص چهره + عامل یادگیری رفتاری (بخش اصلی)"],
    ["رابط‌های کاربری چندکاناله", "نمایشگر، وب موبایل، داشبورد و اپ HA"],
    ["حضور و غیاب مبتنی بر چهره", "معماری سه‌لایه با صف پایا"],
    ["امنیت و حریم خصوصی", "TLS، توکن‌ها، قواعد سخت قفل"],
    ["آزمون‌ها، نتایج و جمع‌بندی", "۱۴ سناریوی انتهابه‌انتها + کارهای آینده"],
  ];
  const colW = 6.05, rowH = 1.18, y0 = 1.85;
  items.forEach(([t, d], i) => {
    const col = i < 4 ? 1 : 0; // right column first
    const x = col ? W - M - colW : M;
    const y = y0 + (i % 4) * rowH;
    numRow(s, i + 1, t, d, x, y, colW, { dy: 0.37, dh: 0.4, ds: 12.5 });
    if (i % 4 < 3) s.addShape("line", { x, y: y + rowH - 0.18, w: colW, h: 0, line: { color: HAIR, width: 0.75 } });
  });
  pageNum(s, 2);
  s.addNotes("مسیر ارائه: از مسئله تا جمع‌بندی. تأکید: بخش ۴ (هوش مصنوعی روی لبه) مفصل‌ترین بخش است.");
}

/* ---------------- S3 — Problem ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "فصل ۱ — مقدمه", "بیان مسئله: سه آسیبِ معماری ابرمحور");
  T(s, "راهکارهای تجاری رایج بر دو ستون تکیه دارند: سخت‌افزار اختصاصی متعدد و سرویس‌های ابری برای پردازش، احراز هویت و ذخیره‌سازی.", { x: M, y: 1.62, w: W - 2 * M, h: 0.45, fontSize: 14.5, color: MUTED, lineSpacingMultiple: 1.15 });
  const probs = [
    ["حریم خصوصی", "داده‌های حساس مانند تصویر چهره ساکنان از خانه خارج و روی سرورهای غیرقابل‌کنترل نگهداری می‌شود."],
    ["وابستگی به اینترنت", "هر قطعی اینترنت به معنای از دست رفتن کنترل و هوشمندی خانه است."],
    ["تاخیر شبکه", "چرخه ابر برای تصمیم‌های حساس مانند بازکردن قفل با چهره قابل‌قبول نیست."],
  ];
  const cw = 3.94, gap = 0.25, y = 2.35, ch = 2.15;
  probs.forEach(([t, d], i) => {
    const x = W - M - cw - i * (cw + gap);
    card(s, x, y, cw, ch, SURFACE, true);
    T(s, t, { x: x + 0.25, y: y + 0.28, w: cw - 0.5, h: 0.45, fontSize: 18, bold: true, color: PRIMARY });
    T(s, d, { x: x + 0.25, y: y + 0.85, w: cw - 0.5, h: ch - 1.05, fontSize: 13.5, color: TEXT, lineSpacingMultiple: 1.25 });
  });
  T(s, "علاوه بر این، بینایی ماشین به‌طور سنتی نیازمند پردازنده قدرتمند و دوربین اختصاصی است؛ هزینه، پیچیدگی نصب و مصرف انرژی بالا می‌رود.", { x: M, y: 4.75, w: W - 2 * M, h: 0.45, fontSize: 14.5, color: MUTED, lineSpacingMultiple: 1.15 });
  card(s, M, 5.45, W - 2 * M, 1.35, SURF2);
  T(s, "پرسش اصلی پروژه", { x: M + 0.2, y: 5.62, w: W - 2 * M - 0.4, h: 0.35, fontSize: 14, bold: true, color: ACCENT, align: "center" });
  T(s, "آیا می‌توان خانه هوشمند کامل، امن و هوشمند را با حداقل سخت‌افزار لبه ساخت — با اجرای کامل هوش مصنوعی روی خود میکروکنترلر، دوربین قرضی از گوشی کاربر، و بدون خروج هیچ داده‌ای از شبکه خانگی؟", { x: M + 0.35, y: 5.98, w: W - 2 * M - 0.7, h: 0.75, fontSize: 15.5, bold: true, color: PRIMARY, align: "center", lineSpacingMultiple: 1.25 });
  pageNum(s, 3);
  s.addNotes("سه مشکل ابر: حریم خصوصی، قطعی، تاخیر. + هزینه سخت‌افزار بینایی ماشین. سپس پرسش اصلی پروژه.");
}

/* ---------------- S4 — Solution + block diagram ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "فصل ۱ — مقدمه", "پاسخ پروژه: همه هوشمندی روی لبه، همه داده در خانه");
  imgCard(s, `${FIGS}/fig7_block_diagram.png`, 0.65, 1.95, 6.45, 4.5);
  srcNote(s, "منبع: گزارش پروژه — شکل ۱-۱", 0.65, 6.62, 4);
  const rows = [
    ["تشخیص چهره کامل روی ESP32-S3", "از رمزگشایی JPEG تا تطبیق بردار ویژگی — تصویر هرگز از برد خارج نمی‌شود."],
    ["گوشی کاربر = سنسور و رابط", "دوربین و مرورگر گوشی از طریق QR روی نمایشگر؛ بدون ماژول دوربین اختصاصی."],
    ["زیرساخت لوکال با Docker", "Mosquitto و Home Assistant روی سرور خانگی؛ فقط مودم، بدون سرویس ابری."],
    ["چهار کانال تعامل موازی", "نمایشگر لمسی، وب موبایل، داشبورد HA و اپ HA — با یک منبع واحد وضعیت."],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, 7.5, 2.0 + i * 1.12, 5.35, { dy: 0.37, dh: 0.55, ds: 12.5 }));
  pageNum(s, 4);
  s.addNotes("بلوک دیاگرام: گوشی فقط دوربین را قرض می‌دهد؛ همه پردازش روی ESP32-S3؛ سرور خانگی Docker؛ اینترنت اختیاری فقط برای آرشیو/اعلان.");
}

/* ---------------- S5 — Goals ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "فصل ۱ — مقدمه", "اهداف پروژه");
  const goals = [
    ["گره IoT با ESP32-S3", "نمایشگر لمسی رنگی، وب‌سرور امن HTTPS داخلی و اتصال پایدار به شبکه خانگی"],
    ["چهره کامل روی میکروکنترلر", "رمزگشایی، آشکارسازی، استخراج بردار ویژگی و تطبیق — بدون ارسال تصویر به بیرون"],
    ["دوربین قرضی از گوشی کاربر", "ورودی تصویری با صفحه وب موبایل و مکانیزم QR به‌جای ماژول دوربین اختصاصی"],
    ["زیرساخت لوکال با کشف خودکار", "Mosquitto + Home Assistant با Docker و یکپارچه‌سازی Discovery"],
    ["رابط کاربری چندکاناله", "چهار کانال موازی، همه با یک منبع واحد وضعیت"],
    ["عامل یادگیری رفتاری سبک", "کنترل روشنایی و فن با پیش‌آموزش آفلاین + یادگیری برخط روی تراشه"],
    ["حضور و غیاب مبتنی بر چهره", "سرور لوکال، صف پیام پایا، آرشیو شمسی و اطلاع‌رسانی بله"],
    ["پایداری و آزمون", "سگ‌نگهبان، اتصال مجدد شبکه‌آگاه و آزمون سناریومحور انتهابه‌انتها"],
  ];
  const colW = 6.05, rowH = 1.22, y0 = 1.8;
  goals.forEach(([t, d], i) => {
    const col = i % 2 === 0 ? 1 : 0;
    const x = col ? W - M - colW : M;
    const y = y0 + Math.floor(i / 2) * rowH;
    numRow(s, i + 1, t, d, x, y, colW, { ts: 15, dy: 0.36, dh: 0.62, ds: 12.5 });
  });
  pageNum(s, 5);
  s.addNotes("۸ هدف رسمی پروژه — همان فهرست فصل ۱ گزارش.");
}

/* ---------------- S6 — Innovations ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "فصل ۱ — مقدمه", "نوآوری‌ها و ویژگی‌های متمایز");
  const rows = [
    ["پردازش هوش مصنوعی روی دستگاه کم‌توان", "کل زنجیره تشخیص چهره از رمزگشایی JPEG سخت‌افزاری تا استخراج بردار ویژگی، روی میکروکنترلر اجرا می‌شود — حریم خصوصی کامل و تاخیر تقریباً صفر."],
    ["هویت‌سنجی با سخت‌افزار موجود کاربر", "دوربین و مرورگر گوشی به‌عنوان سنسور تصویری؛ سخت‌افزار لبه به حداقل می‌رسد."],
    ["استقلال کامل از فضای ابری", "کارکرد کامل در قطعی اینترنت بین‌المللی و حتی داخلی؛ داده در شبکه محلی می‌ماند."],
    ["یادگیری رفتاری روی تراشه", "پیش‌آموزش آفلاین + به‌روزرسانی برخط SGD؛ چرخه سایه → خودکار؛ قفل در هرگز تحت کنترل مدل نیست."],
    ["اکوسیستم حضور و غیاب", "سه‌لایه با صف پایا روی برد، سرور FastAPI/SQLite، شیت شمسی و اعلان بله — مستقل از HA."],
    ["تمرین جامع مهندسی", "الکترونیک، معماری، سیستم‌عامل، شبکه، امنیت، UI/UX و مهندسی نرم‌افزار در یک کار واقعی."],
  ];
  const rowH = 0.86, y0 = 1.78;
  rows.forEach(([t, d], i) => {
    const y = y0 + i * rowH;
    s.addShape("roundRect", { x: W - M - 0.16, y: y + 0.12, w: 0.16, h: 0.16, rectRadius: 0.03, fill: { color: ACCENT }, line: { color: ACCENT, width: 0 } });
    T(s, t, { x: W - M - 4.35, y, w: 4.0, h: 0.8, fontSize: 15, bold: true, color: PRIMARY, valign: "top" });
    T(s, d, { x: M, y, w: W - 2 * M - 4.55, h: 0.8, fontSize: 12.5, color: TEXT, valign: "top", lineSpacingMultiple: 1.15 });
    if (i < rows.length - 1) s.addShape("line", { x: M, y: y + rowH - 0.12, w: W - 2 * M, h: 0, line: { color: HAIR, width: 0.75 } });
  });
  pageNum(s, 6);
  s.addNotes("۶ ویژگی متمایز؛ مهم‌ترین‌ها: هوش روی لبه و قرض گرفتن دوربین از گوشی.");
}

/* ---------------- S7 — Technologies ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "فصل ۲ — مبانی", "فناوری‌های به‌کاررفته در یک نگاه");
  imgCard(s, `${FIGS}/fig6_stack.png`, 0.7, 1.95, 5.85, 4.17);
  srcNote(s, "منبع: گزارش پروژه — شکل ۴-۱", 0.7, 6.28, 4);
  const chips = ["ESP32-S3 (N16R8)", "ESP-IDF 6 + FreeRTOS", "LVGL 9.5", "ESP-DL", "MQTT 3.1.1", "Mosquitto", "Home Assistant", "Docker", "HTTPS / TLS", "FastAPI + SQLite"];
  const cw = 2.75, chh = 0.56, gx = 0.22, gy = 0.2, x0 = 7.0, y0 = 2.05;
  chips.forEach((c, i) => {
    const col = i % 2, row = Math.floor(i / 2);
    const x = x0 + (1 - col) * (cw + gx), y = y0 + row * (chh + gy);
    card(s, x, y, cw, chh, SURFACE);
    s.addText(c, { x, y, w: cw, h: chh, fontSize: 13, fontFace: BF, color: PRIMARY, align: "center", valign: "middle", margin: 0 });
  });
  T(s, "ستون زیرساخت: کانتینرهای Docker روی سرور خانگی؛ ستون لبه: FreeRTOS چندهسته‌ای با LVGL، وب‌سرور TLS و مدل‌های ESP-DL روی یک تراشه.", { x: 6.95, y: 5.6, w: 5.85, h: 1.0, fontSize: 13.5, color: TEXT, lineSpacingMultiple: 1.3 });
  pageNum(s, 7);
  s.addNotes("پشته نرم‌افزاری از سخت‌افزار تا سرویس کاربر. سمت راست: فهرست فناوری‌ها.");
}

/* ---------------- S8 — Hardware & firmware ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "فصل ۴ — طراحی سخت‌افزار و فریمور", "سخت‌افزار لبه و ساختار فریمور");
  const cw = 6.05, ch = 4.75, y0 = 1.8;
  card(s, W - M - cw, y0, cw, ch, SURFACE, false);
  T(s, "سخت‌افزار", { x: W - M - cw + 0.3, y: y0 + 0.22, w: cw - 0.6, h: 0.45, fontSize: 18, bold: true, color: PRIMARY });
  const hw = [
    "برد ESP32-S3-DevKitC-1 با ماژول WROOM-1-N16R8: ۱۶MB فلش + ۸MB PSRAM اکتال (۸۰MHz)",
    "هسته دوگانه Xtensa LX7 تا ۲۴۰MHz با دستورالعمل‌های برداری شتاب‌دهی ML",
    "نمایشگر ST7796 با رزولوشن ۳۲۰×۴۸۰ (رابط موازی ۸ بیتی) + کنترلر لمس GT911 روی I2C",
    "سنسور محیطی BME280: دما، رطوبت و فشار روی I2C مشترک",
    "رله قفل در با بازقفل خودکار ایمن ۸ ثانیه‌ای + چراغ و دکمه فیزیکی",
  ];
  hw.forEach((t, i) => T(s, t, { x: W - M - cw + 0.3, y: y0 + 0.8 + i * 0.78, w: cw - 0.6, h: 0.74, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.15 }));
  card(s, M, y0, cw, ch, SURF2, false);
  T(s, "فریمور (ESP-IDF + FreeRTOS)", { x: M + 0.3, y: y0 + 0.22, w: cw - 0.6, h: 0.45, fontSize: 18, bold: true, color: PRIMARY });
  const fw = [
    "پارتیشن‌بندی اختصاصی فلش: برنامه، NVS و SPIFFS (پایگاه چهره)",
    "وظایف چندهسته‌ای با اولویت و پشته مستقل: رابط گرافیکی، پردازش چهره، شبکه، عامل یادگیری",
    "همگام‌سازی با صف‌های کم‌عمق (مثلاً صف تصویر چهره با عمق ۱) به‌جای اشتراک مستقیم حافظه",
    "سگ‌نگهبان وظایف (TWDT) + سگ‌نگهبان لمس با ریست سخت‌افزاری (RST/INT)",
    "پایش دوره‌ای حافظه (هر ۳۰ ثانیه) به‌عنوان زنگ هشدار شکستگی حافظه",
  ];
  fw.forEach((t, i) => T(s, t, { x: M + 0.3, y: y0 + 0.8 + i * 0.78, w: cw - 0.6, h: 0.74, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.15 }));
  T(s, "نکته پین‌گذاری: GPIO35 تا GPIO37 به دلیل PSRAM اکتال قابل استفاده نیستند.", { x: M, y: 6.68, w: W - 2 * M, h: 0.32, fontSize: 12, color: MUTED });
  pageNum(s, 8);
  s.addNotes("سمت راست سخت‌افزار، سمت چپ فریمور. تأکید بر PSRAM به‌عنوان عامل امکان‌سنج اجرای هم‌زمان UI+TLS+CNN.");
}

/* ---------------- S9 — Local infrastructure ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "فصل ۵ — معماری ارتباطی", "زیرساخت کاملاً لوکال: MQTT و Home Assistant");
  imgCard(s, `${FIGS}/fig1_architecture.png`, 0.65, 1.95, 6.6, 4.27);
  srcNote(s, "منبع: گزارش پروژه — شکل ۵-۱", 0.65, 6.4, 4);
  const rows = [
    ["MQTT: انتشار/اشتراک با کارگزار مرکزی", "QoS=1 برای وضعیت، پیام‌های Retained و وصیت‌نامه LWT برای تشخیص قطعی برد."],
    ["کشف خودکار (MQTT Discovery)", "۱۳ موجودیت بدون پیکربندی دستی در Home Assistant ثبت می‌شوند."],
    ["استقرار با Docker روی سرور خانگی", "Mosquitto + Home Assistant (+ سرور حضور و غیاب) — انتقال‌پذیر و بدون ابر."],
    ["کارکرد فقط با مودم خانگی", "در قطعی اینترنت بین‌المللی و داخلی بی‌وقفه کار می‌کند؛ تاخیر حداقلی."],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, 7.55, 2.0 + i * 1.12, 5.3, { dy: 0.37, dh: 0.55, ds: 12.5 }));
  pageNum(s, 9);
  s.addNotes("Topicهای سلسله‌مراتبی؛ افزودن مشترک جدید بدون تغییر برد. جدول topicها در گزارش (۵-۱).");
}

/* ---------------- S10 — Divider: Edge AI ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG_DARK };
  s.addText("EDGE AI", { x: 0.4, y: 1.5, w: W - 0.8, h: 3.4, fontSize: 130, fontFace: TF, color: "1D3350", align: "center", margin: 0 });
  T(s, "بخش اصلی ارائه", { x: 1, y: 2.35, w: W - 2, h: 0.4, fontSize: 15, bold: true, color: ACCENT, align: "center" });
  T(s, "هوش مصنوعی روی لبه", { x: 1, y: 2.8, w: W - 2, h: 1.0, fontSize: 44, fontFace: TF, color: "FFFFFF", align: "center" });
  T(s, "دو موتور هوشمند، هر دو روی یک میکروکنترلر کم‌توان", { x: 1, y: 3.9, w: W - 2, h: 0.45, fontSize: 16, color: LIGHT_ON_DARK, align: "center" });
  const boxes = [
    ["۱ — تشخیص چهره روی میکروکنترلر", "خط پردازش کامل بینایی ماشین با ESP-DL؛ احراز هویت بدون ابر"],
    ["۲ — عامل یادگیری رفتاری روی تراشه", "دو رگرسیون لجستیک؛ پیش‌آموزش آفلاین + یادگیری برخط SGD"],
  ];
  const bw = 5.5, by = 4.75;
  boxes.forEach(([t, d], i) => {
    const x = i === 0 ? W / 2 + 0.25 : W / 2 - bw - 0.25;
    s.addShape("roundRect", { x, y: by, w: bw, h: 1.5, rectRadius: 0.08, fill: { color: "1D3350" }, line: { color: "2E4A6E", width: 1 } });
    T(s, t, { x: x + 0.3, y: by + 0.22, w: bw - 0.6, h: 0.45, fontSize: 16.5, bold: true, color: "FFFFFF", align: "center" });
    T(s, d, { x: x + 0.3, y: by + 0.75, w: bw - 0.6, h: 0.55, fontSize: 12.5, color: LIGHT_ON_DARK, align: "center", lineSpacingMultiple: 1.2 });
  });
  pageNum(s, 10);
  s.addNotes("بخش اصلی: دو محور هوش مصنوعی. ابتدا تشخیص چهره، سپس عامل یادگیری رفتاری با تشریح کامل مدل.");
}

/* ---------------- S11 — Face pipeline ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "هوش مصنوعی روی لبه — ۱) تشخیص چهره", "خط پردازش کامل تشخیص چهره روی میکروکنترلر", 27);
  imgCard(s, `${FIGS}/fig2_face_pipeline.png`, (W - 9.0) / 2, 1.58, 9.0, 4.22);
  srcNote(s, "منبع: گزارش پروژه — شکل ۶-۱", W / 2 - 1.2, 5.92, 2.6);
  const notes = [
    "صف ورودی عمق ۱ — پردازش سری‌شده، بدون تداخل حافظه",
    "بافرهای بزرگ در PSRAM — حافظه داخلی برای TLS آزاد می‌ماند",
    "تحویل غیرمسدودکننده با وظیفه face_worker — UI پاسخ‌گو می‌ماند",
  ];
  const nw = 4.1, ny = 6.42, gap = 0.25;
  notes.forEach((t, i) => {
    const x = W - M - nw - i * (nw + gap);
    card(s, x, ny, nw, 0.72, SURF2);
    T(s, t, { x: x + 0.15, y: ny, w: nw - 0.3, h: 0.72, fontSize: 11.5, color: PRIMARY, valign: "middle", lineSpacingMultiple: 1.15 });
  });
  pageNum(s, 11);
  s.addNotes("تصویر JPEG تا ۳۰۰KB از مرورگر گوشی → رمزگشایی سخت‌افزاری به RGB565 ۳۲۰×۲۴۰ در PSRAM → آشکارسازی HumanFaceDetect → استخراج embedding با HumanFaceRecognizer → تطبیق با پایگاه چهره در SPIFFS؛ آستانه ۰٫۷۰.");
}

/* ---------------- S12 — Enrollment & threshold ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "هوش مصنوعی روی لبه — ۱) تشخیص چهره", "ثبت‌چهره چندنمونه‌ای و آستانه تصمیم");
  const steps = [
    ["صدور لینک توسط مدیر", "از بخش تنظیمات (محافظت‌شده با رمز و نشست ۱۰ دقیقه‌ای)"],
    ["توکن شش‌رقمی با اعتبار ۳ دقیقه", "لینک https://IP/enroll?token=… — توکن چندمصرف تا پایان اعتبار"],
    ["ثبت چند نمونه از صفحه وب موبایل", "POST /api/face/enroll — نمونه‌ها با زاویه و نور متفاوت"],
    ["گروه‌بندی زیر یک نام در پایگاه چهره", "لایه face_db با شمارنده نمونه‌ها در پارتیشن SPIFFS"],
  ];
  steps.forEach(([t, d], i) => numRow(s, i + 1, t, d, 6.3, 1.95 + i * 1.18, 6.55, { dy: 0.38, dh: 0.6, ds: 12.5 }));
  const bx = M, bw = 5.35;
  card(s, bx, 1.95, bw, 2.6, SURFACE, false);
  T(s, "چرا آستانه ۰٫۷۰؟", { x: bx + 0.3, y: 2.18, w: bw - 0.6, h: 0.45, fontSize: 17, bold: true, color: PRIMARY });
  T(s, "مقدار آستانه پس از آزمون‌های عملی با ثبت چندنمونه‌ای نگه داشته شد؛ ثبت چندنمونه‌ای دقت تطبیق در حالات نور و زاویه مختلف را به‌طور محسوس بهبود داد و کالیبراسیون بیشتر عمداً به نسخه‌های بعد موکول شد.", { x: bx + 0.3, y: 2.7, w: bw - 0.6, h: 1.7, fontSize: 13, color: TEXT, lineSpacingMultiple: 1.3 });
  card(s, bx, 4.75, bw, 1.85, SURF2, false);
  T(s, "تصمیم‌های مهندسی خط پردازش", { x: bx + 0.3, y: 4.95, w: bw - 0.6, h: 0.4, fontSize: 15, bold: true, color: PRIMARY });
  T(s, "صف عمق ۱ برای پایداری مدل‌ها، جای‌گذاری بافرها در PSRAM و خروجی غیرمسدودکننده وب‌سرور — همگی برای حفظ پاسخ‌گویی رابط گرافیکی روی یک تراشه.", { x: bx + 0.3, y: 5.4, w: bw - 0.6, h: 1.1, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.25 });
  pageNum(s, 12);
  s.addNotes("جریان ثبت‌چهره ۴ مرحله؛ نکته: آستانه ۰٫۷۰ با ثبت چندنمونه‌ای کفایت می‌کند (تصمیم آگاهانه).");
}

/* ---------------- S13 — ML model formulation ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "هوش مصنوعی روی لبه — ۲) عامل یادگیری رفتاری", "صورت‌بندی مدل: دو رگرسیون لجستیک با ویژگی فقط-زمینه‌ای", 26);
  // right: features table (RTL: rightmost = group)
  const rowsT = [
    [{ text: "توضیح", options: { bold: true, color: "FFFFFF", fill: { color: PRIMARY }, align: "center" } },
     { text: "تعداد", options: { bold: true, color: "FFFFFF", fill: { color: PRIMARY }, align: "center" } },
     { text: "گروه ویژگی", options: { bold: true, color: "FFFFFF", fill: { color: PRIMARY }, align: "center" } }],
    ["جمله ثابت مدل", "۱", "بایاس"],
    ["sin/cos سه همساز برای پوشش الگوی روزانه بدون گسست نیمه‌شب", "۶", "همسازهای ساعتی"],
    ["پنج‌شنبه و جمعه", "۲", "تعطیلات"],
    ["HOME پس از هر بازکردن موفق (حداقل ۴۵ دقیقه)", "۱", "حضور ساکن"],
    ["شدت نور نرمال‌شده، دما و رطوبت نرمال‌شده", "۲", "زمینه محیطی"],
  ];
  s.addTable(rowsT, {
    x: 5.75, y: 1.95, w: 7.1, colW: [4.25, 0.85, 2.0], rowH: 0.52,
    fontFace: BF, fontSize: 12, color: TEXT, align: "right", valign: "middle", rtlMode: true,
    border: { pt: 0.5, color: HAIR }, fill: { color: "FFFFFF" }, margin: 0.06,
  });
  T(s, "جدول ۶-۱ گزارش: بردار ویژگی ۱۲بعدی «فقط-زمینه‌ای» — بدون ویژگی وضعیت خود دستگاه", { x: 5.75, y: 5.35, w: 7.1, h: 0.55, fontSize: 12, color: MUTED, lineSpacingMultiple: 1.2 });
  // left: formulas
  const fx = M, fw2 = 4.9;
  card(s, fx, 1.95, fw2, 2.5, SURFACE, false);
  T(s, "مدل تصمیم (هر دستگاه یک نورون سیگموئید)", { x: fx + 0.25, y: 2.12, w: fw2 - 0.5, h: 0.42, fontSize: 14, bold: true, color: PRIMARY });
  s.addText([
    { text: "z = w\u1D40x ,   p = \u03C3(z) = 1 / (1 + e\u207B\u1DBB)", options: { breakLine: true } },
    { text: "", options: { breakLine: true } },
    { text: "p \u2265 0.75 \u2192 ON ,   p \u2264 0.25 \u2192 OFF", options: { breakLine: true } },
    { text: "0.25 < p < 0.75 \u2192 hold (no action)", options: {} },
  ], { x: fx + 0.25, y: 2.6, w: fw2 - 0.5, h: 1.7, fontSize: 14, fontFace: BF, color: TEXT, align: "left", margin: 0, lineSpacingMultiple: 1.25 });
  card(s, fx, 4.7, fw2, 2.1, "FBF4E4", false);
  T(s, "درس مهندسی: رفتار پسماندی", { x: fx + 0.25, y: 4.88, w: fw2 - 0.5, h: 0.42, fontSize: 14.5, bold: true, color: ACCENT });
  T(s, "ویژگی «وضعیت خود دستگاه» در نسخه نخست، مدل را با وجود دقت ۹۱٪ در حالت خودکار منفعل کرده بود؛ حذف آن و افزودن همساز سوم، دقت را به ۹۴٪ رساند و رفتار را فعال کرد.", { x: fx + 0.25, y: 5.32, w: fw2 - 0.5, h: 1.4, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.28 });
  pageNum(s, 13);
  s.addNotes("دو مدل مستقل چراغ/فن. ویژگی‌ها کاملاً زمینه‌ای: زمان روی دایره (همسازها)، تعطیلات، حضور، نور/دما/رطوبت. درس کلیدی: حذف ویژگی وضعیت دستگاه → رفع پسماند.");
}

/* ---------------- S14 — Model figure ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "هوش مصنوعی روی لبه — ۲) عامل یادگیری رفتاری", "معماری مدل، فرمول‌ها و قواعد تصمیم", 27);
  imgCard(s, `${FIGS}/fig_ml_model.png`, (W - 8.55) / 2, 1.62, 8.55, 5.52);
  srcNote(s, "منبع: گزارش پروژه — شکل مدل عامل رفتاری (fig_ml_model)", W / 2 - 2.2, 6.98, 4.6);
  pageNum(s, 14);
  s.addNotes("این شکل کامل‌ترین نمای مدل است: ورودی‌ها، نورون سیگموئید هر دستگاه، قاعده تصمیم با آستانه‌ها، گیت ارتقا، پیش‌آموزش آفلاین، SGD برخط و ماندگاری در NVS. قفل در هرگز تحت کنترل مدل نیست.");
}

/* ---------------- S15 — Training ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "هوش مصنوعی روی لبه — ۲) عامل یادگیری رفتاری", "آموزش ترکیبی: پیش‌آموزش آفلاین + یادگیری برخط");
  const rowsT = [
    [{ text: "مقدار", options: { bold: true, color: "FFFFFF", fill: { color: PRIMARY }, align: "center" } },
     { text: "پارامتر", options: { bold: true, color: "FFFFFF", fill: { color: PRIMARY }, align: "center" } }],
    ["۳۰ روز، ۸۶۴۰ نمونه (گام ۵ دقیقه)", "دیتاست پیش‌آموزش"],
    ["گرادیان کامل، ۱۵۰۰ تکرار، با L2", "بهینه‌سازی آفلاین"],
    ["۶ روز پایانی داده", "اعتبارسنجی"],
    ["SGD با نرخ ۰٫۰۸، محدودسازی وزن (±۸)", "یادگیری برخط"],
    ["NVS (~۹۰ بایت، هر ۳۰ ثانیه در صورت تغییر)", "ذخیره‌سازی وزن"],
    ["هر ۳۰ ثانیه", "دوره تصمیم"],
    ["p ≥ ۰٫۷۵ روشن، p ≤ ۰٫۲۵ خاموش", "آستانه عمل"],
    ["چراغ ۹۴٫۲٪ — فن ۹۳٫۶٪ (سقف بیزی ≈ ۹۵٪)", "دقت نهایی"],
  ];
  s.addTable(rowsT, {
    x: 6.55, y: 1.9, w: 6.3, colW: [4.2, 2.1], rowH: 0.55,
    fontFace: BF, fontSize: 12, color: TEXT, align: "right", valign: "middle", rtlMode: true,
    border: { pt: 0.5, color: HAIR }, margin: 0.06,
  });
  T(s, "جدول ۶-۲ گزارش: پارامترهای آموزش و عملکرد عامل", { x: M, y: 6.9, w: 5.6, h: 0.35, fontSize: 12, color: MUTED });
  const lx = M, lw = 5.6;
  card(s, lx, 1.9, lw, 2.3, SURFACE, false);
  T(s, "۱) پیش‌آموزش آفلاین (روی PC)", { x: lx + 0.28, y: 2.08, w: lw - 0.56, h: 0.42, fontSize: 15.5, bold: true, color: PRIMARY });
  T(s, "دیتاست مصنوعی ۳۰ روزه بر پایه سیاست کاربر با ۴ تا ۵ درصد نویز برچسب عمدی؛ بهینه‌سازی گرادیان کامل با منظم‌ساز L2. سقف نظری (بیزی) مسئله ≈ ۹۵٪ است.", { x: lx + 0.28, y: 2.55, w: lw - 0.56, h: 1.55, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.28 });
  card(s, lx, 4.4, lw, 2.35, SURF2, false);
  T(s, "۲) یادگیری برخط (روی خود تراشه)", { x: lx + 0.28, y: 4.58, w: lw - 0.56, h: 0.42, fontSize: 15.5, bold: true, color: PRIMARY });
  T(s, "هر اقدام دستی کاربر (از هر کانال) یک نمونه آموزشی است و یک گام SGD اجرا می‌کند؛ اقدامات خودِ مدل آموزش نمی‌شوند (بدون بازخورد خود-تحریک). وزن‌ها با نسخه‌گذاری در NVS ماندگارند و پس از ریست بازیابی می‌شوند.", { x: lx + 0.28, y: 5.05, w: lw - 0.56, h: 1.6, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.28 });
  pageNum(s, 15);
  s.addNotes("آموزش دو مرحله‌ای: آفلاین numpy (GD کامل + L2)؛ سپس SGD برخط روی تراشه با η=۰٫۰۸ و clip ±۸. قرارداد حساس: ترتیب ویژگی‌ها بین Python و C دقیقاً یکی است.");
}

/* ---------------- S16 — Learning GIF ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "هوش مصنوعی روی لبه — ۲) عامل یادگیری رفتاری", "یادگیری برخط در عمل: از وزن‌های صفر تا الگوی کاربر", 27);
  imgCard(s, `${FIGS}/fig_ml_learning.gif`, 0.7, 1.85, 7.6, 4.12);
  srcNote(s, "انیمیشن یادگیری آنلاین — هر اقدام کاربر یک گام SGD (fig_ml_learning)", 0.7, 6.1, 6.5);
  const rows = [
    ["شروع در فاز سایه با w = 0", "مدل هیچ چیزی نمی‌داند؛ p خروجی ثابت است."],
    ["هر اقدام دستی = یک گام SGD", "به‌روزرسانی وزن با نرخ یادگیری ۰٫۰۸ و محدودسازی ±۸"],
    ["خم p(light) به الگوی شب/روز نزدیک می‌شود", "وزن‌های همسازهای ساعتی و حضور شکل می‌گیرند."],
    ["پس از عبور از گیت ارتقا → حالت خودکار", "دقت پنجره ۲۰تایی به ≥ ۸۵٪ می‌رسد."],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, 8.6, 2.0 + i * 1.12, 4.25, { dy: 0.37, dh: 0.55, ds: 11.5, ts: 13.5 }));
  T(s, "نکته: در انیمیشن وزن‌ها از صفر شروع می‌شوند تا فرایند یادگیری دیده شود؛ دستگاه واقعی از وزن‌های پیش‌آموزش‌شده boot می‌کند.", { x: 8.6, y: 6.35, w: 4.25, h: 0.75, fontSize: 11.5, color: MUTED, lineSpacingMultiple: 1.2 });
  pageNum(s, 16);
  s.addNotes("گیف: نمودار چپ p(light ON) در طول روز؛ نمودار راست وزن‌ها. حرکت: هر اقدام کاربر گام SGD؛ خم به سمت الگوی واقعی؛ سپس ارتقا.");
}

/* ---------------- S17 — Shadow → Auto ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "هوش مصنوعی روی لبه — ۲) عامل یادگیری رفتاری", "چرخه زندگی مدل: فاز سایه → فاز خودکار", 27);
  imgCard(s, `${FIGS}/fig5_ml_agent.png`, 0.65, 1.85, 7.9, 4.23);
  srcNote(s, "منبع: گزارش پروژه — شکل ۶-۲", 0.65, 6.22, 4);
  const rows = [
    ["فاز سایه: پیش‌بینی بدون اقدام", "هر ۳۰ ثانیه پیش‌بینی، انتشار در MQTT و ارزیابی."],
    ["گیت ارتقا", "پنجره متحرک ۲۰ تصمیم؛ ارتقا با ≥ ۱۵ تصمیم صحیح و دقت ≥ ۸۵٪ (هر دستگاه مستقل)."],
    ["فاز خودکار: اقدام فقط با اطمینان", "p ≥ ۰٫۷۵ → روشن، p ≤ ۰٫۲۵ → خاموش، در غیر این صورت بدون اقدام."],
    ["اقدام دستی همیشه برنده است", "فوراً اجرا و به‌عنوان نمونه آموزشی ثبت می‌شود؛ سوییچ ML Autonomy در HA."],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, 8.85, 1.95 + i * 1.15, 4.0, { dy: 0.36, dh: 0.62, ds: 11.5, ts: 13.5 }));
  card(s, 8.85, 6.55, 4.0, 0.52, "FBF4E4");
  T(s, "قفل در به‌طور قطعی هرگز تحت کنترل مدل نیست", { x: 9.0, y: 6.55, w: 3.7, h: 0.52, fontSize: 11.5, bold: true, color: ACCENT, valign: "middle" });
  pageNum(s, 17);
  s.addNotes("چرخه سایه→خودکار + بازگشت به سایه با سوییچ HA. قاعده امنیتی سخت: قفل بیرون از کنترل مدل.");
}

/* ---------------- S18 — ML results ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "هوش مصنوعی روی لبه — ۲) عامل یادگیری رفتاری", "نتایج: مدل به مرز نظری مسئله رسیده است", 27);
  const stats = [
    ["۹۴٫۲٪", "دقت چراغ (اعتبارسنجی)", PRIMARY],
    ["۹۳٫۶٪", "دقت فن (اعتبارسنجی)", PRIMARY],
    ["≈ ۹۵٪", "سقف بیزی مسئله", ACCENT],
  ];
  const sw = 3.7, sy = 1.95, shh = 2.0;
  stats.forEach(([v, l, c], i) => {
    const x = W - M - sw - i * (sw + 0.3);
    card(s, x, sy, sw, shh, SURFACE, true);
    T(s, v, { x, y: sy + 0.22, w: sw, h: 1.0, fontSize: 46, fontFace: TF, color: c, align: "center" });
    T(s, l, { x: x + 0.2, y: sy + 1.35, w: sw - 0.4, h: 0.45, fontSize: 14, color: MUTED, align: "center" });
  });
  T(s, "سناریوهای رفتاری مرجع — بازآزمایی روی فریمور مستقر:", { x: M, y: 4.35, w: W - 2 * M, h: 0.45, fontSize: 15.5, bold: true, color: PRIMARY });
  const scen = [
    ["شب + حضور + تاریکی", "p ≈ ۰٫۹۹ → روشن کردن چراغ"],
    ["ظهر با چراغ روشن", "p ≤ ۰٫۰۵ → خاموش کردن چراغ"],
    ["ظهر با چراغ خاموش", "p ≈ ۰٫۰۷ → بدون اقدام (منطقه سکون)"],
  ];
  const rw = 4.0, ry = 4.95;
  scen.forEach(([t, d], i) => {
    const x = W - M - rw - i * (rw + 0.25);
    card(s, x, ry, rw, 1.15, SURF2);
    T(s, t, { x: x + 0.22, y: ry + 0.14, w: rw - 0.44, h: 0.4, fontSize: 13.5, bold: true, color: PRIMARY });
    T(s, d, { x: x + 0.22, y: ry + 0.58, w: rw - 0.44, h: 0.45, fontSize: 13, color: TEXT });
  });
  T(s, "برچسب‌های داده عمداً ۴ تا ۵ درصد نویز انسانی دارند؛ با این حال ارتقای خودکار سایه → خودکار در آزمون عملی مشاهده شد.", { x: M, y: 6.45, w: W - 2 * M, h: 0.4, fontSize: 12.5, color: MUTED });
  pageNum(s, 18);
  s.addNotes("دقت روی ۶ روز پایانی داده. سه سناریوی مرجع روی فریمور واقعی. مدل عملاً به سقف بیزی رسیده.");
}

/* ---------------- S19 — UI channels ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "فصل ۷ — رابط‌های کاربری", "چهار کانال موازی با یک منبع واحد وضعیت");
  imgCard(s, `${FIGS}/fig3_ui_channels.png`, 0.65, 1.95, 6.35, 3.62);
  srcNote(s, "منبع: گزارش پروژه — شکل ۷-۱", 0.65, 5.7, 4);
  const rows = [
    ["وب‌سرور امن روی خود برد", "esp_https_server با گواهی خودامضا؛ TLS پیش‌نیاز فنی دسترسی به دوربین (getUserMedia)."],
    ["ورود کوتاه‌مسیر با QR", "صفحات recognize، enroll و attendance با توکن‌های زمان‌دار و نوسازی خودکار."],
    ["منبع واحد وضعیت (app_state)", "نمایشگر، پنل وب، داشبورد HA و اپ HA همیشه تصویر سازگار نشان می‌دهند."],
    ["تنزل محترمانه در خرابی", "بدون شبکه، نمایشگر همچنان قفل، چراغ و فن را مدیریت می‌کند."],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, 7.35, 1.95 + i * 1.15, 5.5, { dy: 0.36, dh: 0.6, ds: 12 }));
  T(s, "۱۳ موجودیت HA بدون یک خط کد اضافه روی برد؛ شش صفحه LVGL روی نمایشگر.", { x: 7.35, y: 6.55, w: 5.5, h: 0.4, fontSize: 12, color: MUTED });
  pageNum(s, 19);
  s.addNotes("چهار کانال: LCD، وب موبایل، داشبورد HA، اپ HA. جدول endpoint ها در گزارش (۷-۱).");
}

/* ---------------- S20 — Attendance ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "فصل ۸ — حضور و غیاب", "معماری سه‌لایه با صف پیام پایا");
  imgCard(s, `${FIGS}/fig4_attendance.png`, 0.65, 1.95, 6.5, 3.77);
  srcNote(s, "منبع: گزارش پروژه — شکل ۸-۱", 0.65, 5.85, 4);
  const rows = [
    ["لایه برد: تولید QR و تطبیق چهره", "QR ده‌دقیقه‌ای با شمارش معکوس؛ ضدتکرار ۶۰ ثانیه‌ای."],
    ["صف پایا در NVS (۳۲ خانه)", "اگر شبکه/سرور در دسترس نباشد رکوردها می‌مانند و بعداً به‌ترتیب ارسال می‌شوند."],
    ["سرور لوکال: FastAPI + SQLite", "جدول به‌شکل صندوق خروج (Outbox)؛ همگام‌سازی هر ۱۵ ثانیه."],
    ["تحویل اختیاری: شیت شمسی + بله", "تقویم جلالی بدون وابستگی؛ برد هیچ کلید ابری نگه نمی‌دارد."],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, 7.45, 1.95 + i * 1.15, 5.4, { dy: 0.36, dh: 0.6, ds: 12 }));
  pageNum(s, 20);
  s.addNotes("مستقل از HA و MQTT. تفکیک اعتبارنامه‌ها: ارتباط برد-سرور فقط با رمز مشترک.");
}

/* ---------------- S21 — Security ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "فصل ۹ — امنیت و حریم خصوصی", "لایه‌های دفاعی سامانه");
  const items = [
    ["حریم خصوصی داده چهره", "تصویر روی خود برد پردازش و آزاد می‌شود؛ خروجی فقط «تصمیم» و بردار ویژگی است — نه تصویر خام، نه سرویس ابری."],
    ["امنیت لایه وب", "TLS با گواهی خودامضا؛ کوکی نشست HttpOnly/Secure/SameSite=Strict با مهلت ۱۰ دقیقه؛ توکن‌های ۳ و ۱۰ دقیقه‌ای؛ تأخیر عمدی ۱ ثانیه در رمز نادرست."],
    ["امنیت قفل در", "بازکردن فقط از مسیرهای تأییدشده؛ بدون هیچ مسیر فرمان از HA؛ رله با سازوکار ایمن مستقل: در صورت قطع فریمور، بازگشت خودکار به قفل حداکثر پس از ۸ ثانیه."],
    ["مرز شناخته‌شده و مسیر ارتقا", "Mosquitto فعلاً بدون احراز هویت (انتخاب آگاهانه برای استقرار خانگی)؛ گام بعد: auth+TLS بروکر و WebAuthn — بدون تغییر در برد."],
  ];
  const cw = 6.05, chh = 2.28, y0 = 1.85;
  items.forEach(([t, d], i) => {
    const col = i % 2 === 0 ? 1 : 0;
    const x = col ? W - M - cw : M;
    const y = y0 + Math.floor(i / 2) * (chh + 0.3);
    card(s, x, y, cw, chh, i === 0 ? SURF2 : SURFACE, true);
    T(s, t, { x: x + 0.3, y: y + 0.24, w: cw - 0.6, h: 0.45, fontSize: 16.5, bold: true, color: PRIMARY });
    T(s, d, { x: x + 0.3, y: y + 0.78, w: cw - 0.6, h: chh - 1.0, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.3 });
  });
  pageNum(s, 21);
  s.addNotes("چهار محور امنیتی. تأکید: تصویر چهره هرگز از دستگاه خارج نمی‌شود؛ قفل fail-safe است؛ مرز Mosquitto شناخته‌شده و مسیر ارتقای آن برنامه‌ریزی‌شده است.");
}

/* ---------------- S22 — Tests ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "فصل ۱۰ — آزمایش‌ها و نتایج", "آزمون انتهابه‌انتها: ۱۴ سناریو، هر ۱۴ موفق");
  const stats2 = [
    ["۱۴/۱۴", "سناریوی آزمون موفق"],
    ["۳", "سطح آزمون: واحد، یکپارچگی، E2E"],
    ["۰", "ریست غیرعمدی در کارکرد طولانی‌مدت"],
  ];
  const sw2 = 5.2, sy2 = 1.9;
  stats2.forEach(([v, l], i) => {
    const y = sy2 + i * 1.58;
    card(s, M, y, sw2, 1.38, SURFACE, true);
    T(s, v, { x: M + 3.0, y: y + 0.1, w: 1.9, h: 1.18, fontSize: 30, fontFace: TF, color: i === 0 ? ACCENT : PRIMARY, align: "center", valign: "middle" });
    T(s, l, { x: M + 0.25, y: y + 0.1, w: 2.7, h: 1.18, fontSize: 13, color: MUTED, align: "right", valign: "middle", lineSpacingMultiple: 1.2 });
  });
  const rowsT = [
    [{ text: "نتیجه", options: { bold: true, color: "FFFFFF", fill: { color: PRIMARY }, align: "center" } },
     { text: "سناریو", options: { bold: true, color: "FFFFFF", fill: { color: PRIMARY }, align: "center" } }],
    ["موفق", "بازکردن در با چهره در نور و زاویه متفاوت"],
    ["موفق", "رد چهره ناشناس و تلاش مجدد"],
    ["موفق", "حضور و غیاب + ضدتکرار ۶۰ ثانیه‌ای"],
    ["موفق", "صف پایا: قطع سرور و بازگشت"],
    ["موفق", "اتصال مجدد وای‌فای و MQTT (۵ تلاش)"],
    ["موفق", "قطعی کامل اینترنت: کارکرد همه قابلیت‌های لوکال"],
    ["موفق", "بازیابی خودکار لمس از قفل‌شدگی I2C"],
  ];
  s.addTable(rowsT, {
    x: 5.95, y: 1.9, w: 6.9, colW: [1.25, 5.65], rowH: 0.51,
    fontFace: BF, fontSize: 12, color: TEXT, align: "right", valign: "middle", rtlMode: true,
    border: { pt: 0.5, color: HAIR }, margin: 0.06,
  });
  T(s, "روش: توسعه فیچرمحور روی شاخه‌های Git و آزمون عملی روی برد پیش از ادغام؛ گزیده جدول ۱۰-۱ گزارش.", { x: M, y: 6.65, w: 5.2, h: 0.6, fontSize: 12, color: MUTED, lineSpacingMultiple: 1.25 });
  pageNum(s, 22);
  s.addNotes("آزمون سناریومحور E2E توسط کاربر واقعی. گزیده ۷ سناریو از ۱۴ سناریوی جدول ۱۰-۱.");
}

/* ---------------- S23 — Engineering challenges ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "فصل ۱۰ — آزمایش‌ها و نتایج", "چالش‌های مهندسی ریشه‌یابی‌شده و حل‌شده");
  const items = [
    ["کمبود حافظه در هندشیک TLS پس از افزودن وب‌گرافیک", "ریشه: اشغال حافظه داخلی توسط بافرهای رندر LVGL → جابه‌جایی بافرها به PSRAM + keep-alive لایه شبکه + lru_purge و ۱۶ سوکت lwIP"],
    ["قفل‌شدگی گذرگاه I2C کنترلر لمس GT911", "ریشه: نشتی mutex در مسیر خطا → اصلاح مسیر آزادسازی + «سگ‌نگهبان لمس» با توالی ریست سخت‌افزاری (RST/INT) — سامانه خودبازیاب شد"],
    ["حلقه‌های panic سگ‌نگهبان در وظیفه چهره", "ریشه: انسداد طولانی انتظار برای کار جدید → ثبت دوره‌ای TWDT (حداکثر هر ۵ ثانیه) در حلقه انتظار + مهلت ۲۰ ثانیه‌ای برای دریافت بدنه تصویر"],
    ["رفتار منفعل مدل رفتاری با دقت بالا", "ریشه: ویژگی «وضعیت خود دستگاه» باعث پسماند و بی‌عملی می‌شد → بازطراحی بردار ویژگی به فرم صرفاً زمینه‌ای (فصل ۶)"],
  ];
  const rowH = 1.22, y0 = 1.85;
  items.forEach(([t, d], i) => {
    const y = y0 + i * rowH;
    T(s, fa(i + 1), { x: W - M - 0.6, y: y + 0.05, w: 0.6, h: 0.6, fontSize: 24, fontFace: TF, color: ACCENT, align: "center" });
    T(s, t, { x: M + 0.2, y, w: W - 2 * M - 0.95, h: 0.42, fontSize: 14.5, bold: true, color: PRIMARY });
    T(s, d, { x: M + 0.2, y: y + 0.44, w: W - 2 * M - 0.95, h: 0.66, fontSize: 12, color: MUTED, lineSpacingMultiple: 1.2 });
    if (i < items.length - 1) s.addShape("line", { x: M, y: y + rowH - 0.14, w: W - 2 * M, h: 0, line: { color: HAIR, width: 0.75 } });
  });
  pageNum(s, 23);
  s.addNotes("هر چالش: علامت → ریشه → فیکس. نشان‌دهنده مهارت ریشه‌یابی واقعی در سیستم‌های نهفته.");
}

/* ---------------- S24 — Summary + future ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "فصل ۱۱ — جمع‌بندی", "جمع‌بندی و مسیر توسعه آینده");
  const cw = 6.05, y0 = 1.85, chh = 4.9;
  card(s, W - M - cw, y0, cw, chh, SURFACE, false);
  T(s, "آنچه ساخته شد", { x: W - M - cw + 0.3, y: y0 + 0.22, w: cw - 0.6, h: 0.45, fontSize: 17, bold: true, color: PRIMARY });
  const done = [
    "تشخیص چهره کامل روی میکروکنترلر؛ دوربین و رابط کاربری قرضی از گوشی",
    "زیرساخت صددرصد لوکال با Docker — کارکرد کامل در قطعی اینترنت",
    "عامل یادگیری رفتاری با دقت ۹۴٫۲٪ و ۹۳٫۶٪ و یادگیری برخط ماندگار",
    "حضور و غیاب سه‌لایه با صف پایا، شیت شمسی و اعلان بله",
    "چهار کانال تعامل هم‌راستا با منبع واحد وضعیت",
    "پایداری اثبات‌شده: سگ‌نگهبان‌ها، اتصال مجدد شبکه‌آگاه، آزمون E2E",
  ];
  done.forEach((t, i) => T(s, t, { x: W - M - cw + 0.3, y: y0 + 0.8 + i * 0.68, w: cw - 0.6, h: 0.64, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.15 }));
  card(s, M, y0, cw, chh, SURF2, false);
  T(s, "کارهای آینده", { x: M + 0.3, y: y0 + 0.22, w: cw - 0.6, h: 0.45, fontSize: 17, bold: true, color: PRIMARY });
  const next = [
    "یادگیری فدرال روی میکروکنترلرها — بدون خروج داده خام",
    "هوشمندسازی صوتی روی تراشه با ESP-SR (کانال پنجم)",
    "شناسایی زنده‌بودن (liveness) برای مقاوم‌سازی چهره",
    "به‌روزرسانی بی‌سیم فریمور (OTA)",
    "امن‌سازی Mosquitto (auth + TLS) و مهاجرت به WebAuthn",
    "سنسورهای واقعی حضور/نور، اثر انگشت دو-عاملی و برد اختصاصی (PCB)",
  ];
  next.forEach((t, i) => T(s, t, { x: M + 0.3, y: y0 + 0.8 + i * 0.68, w: cw - 0.6, h: 0.64, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.15 }));
  pageNum(s, 24);
  s.addNotes("جمع‌بندی: پروژه به همه اهداف طراحی رسید. آینده: فدرال، صوتی، liveness، OTA، امن‌سازی، PCB.");
}

/* ---------------- S25 — Closing ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG_DARK };
  s.addImage({ path: `${FIGS}/logo_1.png`, x: W / 2 - 0.45, y: 1.35, w: 0.9, h: 0.9 });
  T(s, "با تشکر از توجه شما", { x: 1, y: 2.7, w: W - 2, h: 1.0, fontSize: 40, fontFace: TF, color: "FFFFFF", align: "center" });
  T(s, "پرسش و پاسخ", { x: 1, y: 3.85, w: W - 2, h: 0.55, fontSize: 20, bold: true, color: ACCENT, align: "center" });
  s.addShape("line", { x: W / 2 - 1.1, y: 4.75, w: 2.2, h: 0, line: { color: "2E4A6E", width: 1 } });
  T(s, "محمد سروش ربیعی — طراحی و پیاده‌سازی سامانه خانه هوشمند مبتنی بر اینترنت اشیا با هوش مصنوعی روی لبه", { x: 1.5, y: 5.0, w: W - 3, h: 0.45, fontSize: 13, color: LIGHT_ON_DARK, align: "center" });
  T(s, "دانشکده مهندسی برق و کامپیوتر، دانشگاه صنعتی اصفهان — مهر ۱۴۰۵", { x: 1.5, y: 5.5, w: W - 3, h: 0.4, fontSize: 12, color: "8FA3BC", align: "center" });
  pageNum(s, 25);
  s.addNotes("پایان ارائه. آماده پاسخ به پرسش‌ها. اسلایدهای ۱۴ و ۱۶ برای بازگشت سریع در پرسش‌های ML مناسب‌اند.");
}

pres.writeFile({ fileName: "D:/Project/Smart-Home/presentation/SmartHome-Defense.pptx" })
  .then(() => console.log("WRITTEN OK"))
  .catch(e => { console.error("FAIL", e); process.exit(1); });
