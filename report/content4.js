// Chapters 10-11 + references
const { P, Pm, B, NItem, H1, H2, H3, FIG, TBL } = require("./helpers");
const { Paragraph, TextRun, AlignmentType, HeadingLevel } = require("docx");
const { en, FONT_EN } = require("./helpers");

const ch10 = [
  H1("فصل ۱۰: آزمایش‌ها، نتایج و ارزیابی", { pageBreakBefore: true }),

  H2("۱۰-۱ روش آزمون"),
  P("ارزیابی سامانه در سه سطح انجام شد: آزمون واحد اجزا (راه‌اندازی هر درایور و ماژول به‌تنهایی و بررسی لاگ)، آزمون یکپارچگی (تبادل پیام میان برد، کارگزار MQTT و Home Assistant با ابزارهای کلاینت MQTT) و آزمون انتهابه‌انتها سناریومحور (اجرای کامل چرخه‌های کاربردی توسط کاربر واقعی). توسعه به‌صورت فیچرمحور روی شاخه‌های مجزای Git انجام و پس از آزمون عملی روی برد ادغام شد؛ به این ترتیب هر ویژگی پیش از ورود به خط اصلی، در شرایط واقعی سنجیده شده است."),

  H2("۱۰-۲ سناریوهای آزمون"),
  P("سناریوهای اصلی آزمون و نتایج آن‌ها در جدول ۱۰-۱ آمده است."),
  ...TBL("جدول ۱۰-۱: سناریوهای آزمون و نتایج",
    ["سناریو", "شرط / ورودی", "انتظار", "نتیجه"],
    [
      ["بازکردن در با چهره", "چهره ثبت‌شده، نور و زاویه متفاوت", "باز شدن قفل و رویداد دسترسی", "موفق"],
      ["رد چهره ناشناس", "فرد ثبت‌نشده", "عدم باز شدن و امکان تلاش مجدد", "موفق"],
      ["بازکردن با رمز", "رمز صحیح / نادرست", "باز شدن / خطا با تأخیر ۱ ثانیه", "موفق"],
      ["ثبت‌چهره چندنمونه‌ای", "توکن معتبر، ۳ نمونه", "گروه‌شدن زیر یک نام و بهبود تطبیق", "موفق"],
      ["انقضای توکن", "QR پس از ۳ / ۱۰ دقیقه", "رد درخواست و تولد توکن جدید", "موفق"],
      ["حضور و غیاب ورود/خروج", "چهره ثبت‌شده + توکن", "ثبت در سرور، شیت و اعلان بله", "موفق"],
      ["ضدتکرار حضور", "دو ثبت پیاپی < ۶۰ ثانیه", "سرکوب رویداد دوم", "موفق"],
      ["صف پایا", "قطع شبکه سرور و بازگشت", "ماندن رکوردها و ارسال پس از اتصال", "موفق"],
      ["اتصال مجدد وای‌فای", "قطع و وصل رادیو", "انتخاب شبکه اخیر و بازگشت به شبکه", "موفق"],
      ["اتصال مجدد MQTT", "ریست کارگزار", "۵ تلاش متوالی و بازیابی اشتراک‌ها", "موفق"],
      ["رفتار در قطعی اینترنت", "قطع کامل اینترنت", "کارکرد کامل همه قابلیت‌های لوکال", "موفق"],
      ["ارتقای عامل رفتاری", "پنجره ۲۰ تصمیم", "عبور از آستانه و انتقال سایه→خودکار", "موفق"],
      ["پایداری طولانی‌مدت", "کارکرد پیوسته", "بدون ریست غیرعمدی (نظارت TWDT)", "موفق"],
      ["بازیابی لمس", "قفل‌شدگی I2C در GT911", "ریست سخت‌افزاری و ادامه کار", "موفق"],
    ],
    [24, 26, 32, 18], { cellSize: 22 }),

  H2("۱۰-۳ نتایج عامل یادگیری رفتاری"),
  P("مدل نهایی روی داده اعتبارسنجی شش روز پایانی به دقت ۹۴٫۲ درصد برای چراغ و ۹۳٫۶ درصد برای فن رسید؛ سقف نظری (بیزی) این مسئله با توجه به نویز برچسب عمدی ۴ تا ۵ درصدی حدود ۹۵ درصد است و مدل عملاً به مرز مسئله رسیده است. سه سناریوی رفتاری مرجع نیز روی فریمور مستقر بازآزمایی شد: شب + حضور + تاریکی (p≈۰٫۹۹ → روشن کردن)، ظهر با چراغ روشن (p≤۰٫۰۵ → خاموش کردن) و ظهر با چراغ خاموش (p≈۰٫۰۷ → بدون اقدام). ارتقای خودکار از فاز سایه به خودکار نیز پس از حداقل ۱۵ تصمیم صحیح در پنجره ۲۰تایی، در آزمون عملی مشاهده شد."),

  H2("۱۰-۴ چالش‌های مهندسی حل‌شده"),
  P("در مسیر پیاده‌سازی، چند مسئله واقعی ریشه‌یابی و حل شد که هر یک درس مهندسی مستقلی داشت:"),
  NItem("۱. کمبود حافظه در هندشیک TLS پس از افزودن وب‌گرافیک: ریشه‌یابی نشان داد بافرهای رندر LVGL حافظه داخلی را اشغال کرده‌اند؛ جابه‌جایی بافرها به PSRAM همراه با تنظیم keep-alive لایه شبکه (بیکار ۳۰ / فاصله ۵ / شمار ۳)، فعال‌سازی lru_purge و ۱۶ سوکت lwIP مشکل را به‌طور پایدار رفع کرد."),
  NItem("۲. قفل‌شدگی گذرگاه I2C کنترلر لمس GT911: نشتی mutex در مسیر خطا به بن‌بست می‌رسید؛ با اصلاح مسیر آزادسازی قفل و افزودن «سگ‌نگهبان لمس» با توالی ریست سخت‌افزاری (RST و INT)، سامانه از این خرابی خودبازیاب شد."),
  NItem("۳. حلقه‌های panic سگ‌نگهبان در وظیفه چهره: انسداد طولانی انتظار برای کار جدید باعث گرسنگی وظیفه سنجش شد؛ ثبت دوره‌ای TWDT (حداکثر هر ۵ ثانیه) در حلقه انتظار و مهلت ۲۰ ثانیه‌ای برای دریافت بدنه تصویر، پایداری را تضمین کرد."),
  NItem("۴. رفتار منفعل مدل رفتاری با دقت بالا: به‌کارگیری ویژگی «وضعیت خود دستگاه» باعث پسماند و بی‌عملی در حالت خودکار می‌شد؛ بازطراحی بردار ویژگی به فرم صرفاً زمینه‌ای، مسئله را ریشه‌ای حل کرد (فصل ۶)."),

  H2("۱۰-۵ ارزیابی کلی"),
  P("ارزیابی کلی نشان می‌دهد سامانه به اهداف طراحی رسیده است: چرخه کامل بازکردن در با چهره داخل شبکه محلی و روی میکروکنترلر اجرا می‌شود؛ زیرساخت MQTT و Home Assistant لوکال در قطعی اینترنت بی‌وقفه کار می‌کند؛ چهار کانال کاربری تصویر سازگاری از وضعیت ارائه می‌دهند؛ سامانه حضور و غیاب با صف پایا و تاریخ شمسی در محیط واقعی کارکرد داشته است؛ و فریمور در کارکرد طولانی بدون ریست غیرعمدی پایدار مانده است. مکانیزم‌های پایداری حل‌شده (سگ‌نگهبان‌ها، اتصال مجدد شبکه‌آگاه و پایش حافظه) تجربه عملی ارزشمندی در مهندسی سیستم‌های نهفته مقیاس‌پذیر فراهم کردند."),
];

const ch11 = [
  H1("فصل ۱۱: جمع‌بندی و کارهای آینده", { pageBreakBefore: true }),

  H2("۱۱-۱ جمع‌بندی"),
  P("در این پروژه یک سامانه خانه هوشمند کامل مبتنی بر اینترنت اشیا طراحی و پیاده‌سازی شد که ویژگی متمایز آن، انتقال هوش مصنوعی به دستگاه کم‌توان لبه و استفاده از امکانات موجود کاربر برای احراز هویت است: تشخیص چهره به‌طور کامل روی میکروکنترلر اجرا می‌شود، دوربین و رابط کاربری غنی از گوشی هوشمند کاربر به‌عنوان سنسور و میزبان تعامل به کار گرفته می‌شود و زیرساخت (MQTT و Home Assistant) کاملاً لوکال با Docker مستقر است. اجزای اصلی تکمیل‌شده عبارت‌اند از: گره ESP32-S3 با نمایشگر لمسی و وب‌سرور امن؛ خط پردازش چهره روی تراشه با ثبت چندنمونه‌ای؛ یکپارچه‌سازی کشف خودکار با ۱۳ موجودیت HA؛ عامل یادگیری رفتاری با پیش‌آموزش آفلاین و یادگیری برخط (دقت ۹۴٫۲ و ۹۳٫۶ درصد)؛ سامانه حضور و غیاب سه‌لایه با صف پایا، شیت شمسی و اعلان بله؛ و سازوکارهای پایداری (سگ‌نگهبان‌ها و اتصال مجدد شبکه‌آگاه)."),
  P("از منظر یادگیری، این پروژه عمداً به‌گونه‌ای طراحی شد که مروری عملی بر اکثر درس‌های تخصصی مقطع کارشناسی باشد: طراحی منطقی و الکترونیک (گذرگاه‌ها، رله، مقاومت‌های بالاکش)، معماری کامپیوتر و محدودیت‌های سخت‌افزار (حافظه، PSRAM، پارتیشن‌بندی)، سیستم‌عامل و برنامه‌نویسی چندهسته‌ای (FreeRTOS، همروندی، سگ‌نگهبان)، شبکه و پروتکل‌ها (MQTT، HTTPS/TLS، DNS و DHCP)، پایگاه داده و مهندسی نرم‌افزار (معماری ماژولار، صف پایا، Git و توسعه شاخه‌محور)، هوش مصنوعی (کوانتیزه‌سازی مدل، TinyML، یادگیری برخط) و طراحی رابط و تجربه کاربری. محصول نهایی، افزون بر خود سامانه، یک بستر آزمایشگاهی پایدار برای توسعه و آزمایش ایده‌های بعدی — از تشخیص تصویر و فرمان‌های صوتی تا تحلیل الگوهای رفتاری سنسورها — است."),

  H2("۱۱-۲ کارهای آینده"),
  P("مسیرهای توسعه آینده پروژه به شرح زیر پیشنهاد می‌شود:"),
  NItem("۱. یادگیری فدرال روی میکروکنترلرها: به‌کارگیری یادگیری فدرال برای آموزش مشترک مدل‌های رفتاری میان چند برد خانه، بدون خروج داده خام از دستگاه‌ها — ادامه طبیعی رویکرد «هوش در لبه با حفظ حریم خصوصی» این پروژه."),
  NItem("۲. هوشمندسازی صوتی: افزودن تشخیص فرمان‌های صوتی روی تراشه با چارچوب ESP-SR به‌عنوان کانال تعامل پنجم."),
  NItem("۳. تحلیل الگوهای رفتاری سنسورها: بسط عامل یادگیری به داده‌های سنسورهای واقعی حضور (PIR HC-SR501) و شدت نور (BH1750) و فن واقعی با رله دوکاناله."),
  NItem("۴. اثر انگشت به‌عنوان عامل احراز هویت دوم: افزودن سنسور اثر انگشت برای ورود بدون گوشی و اتصال دو-عاملی."),
  NItem("۵. امن‌سازی کارگزار MQTT (احراز هویت و TLS) و ارتقای اعتبارسنجی وب به استاندارد WebAuthn روی بستر توکن‌های موجود."),
  NItem("۶. شناسایی زنده‌بودن (liveness detection) برای مقاوم‌سازی تشخیص چهره در برابر عکس و پخش ویدئو."),
  NItem("۷. به‌روزرسانی بی‌سیم فریمور (OTA) برای نگهداری آسان‌تر."),
  NItem("۸. شبیه‌ساز LVGL روی رایانه (WSL2 با SDL2) برای چرخه توسعه سریع‌تر رابط گرافیکی."),
  NItem("۹. بهبود داشبورد Home Assistant (نمودارها، اتوماسیون‌ها و مدیریت چهره‌ها از HA) و دکمه بازآموزی مدل."),
  NItem("۱۰. استقرار نهایی روی سخت‌افزار ماندگار: قفل برقی واقعی، جعبه و سیم‌کشی تمیز، مقاومت‌های بالاکش I2C و انتقال سرویس‌ها به رایانه کم‌مصرف همیشه‌روشن."),
  NItem("۱۱. گزینه‌های تعاملی تکمیلی: BLE/iBeacon و NFC برای ورود نزدیک‌فاصله، و اپ موبایل اختصاصی."),
  NItem("۱۲. طراحی برد چاپی اختصاصی (PCB) برای نسخه محصولی سامانه."),
];

const refs = [
  new Paragraph({
    bidirectional: true,
    alignment: AlignmentType.CENTER,
    heading: HeadingLevel.HEADING_1,
    pageBreakBefore: true,
    spacing: { before: 240, after: 300, line: Math.ceil(18 * 23), lineRule: "atLeast" },
    children: [new TextRun({ text: "منابع", rightToLeft: true, font: { ascii: "Times New Roman", hAnsi: "Times New Roman", cs: "Vazir" }, size: 36, bold: true, color: "000000" })],
  }),
  ...[
    "[1] Espressif Systems, \u201CESP-IDF Programming Guide (v6.x).\u201D [Online]. Available: https://docs.espressif.com/projects/esp-idf/",
    "[2] Espressif Systems, \u201CESP-DL: Deep Learning Library for ESP Series.\u201D [Online]. Available: https://github.com/espressif/esp-dl",
    "[3] Espressif Systems, \u201Chuman_face_detect / human_face_recognition Components Registry.\u201D [Online]. Available: https://components.espressif.com/",
    "[4] FreeRTOS Team, \u201CThe FreeRTOS Reference Manual.\u201D [Online]. Available: https://www.freertos.org/",
    "[5] LVGL, \u201CLight and Versatile Graphics Library, v9.5 Documentation.\u201D [Online]. Available: https://docs.lvgl.io/",
    "[6] OASIS Standard, \u201CMQTT Version 3.1.1 Specification.\u201D [Online]. Available: https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/",
    "[7] Eclipse Mosquitto, \u201CMosquitto Documentation.\u201D [Online]. Available: https://mosquitto.org/documentation/",
    "[8] Home Assistant, \u201CMQTT Discovery Documentation.\u201D [Online]. Available: https://www.home-assistant.io/integrations/mqtt/",
    "[9] Docker Inc., \u201CDocker Documentation.\u201D [Online]. Available: https://docs.docker.com/",
    "[10] E. Rescorla and T. Dierks, \u201CThe Transport Layer Security (TLS) Protocol Version 1.2,\u201D RFC 5246, 2008.",
    "[11] FastAPI, \u201CFastAPI Documentation.\u201D [Online]. Available: https://fastapi.tiangolo.com/",
    "[12] SQLite Consortium, \u201CSQLite Documentation.\u201D [Online]. Available: https://www.sqlite.org/docs.html",
    "[13] P. Warden and D. Situnayake, \u201CTinyML: Machine Learning with TensorFlow Lite on Arduino and Ultra-Low-Power Microcontrollers,\u201D O'Reilly Media, 2020.",
    "[14] B. McMahan et al., \u201CCommunication-Efficient Learning of Deep Networks from Decentralized Data (Federated Learning),\u201D in Proc. AISTATS, 2017.",
    "[15] W3C / FIDO Alliance, \u201CWeb Authentication (WebAuthn): An API for accessing Public Key Credentials.\u201D [Online]. Available: https://www.w3.org/TR/webauthn/",
    "[16] Bosch Sensortec, \u201CBME280 Combined Humidity and Pressure Sensor Datasheet.\u201D [Online]. Available: https://www.bosch-sensortec.com/",
    "[17] Goodix, \u201CGT911 Capacitive Touch Panel Controller Datasheet.\u201D [Online]. Available: https://www.goodix.com/",
    "[18] Sitronix Technology, \u201CST7796 320x480 TFT Controller Datasheet.\u201D [Online]. Available: https://www.sitronix.com.tw/",
  ].map(txt => new Paragraph({
    alignment: AlignmentType.LEFT,
    indent: { left: 420, hanging: 420 },
    spacing: { line: 312, after: 60 },
    children: [en(txt, { size: 21 })],
  })),
];

// ---- English abstract + English title page (LTR) ----
function ENP(text, opts = {}) {
  return new Paragraph({
    alignment: AlignmentType.JUSTIFIED,
    spacing: { line: 360, after: 120 },
    children: [en(text, { size: 24 })],
  });
}

const abstractEN = [
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before: 200, after: 300, line: Math.ceil(16 * 23), lineRule: "atLeast" },
    children: [en("Abstract", { size: 32, bold: true })],
  }),
  ENP("In this project, a complete Internet of Things (IoT) smart-home system was designed and implemented with a distinctive focus on bringing artificial intelligence to low-power edge devices and reusing the hardware the user already owns. Face recognition \u2014 the authentication mechanism of the system \u2014 runs entirely on an ESP32-S3 microcontroller: the camera of the user's smartphone, reached by scanning a QR code shown on the device's touch LCD, acts as the image sensor, so no dedicated camera module is required and the edge hardware is minimized."),
  ENP("The supporting infrastructure is fully local: an MQTT broker (Mosquitto) and the Home Assistant platform run as Docker containers on a home server. The system therefore operates with nothing more than the home modem, survives both international and domestic Internet outages, and even when the global network is available, no cloud service is required \u2014 user data stays inside the home network, preserving privacy and keeping network latency minimal. Four parallel user interfaces are provided: the on-device touch display (LVGL), a mobile web panel opened via QR scan, the Home Assistant web dashboard, and the Home Assistant companion app, all fed by a single source of state on the device."),
  ENP("Beyond vision, a lightweight behavior-learning agent (two logistic-regression models with offline pre-training at 94.2% / 93.6% validation accuracy and on-chip online SGD) learns user habits for lighting and fan control in a shadow-then-auto lifecycle; the door lock is deliberately never under model control. A three-layer face-based attendance system (device outbox \u2192 local FastAPI/SQLite server \u2192 Jalali-calendar Google Sheets archive and Bale messenger notifications) was also implemented and deployed. Reliability mechanisms \u2014 task watchdogs, a hardware-reset watchdog for the touch controller, and network-aware Wi-Fi/MQTT reconnection \u2014 were validated through end-to-end scenario testing. The project served as a practical rehearsal of the full breadth of undergraduate engineering education and established a stable platform for future research, including federated learning on microcontrollers."),
  new Paragraph({
    alignment: AlignmentType.JUSTIFIED,
    spacing: { before: 200, line: 360 },
    children: [
      en("Keywords: ", { size: 24, bold: true }),
      en("Internet of Things, Edge AI, TinyML, ESP32-S3, Face Recognition, MQTT, Home Assistant, Docker, FreeRTOS, LVGL, Privacy, Local-first", { size: 24 }),
    ],
  }),
];

function ENTitleLine(text, size, bold = false, before = 0, after = 200) {
  return new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before, after, line: Math.ceil((size / 2) * 23), lineRule: "atLeast" },
    children: [en(text, { size, bold })],
  });
}

const titleEN = [
  ENTitleLine("Department of Electrical and Computer Engineering", 28, false, 2200, 500),
  ENTitleLine("Design and Implementation of a Smart Home IoT System", 32, true, 800, 100),
  ENTitleLine("with On-Device Edge AI Face Authentication", 32, true, 0, 100),
  ENTitleLine("and a Fully Local, Cloud-Free Infrastructure", 32, true, 0, 700),
  ENTitleLine("Undergraduate Specialized Project", 26, false, 600, 500),
  ENTitleLine("Supervisor:", 24, true, 800, 60),
  ENTitleLine("Dr. Amir Khorsandi", 26, false, 0, 400),
  ENTitleLine("Student:", 24, true, 300, 60),
  ENTitleLine("Mohammad Soroush Rabiei", 26, false, 0, 900),
  ENTitleLine("September 2026", 26, false, 500, 0),
];

module.exports = { ch10, ch11, refs, abstractEN, titleEN };
