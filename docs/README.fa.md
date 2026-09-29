<div align="center">

[🇬🇧 English](../README.md) · **🇮🇷 فارسی**

# خانه هوشمند ESP32-S3

سیستم کنترل خانه‌ی هوشمند روی برد **ESP32-S3-DevKitC-1 (N16R8)**: نمایشگر LCD لمسی
(ST7796 + GT911) با رابط LVGL، باز کردن درب با تشخیص چهره، مدل ML رفتاری برای
کنترل خودکار چراغ و فن، وب‌داشبورد (HTTPS)، حضور و غیاب با QR و دوربین گوشی،
ربات کنترل بله، سنسور BME280 و یکپارچگی با Home Assistant.

</div>

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
