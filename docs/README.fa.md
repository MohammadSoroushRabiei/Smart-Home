<div align="center">

[🇬🇧 English](../README.md) · **🇮🇷 فارسی**

# خانه هوشمند ESP32-S3

سیستم کنترل خانه‌ی هوشمند روی برد **ESP32-S3-DevKitC-1 (N16R8)**: نمایشگر LCD لمسی
(ST7796 + GT911) با رابط LVGL، باز کردن درب با تشخیص چهره، مدل ML رفتاری برای
کنترل خودکار چراغ و فن، وب‌داشبورد (HTTPS)، حضور و غیاب با QR و دوربین گوشی،
ربات کنترل بله، سنسور BME280 و یکپارچگی با Home Assistant.

<img src="../presentation/Images/Board%20Image.jpg" width="480" alt="نمونه‌ی واقعی روی میز کار — برد ESP32-S3، داشبورد LVGL روی LCD و اپ Home Assistant روی گوشی"/>

*نمونه‌ی واقعی روی میز کار — گره‌ی لبه‌ی ESP32-S3 با داشبورد LVGL روی LCD،
و اپ همراه Home Assistant به‌صورت زنده روی گوشی.*

</div>

## 📸 تصاویر واقعی و ویدیوهای اجرا

هیچ‌کدام از این‌ها رندر یا ماکت نیست — همه‌چیز از سیستم در حال اجرا ضبط شده است:
برد فیزیکی، LCD واقعی، کانتینرهای مستقر و اپلیکیشن‌ها. ویدیوهای تست همین‌جا در
صفحه پخش می‌شوند.

### 🖥️ رابط کاربری روی خود دستگاه — اسکرین‌شات از LCD واقعی

| | | |
|:---:|:---:|:---:|
| <img src="../presentation/Images/LCD%20UI/Main%20Screen.png" width="300" alt="صفحه‌ی اصلی — وضعیت وای‌فای، دکمه‌های حضور و وب‌داشبورد، کاشی‌های چراغ/فن/ML و وضعیت محیط"/> | <img src="../presentation/Images/LCD%20UI/Unlock%20Screen.png" width="300" alt="صفحه‌ی باز کردن درب — تشخیص چهره و کنترل دستی"/> | <img src="../presentation/Images/LCD%20UI/Wifi%20List.png" width="300" alt="فهرست وای‌فای — اسکن و انتخاب شبکه روی خود دستگاه"/> |
| <img src="../presentation/Images/LCD%20UI/Wifi%20Enter%20Password.png" width="300" alt="ورود رمز وای‌فای با کیبورد روی صفحه"/> | <img src="../presentation/Images/LCD%20UI/Mqtt%20Setting.png" width="300" alt="پیکربندی بروکر MQTT روی دستگاه"/> | <img src="../presentation/Images/LCD%20UI/Setting%20Screen.png" width="300" alt="تنظیمات — رمزها، روشنایی و گزینه‌های سیستم"/> |
| <img src="../presentation/Images/LCD%20UI/Faces%20List.png" width="300" alt="فهرست چهره‌ها — مدیریت چهره‌های ثبت‌شده روی دستگاه"/> | <img src="../presentation/Images/LCD%20UI/Attend%20QR%20Code.png" width="300" alt="توکن QR حضور و غیاب با شمارش معکوس"/> | <img src="../presentation/Images/LCD%20UI/Web%20QR%20Code.png" width="300" alt="QR باز کردن وب‌داشبورد روی هر گوشی"/> |

*داشبورد اصلی · باز کردن با چهره · اسکن و اتصال وای‌فای با کیبورد روی صفحه ·
پیکربندی MQTT · تنظیمات و رمزها · ثبت چهره · QR دو-حالته‌ی حضور و غیاب ·
QR باز کردن وب‌داشبورد روی هر گوشی.*

*تست زنده روی نمایشگر واقعی — لمس، باز کردن درب، سنسورها (۳۸ ثانیه):*

<div align="center">

<video controls width="360" src="https://github.com/user-attachments/assets/3fca52ed-e168-4575-a4f6-300e99fccbfe"></video>

</div>

### 🌐 وب‌داشبورد — سروِ سرور حضور و غیاب

<div align="center">

<img src="../presentation/Images/Web%20Dashboard.png" width="360" alt="وب‌داشبورد — کاشی‌های دستگاه، قفل درب با رمز و تشخیص چهره، وضعیت زنده‌ی محیط، وضعیت عامل ML"/>

</div>

کاشی‌های دستگاه، قفل درب (رمز پین + تشخیص چهره)، وضعیت زنده‌ی محیط و حالت
SHADOW/AUTO عامل ML — روی LAN سرو می‌شود؛ هر گوشی با اسکن QR روی LCD به آن
می‌رسد.

*تست وب‌داشبورد — کلیدها، باز کردن درب، مقادیر زنده (۴۳ ثانیه):*

<div align="center">

<video controls width="300" src="https://github.com/user-attachments/assets/7c6d9063-2cdd-453c-a8b2-fb2aef746ac0"></video>

</div>

### 🏠 Home Assistant — اپ همراه روی گوشی واقعی

<div align="center">

<img src="../presentation/Images/Smart%20Home%20%E2%80%93%20Home%20Assistant_Dashboard.png" width="420" alt="داشبورد Home Assistant روی گوشی — کارت‌های محیط، کنترل دستگاه‌ها، وضعیت دسترسی و اتوماسیون‌ها"/>

</div>

کارت‌های محیط، کنترل چراغ / فن / عامل ML، وضعیت دسترسی و چهار اتوماسیون واقعی —
همه‌ی وضعیت‌ها دوطرفه از طریق MQTT رد و بدل می‌شوند.

*تست اپ موبایل — وضعیت زنده و کنترل (۵۸ ثانیه):*

<div align="center">

<video controls width="300" src="https://github.com/user-attachments/assets/a053339e-497f-4105-baa8-8d17881ceb84"></video>

</div>

### 🤖 ربات بله — کنترل از راه دور با چت

| | | |
|:---:|:---:|:---:|
| <img src="../presentation/Images/Bale_1.jpg" width="300" alt="ربات بله — کنترل با کیبورد پاسخ‌گو و تأیید فوری"/> | <img src="../presentation/Images/Bale_2.jpg" width="300" alt="ربات بله — باز کردن درب با رمز یک‌بارمصرف دو دقیقه‌ای"/> | <img src="../presentation/Images/Bale_3.jpg" width="300" alt="ربات بله — منوی کامل دستورهای اسلش"/> |

*کنترل با کیبورد پاسخ‌گو و تأیید فوری · باز کردن درب با رمز یک‌بارمصرف
۲ دقیقه‌ای · منوی کامل دستورهای اسلش.*

### 📅 حضور و غیاب — انتها به انتها

رکورد روی سرور محلی FastAPI/SQLite می‌نشیند، با تاریخ جلالی و نوع ورود/خروج در
Google Sheets بایگانی می‌شود و ربات بله خبر می‌دهد — همه در چند ثانیه.

<div align="center">

<img src="../presentation/Images/Google%20Sheet.png" width="480" alt="بایگانی حضور و غیاب در Google Sheet — تاریخ جلالی، ساعت، نام و ردیف‌های ورود/خروج"/>

</div>

*تست حضور و غیاب با چهره — اسکن QR → تطبیق چهره → ثبت رکورد + اعلان (۳۰ ثانیه):*

<div align="center">

<video controls width="300" src="https://github.com/user-attachments/assets/4096e28d-9eb9-4661-957a-d4dcb7e40f97"></video>

</div>

### 🐳 سرویس‌های محلی

<div align="center">

<img src="../presentation/Images/Docker.png" width="90%" alt="Docker Desktop — کانتینرهای سرور حضور و غیاب، Mosquitto، Home Assistant و ربات بله در حال اجرا"/>

</div>

کل زیرساخت پلتفرم روی یک سرور خانگی: سرویس حضور و غیاب، Mosquitto، Home
Assistant و ربات بله — هفت کانتینر با چند صد مگابایت رم.

## ساختار مخزن

```
Smart-Home/
├── main/                  فریمور اصلی ESP-IDF (کامپوننت main)
│   ├── display/           درایور LCD/تاچ + تمام صفحات LVGL
│   └── certs/             گواهی self-signed برای HTTPS (امبد در باینری)
├── demo/                  دموی ارائه روی PC: همان منطق فریمور + سخت‌افزار مجازی SDL2
├── server/                سرویس‌های سمت رایانه (Docker)
│   ├── attendance/        سرور حضور و غیاب (FastAPI + SQLite + Google Sheets + بله)
│   └── bale_bot/          ربات کنترل و اعلان بله (FastAPI + SQLite)
├── homeassistant/         docker-compose دراپر HA + پیکربندی Mosquitto
├── ml/                    خط لوله یادگیری ماشین: تولید داده → آموزش → وزن‌های C
├── tools/                 ابزارهای جانبی (serial_peek.py، نوت‌بوک تست API چهره)
├── docs/                  مستندات و مراجع
│   ├── hardware/          دیتاشیت‌های ESP32-S3 و LCD + جدول پین‌مپینگ
│   ├── proposal/          پروپوزال پروژه (docx/pdf)
│   ├── attendance/        راهنمای راه‌اندازی حضور و غیاب + اسکریپت Google Sheets
│   ├── ml-design.md       طراحی سیستم ML
│   └── roadmap.md         نقشه‌ی راه پروژه
├── report/                گزارش پروژه (تولید docx/pdf با generate.js)
├── presentation/          اسلایدهای دفاع (تولید pptx با gen_deck.js)
├── CMakeLists.txt         پروژه‌ی ESP-IDF 6.0.2
├── partitions.csv         جدول پارتیشن‌های فلش
└── sdkconfig.defaults     پیکربندی پیش‌فرض بیلد
```

## ساخت و فلش فریمور

نیازمند [ESP-IDF 6.0.2](https://docs.espressif.com/projects/esp-idf/) (نصب با EIM) —
دستورات از ریشه‌ی مخزن:

```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

جزئیات پین‌ها و پین‌های آزاد/اشغال‌شده: [`hardware/pin-mapping.md`](hardware/pin-mapping.md)

## دموی رومیزی (اختیاری)

همان منطق و UI فریمور روی PC با SDL2 (مستقل از بیلد فریمور):

```bash
cmake -S demo -B demo/build
cmake --build demo/build
./demo/build/smartdemo
```

## سرویس‌ها

- حضور و غیاب و ربات بله هرکدام `docker-compose.yml` دارند؛ راهنما:
  [`attendance/README.md`](attendance/README.md) و [`../server/bale_bot/README.md`](../server/bale_bot/README.md)
- بازتولید داشبورد/اتوماسیون‌های Home Assistant: [`../homeassistant/`](../homeassistant/)

## یادگیری ماشین

```bash
python ml/generate_data.py   # داده‌ی فرضی ۳۰ روز
python ml/train.py           # آموزش و تولید main/ml_model_weights.h
```

طراحی: [`ml-design.md`](ml-design.md) — بعد از تغییر وزن‌ها فریمور باید دوباره بیلد شود.

## اسناد و ارائه

- گزارش پروژه: `../report/generate.js` (Node) → خروجی `../report/SmartHome-Project-Report.docx`
- اسلایدهای دفاع: `../presentation/gen_deck.js` (Node) → خروجی `../presentation/SmartHome-Defense.pptx`
