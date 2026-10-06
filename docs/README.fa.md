<div align="center">

[🇬🇧 English](../README.md) · **🇮🇷 فارسی** · [🇨🇳 中文](README.zh.md)

# 🏠 خانه هوشمند — اتوماسیون خانگی اول-لبه روی ESP32-S3

**پلتفرمی خودمیزبان برای خانه هوشمند با تشخیص چهره روی خود دستگاه، عامل رفتاری
خودآموخته روی تراشه و پشته‌ی سرویس‌های کاملاً لوکال — بدون ابر.**

[![MCU](https://img.shields.io/badge/ESP32--S3-N16R8-E7352C?logo=espressif&logoColor=white)](hardware/pin-mapping.md)
[![Framework](https://img.shields.io/badge/ESP--IDF-6.0.2-3C5CA8?logo=espressif&logoColor=white)](https://docs.espressif.com/projects/esp-idf/)
[![RTOS](https://img.shields.io/badge/FreeRTOS-dual--core-2B6CB0)](#)
[![UI](https://img.shields.io/badge/LVGL-9.5-18BC9C)](#)
[![C](https://img.shields.io/badge/C-11-555555?logo=c&logoColor=white)](#)
[![C++](https://img.shields.io/badge/C%2B%2B-ESP--DL-00599C?logo=cplusplus&logoColor=white)](#)

[![MQTT](https://img.shields.io/badge/MQTT-Mosquitto-660066?logo=mqtt&logoColor=white)](#)
[![Home Assistant](https://img.shields.io/badge/Home%20Assistant-integration-41BDF5?logo=home-assistant&logoColor=white)](#)
[![FastAPI](https://img.shields.io/badge/FastAPI-servers-009688?logo=fastapi&logoColor=white)](#)
[![Docker](https://img.shields.io/badge/Docker-deployment-2496ED?logo=docker&logoColor=white)](#)
[![Python](https://img.shields.io/badge/Python-3-3776AB?logo=python&logoColor=white)](#)
[![Google Sheets](https://img.shields.io/badge/Google%20Sheets-archive-34A853?logo=googlesheets&logoColor=white)](#)

<img src="../presentation/Images/Board%20Image.jpg" width="480" alt="نمونه‌ی واقعی روی میز کار — برد ESP32-S3، داشبورد LVGL روی LCD و اپ Home Assistant روی گوشی"/>

*نمونه‌ی واقعی روی میز کار — برد ESP32-S3 با داشبورد LVGL روی LCD، و اپ همراه
Home Assistant به‌صورت زنده روی گوشی.*

<img src="../report/figs/fig1_architecture.png" width="100%" alt="معماری سیستم — گوشی، گره لبه ESP32-S3، سرور خانگی و اینترنت اختیاری"/>

*تمام هوشمندی روی گره‌ی لبه اجرا می‌شود — گوشی فقط دوربینش را قرض می‌دهد و سرورها لوکال می‌مانند.*

</div>

## ✨ ویژگی‌های برجسته

- 🔓 **قفل درب با تشخیص چهره روی میکروکنترلر** — خط پردازش CNN با ESP-DL
  (آشکارسازی → استخراج بردار → تطبیق کسینوسی) کاملاً روی ESP32-S3 اجرا می‌شود؛
  گوشی فقط یک JPEG را از طریق HTTPS بارگذاری می‌کند و تصاویر محلی پردازش می‌شوند و هرگز ذخیره نمی‌شوند.
- 🧠 **عامل رفتاری که کاربرش را یاد می‌گیرد** — دو سر سیگموئید (چراغ، فن) با
  پیش‌آموزش آفلاین به دقت اعتبارسنجی **۹۴٫۲٪ / ۹۳٫۶٪**، که سپس با SGD از هر اقدام
  دستی شما، **روی خود تراشه** یادگیری را ادامه می‌دهند.
- 🖥️ **داشبورد لمسی کامل** — رابط LVGL 9 با ابعاد ۳۲۰×۴۸۰ شامل راه‌اندازی وای‌فای،
  پیکربندی MQTT، تنظیمات سیستم، حضور و غیاب و کنترل روشنایی، مستقیماً روی خود دستگاه.
- 📅 **حضور و غیاب اول-آفلاین** — توکن QR روی LCD، تطبیق چهره با دوربین گوشی،
  رکوردها در صف NVS صف می‌شوند و با FastAPI/SQLite، گوگل‌شیت (تقویم جلالی) و
  اعلان‌های بله همگام می‌شوند.
- 🏡 **Home Assistant دوطرفه** — وضعیت همه‌ی دستگاه‌ها از طریق MQTT منتشر و از
  داشبوردها و اتوماسیون‌های HA قابل کنترل است؛ ربات بله هم کنترل از راه دور از
  طریق چت با ۸ دستور، اعلان رخدادها و لاگ امنیتی اضافه می‌کند.
- 🛡️ **مهندسی‌شده برای روشن‌ماندن** — دو گذرگاه I²C، سگ‌نگهبان ریست سخت‌افزاری
  برای کنترلر لمس، سگ‌نگهبان وظایف، اتصال مجدد شبکه‌آگاه وای‌فای/MQTT و وضعیت
  ماندگار در NVS، قطعی‌ها را به‌خوبی تاب می‌آورند.
- 🔒 **حریم خصوصی با معماری** — همه‌ی سرویس‌های اصلی روی LAN خانگی اجرا می‌شوند؛
  اینترنت اختیاری است و فقط برای همگام‌سازی خروجی و اعلان‌ها استفاده می‌شود.

## 📸 اجرای واقعی — عکس‌ها و ویدیوها

هیچ‌کدام از این‌ها رندر یا ماکت نیست — همه‌چیز از سیستم در حال اجرا ضبط شده است:
برد فیزیکی، LCD واقعی، کانتینرهای مستقر و اپلیکیشن‌ها.

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
Assistant و ربات بله — چهار کانتینر با چند صد مگابایت رم.

## 🎬 هاب زنده‌ی تعاملی — کل سیستم در مرورگر شما

[![هاب زنده‌ی تعاملی — در مرورگر باز کنید](img/motion-hub.png)](https://mohammadsoroushrabiei.github.io/Smart-Home/presentation/motion-hub.html)

**[`presentation/motion-hub.html`](../presentation/motion-hub.html)** بازسازی
کاملی مستقل و بدون وابستگی از کل سیستم در حال اجراست. آن را در هر مرورگری باز
کنید — بدون بیلد، بدون سرور، بدون سخت‌افزار — و هر بخش از پلتفرم روی یک صفحه
زنده می‌شود:

- 🖥️ **LCD واقعی** — کیبورد کارا + QR باز کردن با چهره، اسکن و اتصال وای‌فای،
  پیکربندی MQTT، تنظیمات (QR ثبت چهره، مدیریت چهره‌ها، رمز درب/تنظیمات) و
  صفحه‌ی دومُد حضور و غیاب، دقیقاً مثل فرمور.
- 🌐 **وب‌داشبورد زنده** — کاشی‌های هر دستگاه و سه نمودار زنده؛ تب **Google
  Sheet** هر اسکن حضور را ثبت می‌کند (اسکن‌های فرد = ورود، زوج = خروج).
- 🏠 **Home Assistant** — پنل وب با چهار اتوماسیون واقعی از `automations.yaml`
  به‌همراه نمای اپ همراه؛ هر تغییر وضعیت دوطرفه است.
- 🤖 **ربات بله** — مجموعه‌ی کامل دستورها (`/status`، `/open` با رمز ۲ دقیقه‌ای،
  `/light_on`، `/fan_on` و…) با اعلان رخدادهای درب و حضور.
- 🧠 **عامل ML به‌صورت زنده** — دو سر لجستیک با وزن‌های آموزش‌دیده؛ هر اقدام دستی
  که کلیک می‌کنید یک گام واقعی SGD روی همان صفحه است.
- 🎙️ **دموی خودراهنما** — تور نورافکنی ۱۲ مرحله‌ای از همه‌ی موارد بالا.
- 🌍 **رابط سه‌زبانه** — English · فارسی · 中文، قابل تعویض از دکمه‌های نوار بالا.

**▶ باز کردن زنده:** <https://mohammadsoroushrabiei.github.io/Smart-Home/presentation/motion-hub.html>
— یا فایل `presentation/motion-hub.html` را دانلود و دوبارکلیک کنید؛ همه‌چیز
از جمله فونت‌ها در همان یک فایل جاسازی شده است.

## 🏗️ معماری

برد، گره‌ی لبه است: مالک رابط کاربری، موتور چهره، عامل ML و تمام ورودی/خروجی‌ها.
یک سرور خانگی Docker میزبان Mosquitto، Home Assistant و سرویس حضور و غیاب است.
همه‌چیز روی LAN وای‌فای محلی گفت‌وگو می‌کند — ابر، یک خط‌چین است.

<img src="../report/figs/fig7_block_diagram.png" width="100%" alt="بلوک‌دیاگرام فرمور — درون گره لبه ESP32-S3 و جانبی‌ها"/>

جریان داده‌ی زمان اجرا بین زیرسیستم‌های فرمور:

<img src="img/fig_runtime_flow.png" width="100%" alt="جریان داده‌ی زمان اجرا — رابط LVGL، وظیفه سنسور، عامل ML، سرور HTTPS و کلاینت MQTT حول app_state مشترک"/>

## 🧠 یادگیری ماشین روی دستگاه

مدلی عمداً کوچک با خط لوله‌ای جدی: ۱۲ ویژگی زمینه‌ای (زمان روز به‌صورت سیکلی،
تقویم، حضور، لوکس، دما، رطوبت) به دو سر سیگموئید می‌رسد — یکی برای هر دستگاه —
که روی دیتاست مصنوعی ۶٬۹۱۲ نمونه‌ای پیش‌آموزش دیده‌اند و سپس **روی خود تراشه**
پالایش می‌شوند: هر اقدام دستی یک نمونه‌ی آموزشی برای SGD برخط می‌شود که حداکثر
هر ۳۰ ثانیه یک‌بار در NVS ماندگار می‌شود.

<img src="../report/figs/fig_ml_model.png" width="100%" alt="طراحی ML — ویژگی‌ها، شبکه تک‌لایه، قاعده تصمیم، گیت ارتقا"/>

| | چراغ | فن |
|---|---|---|
| دقت اعتبارسنجی (پیش‌آموزش آفلاین) | **۹۴٫۲٪** | **۹۳٫۶٪** |
| یادگیری برخط | SGD روی تراشه (η = ۰٫۰۸) | SGD روی تراشه (η = ۰٫۰۸) |
| ماندگاری | فضای‌نام NVS `mlbrain` | فضای‌نام NVS `mlbrain` |

چرخه‌ی عمر ایمن‌گرا، عامل را صادق نگه می‌دارد: در فاز **SHADOW** شروع می‌کند
(پیش‌بینی می‌کند، در MQTT گزارش می‌کند، اقدام نمی‌کند) و تنها پس از پنجره‌ی
متحرکی با ≥ ۱۵ تصمیم و دقت ≥ ۸۵٪ به **AUTO** ارتقا می‌یابد — و اقدام دستی همیشه
و بلافاصله برنده است. **قفل در هرگز تحت کنترل مدل نیست.**

یادگیری عامل را از صفر ببینید — هر اقدام دستی وزن‌ها را می‌لرزاند، منحنی
احتمال شکل می‌گیرد و پس از برآورده‌شدن گیت ارتقا، حالت به AUTO تغییر می‌کند:

<img src="../report/figs/fig_ml_learning.gif" width="90%" alt="انیمیشن — یادگیری برخط رفتار کاربر، به‌روزرسانی وزن‌ها در هر اقدام دستی"/>

<details>
<summary>🔍 عمق بیشتر: گیت ارتقای SHADOW → AUTO</summary>
<br>
<img src="../report/figs/fig5_ml_agent.png" width="100%" alt="چرخه عمر عامل ML — فاز سایه، گیت ارتقا، حالت خودکار، یادگیری برخط"/>
</details>

## 🔓 تشخیص چهره و کنترل دسترسی

تمام خط پردازش روی برد است: مرورگر عکس را می‌گیرد، رمزگشای JPEG سخت‌افزاری آن را
به بافر RGB565 در PSRAM تبدیل می‌کند و CNN با ESP-DL بردار ویژگی‌ای تولید می‌کند
که با پایگاه چهره روی دستگاه تطبیق داده می‌شود — شباهت کسینوسی، آستانه ۰٫۷۰،
ثبت چندنمونه‌ای.

<img src="../report/figs/fig2_face_pipeline.png" width="100%" alt="خط پردازش تشخیص چهره — دوربین گوشی، بارگذاری HTTPS، رمزگشایی سخت‌افزاری JPEG، ESP-DL، تصمیم تطبیق"/>

- ثبت/ویرایش/حذف چهره از رابط دستگاه یا وب‌داشبورد (پشت رمز)
- لینک‌های ثبت‌چهره با توکن QR که ظرف ۳ دقیقه منقضی می‌شوند
- رخدادهای باز کردن با منبعشان (چهره، کیبورد، وب، ربات) لاگ و به‌صورت اعلان ارسال می‌شوند

## 📅 سیستم حضور و غیاب

پشته‌ی سه‌لایه‌ای با صفر وابستگی ابری: LCD توکن QR چرخان را نشان می‌دهد
(اعتبار ۱۰ دقیقه + شمارش معکوس)، گوشی کاربر صفحه‌ای را باز می‌کند و چهره را
تطبیق می‌دهد و رخدادها روی سرور محلی FastAPI/SQLite می‌نشینند — که آن‌ها را با
تاریخ‌های جلالی در Google Sheets بایگانی و ربات بله را خبر می‌کند. اگر سرور یا
اینترنت قطع باشد، رکوردها در صندوق‌خروج ۳۲ خانه‌ای NVS روی برد می‌مانند و بعداً
ارسال می‌شوند.

<img src="../report/figs/fig4_attendance.png" width="100%" alt="پشته حضور و غیاب — لایه دستگاه، لایه سرور محلی، تحویل اینترنتی اختیاری"/>

## 🧱 پشته فناوری

<img src="../report/figs/fig6_stack.png" width="100%" alt="پشته نرم‌افزاری — سخت‌افزار، ESP-IDF + FreeRTOS، کتابخانه‌ها، ماژول‌های کاربردی، زیرساخت محلی"/>

## 🔌 سخت‌افزار

| قطعه | مشخصات | رابط |
|---|---|---|
| MCU | ESP32-S3-DevKitC-1 (N16R8 — فلش ۱۶ مگابایت، PSRAM اکتال ۸ مگابایت) | — |
| نمایشگر | TFT ۳۲۰×۴۸۰ (ST7796)، موازی ۸ بیتی، بک‌لایت PWM | گذرگاه GPIO |
| لمس | کنترلر خازنی GT911 | I²C (گذرگاه مستقل) |
| محیط | BME280 (دما / رطوبت / فشار) + LDR (لوکس) | I²C / ADC |
| قفل | قفل برقی با رله | GPIO |
| متفرقه | LED وضعیت، دکمه فیزیکی، WS2812 روی برد | GPIO |

نقشه‌ی کامل پین‌ها با GPIOهای آزاد/اشغال: [`hardware/pin-mapping.md`](hardware/pin-mapping.md)

## 📁 ساختار مخزن

```
Smart-Home/
├── main/                  فرمور اصلی ESP-IDF (اپلیکیشن + display/ رابط کاربری + certs/)
├── demo/                  دموی PC — منطق و UI واقعی فرمور روی سخت‌افزار مجازی SDL2
├── server/
│   ├── attendance/        سرور حضور و غیاب (FastAPI + SQLite + Sheets + بله)
│   └── bale_bot/          ربات بله (FastAPI + SQLite)
├── homeassistant/         دراپ‌این docker-compose ها + پیکربندی Mosquitto
├── ml/                    خط لوله ML: داده مصنوعی → آموزش → هدر وزن‌های C
├── tools/                 ابزارهای سمت میزبان (نظاره‌گر لاگ سری، نوت‌بوک تست API چهره)
├── docs/                  دیتاشیت‌ها، نقشه پین، اسناد طراحی، پروپوزال، README فارسی
├── report/                گزارش پروژه (تولیدکننده docx/pdf + شکل‌ها)
├── presentation/          اسلایدهای دفاع (تولیدکننده pptx)
├── CMakeLists.txt         پروژه ESP-IDF 6.0.2
├── partitions.csv         جدول پارتیشن‌های فلش
└── sdkconfig.defaults     پیش‌فرض‌های بیلد
```

## 🚀 شروع به کار

**فرمور** (نیازمند [ESP-IDF 6.0.2](https://docs.espressif.com/projects/esp-idf/)):

```bash
git clone https://github.com/MohammadSoroushRabiei/Smart-Home.git
cd Smart-Home
idf.py set-target esp32s3
idf.py build flash monitor
```

**دموی رومیزی** — همان منطق و UI فرمور روی سخت‌افزار مجازی SDL2 (بدون نیاز به برد):

```bash
cmake -S demo -B demo/build && cmake --build demo/build
./demo/build/smartdemo
```

**سرویس‌ها** (Docker، از پوشه‌ی هر سرویس):

```bash
cd server/attendance && docker compose up -d --build
cd server/bale_bot    && docker compose up -d --build
```

**خط لوله ML** — بازتولید و آموزش مجدد مدل رفتاری:

```bash
python ml/generate_data.py   # دیتاست مصنوعی ۳۰ روزه
python ml/train.py           # می‌نویسد main/ml_model_weights.h
idf.py build                 # فرمور وزن‌های جدید را برمی‌دارد
```

## 📚 مستندات

- 📄 [گزارش پروژه (PDF، ۵۹ صفحه)](../report/render/SmartHome-Project-Report.pdf)
- 🔌 [نقشه پین و مرجع GPIOهای آزاد](hardware/pin-mapping.md)
- 🧠 [طراحی سیستم ML](ml-design.md)
- 📅 [راهنمای راه‌اندازی حضور و غیاب](attendance/README.md)
- 💬 [راهنمای ربات بله](../server/bale_bot/README.md)
- 🇬🇧 [English README](../README.md) · 🇨🇳 [中文说明](README.zh.md)

---

## مجوز

این پروژه اختصاصی است — **تمام حقوق محفوظ است**. بخش [LICENSE](../LICENSE) را
ببینید. بازاستفاده، بازتوزیع یا کار مشتق بدون اجازه‌ی کتبی مجاز نیست.
برای مکاتبه: mohammadsoroushrabiei@gmail.com

<div align="center">

<img src="proposal/logo_iut.png" width="64" alt="نشان دانشگاه صنعتی اصفهان"/><br>

**محمد سروش ربیعی** — پروژه تخصصی کارشناسی، [دانشگاه صنعتی اصفهان](https://www.iut.ac.ir/)

[![GitHub](https://img.shields.io/badge/GitHub-@MohammadSoroushRabiei-181717?logo=github&logoColor=white)](https://github.com/MohammadSoroushRabiei)

</div>
