# ربات بله — کنترل + اعلان‌ها + دیتابیس رویدادها

دو نقش در یک سرویس مستقل:

1. **کنترل** چراغ، فن و درب از داخل بله — عمداً **بدون MQTT**؛ همه‌ی فرمان‌ها مستقیم روی HTTPS خود برد می‌روند، پس با بروکرِ قطع هم کار می‌کنند.
2. **مرکز رویداد و اعلان**: برد رویدادهای درب (موفق/ناموفق) و ورود/خروج را به `POST /api/event` همین سرویس می‌فرستد؛ هر رویداد در SQLite ثبت (لاگ امنیتی/تحلیل) و به بله اعلام می‌شود.

تقسیم کار سرورها:

| سرور | مسئولیت |
|---|---|
| `server/attendance` | فقط حضور و غیاب: دیتابیس + Google Sheets |
| `server/bale_bot` (همین) | کنترل، همه‌ی اعلان‌های بله، لاگ امنیتی، داده‌ی تحلیل |

```
بله (چت شما) ──polling──▶ tapi.bale.ai ◀──long poll── bot.py ◀──POST /api/event── برد ESP32
                                                             │                      │
                                                             ├─HTTPS /api/light|fan|lock|status
                                                             └─SQLite (events + snapshots) هر ۱۰ دقیقه snapshot
```

## رویدادها (واژگان واحد با MQTT/دیتابیس)

| رویداد | معنی | اعلان بله |
|---|---|---|
| `door_unlocked_by_face` | باز شدن درب با چهره | ✅ |
| `door_unlocked_by_code` | باز شدن درب با رمز (کلیدواژه/LCD، داشبورد، ربات) | ✅ |
| `face_not_recognized` | چهره‌ی ناشناس | ✅ ⛔ |
| `wrong_code_entered` | رمز اشتباه | ✅ ⛔ |
| `attendance_in` / `attendance_out` | ورود/خروج | ✅ |

هر رکورد شامل `source` است: `face` / `keypad` / `http` (داشبورد) / `bot` / `manual`.

## دیتابیس (`data/bale_bot.db` — SQLite)

- جدول `events`: همه‌ی رویدادهای دریافتی + زمان دریافت + منبع — پایه‌ی لاگ امنیتی.
- جدول `snapshots`: هر `BALE_SNAPSHOT_INTERVAL_S` ثانیه (پیش‌فرض ۶۰۰ = ۱۰ دقیقه) وضعیت کامل برد از `/api/status` (دما، رطوبت، فشار، نور، حضور، چراغ/فن/درب، احتمال‌های ML) — برای تحلیل داده‌ی آینده.

نمایش سریع:
```bash
.venv/Scripts/python -c "import sqlite3;c=sqlite3.connect('data/bale_bot.db');print(list(c.execute('SELECT * FROM events ORDER BY id DESC LIMIT 10')))"
```

## دستورهای ربات

| دستور | کار |
|---|---|
| `/start` ، `/help` | راهنما + منوی دکمه‌ای |
| `/status` | وضعیت کامل برد (چراغ/فن/درب/سنسورها/ML) |
| `/open` | باز کردن درب — رمز را در چت بفرستید (خودِ رمز تأیید است) |
| `/light_on` ، `/light_off` | چراغ روشن / خاموش |
| `/fan_on` ، `/fan_off` | فن روشن / خاموش |
| `/light on\|off` ، `/fan on\|off` | شکل متنی همان دو فرمان |
| `/cancel` | لغو عملیات درب |

منوی دکمه‌ی پایین چت هم با `setMyCommands` هنگام استارت ثبت می‌شود.

## راه‌اندازی

۱. `.env` را از `.env.example` بسازید:

- `BALE_TOKEN` — توکن ربات (مشترک با اعلان‌ها؛ در سرور attendance دیگر لازم نیست).
- `BALE_ALLOWED_CHATS` — chat_id های مجاز (خالی = حالت یادگیری؛ chat_id در لاگ چاپ می‌شود و هیچ کنترلی انجام نمی‌شود).
- `BOARD_URL` — آدرس HTTPS برد (`https://IP` — بدون scheme هم قبول می‌شود).
- `BALE_BOT_SECRET` — secret دریافتی رویدادها؛ **باید همان `ATTENDANCE_SECRET` سرور attendance باشد** (برد یک secret برای هر دو سرور می‌فرستد).
- `BALE_EVENT_PORT` — پورت دریافت رویداد (پیش‌فرض 8001).
- `BALE_SNAPSHOT_INTERVAL_S` — فاصله‌ی snapshot تحلیلی (پیش‌فرض 600).

۲. اجرا:

```bash
# محلی
python -m venv .venv && .venv/Scripts/activate
pip install -r requirements.txt
python bot.py

# یا داکر (Docker Hub از ایران ۴۰۳ می‌دهد؛ Dockerfile از میرور arvancloud می‌کشد)
docker compose up -d --build
```

۳. روی برد (بعد از فلش فریمور جدید): تنظیمات → بخش Servers → «Bale bot server» را بگذارید `http://<IP-این-کامپیوتر>:8001` و ذخیره کنید. برد رویدادها را به این آدرس می‌فرستد (صف NVS هم دارد؛ اگر سرور قطع باشد بعداً می‌فرستد).

## نکات امنیتی

- فقط چت‌های `BALE_ALLOWED_CHATS` حق کنترل دارند.
- رمز درب در تاریخچه‌ی چت می‌ماند — بعد از استفاده پاکش کنید.
- گواهی برد self-signed است و `verify` خاموش می‌شود؛ ترافیک داخل LAN می‌ماند.
- رویدادها با secret گیت می‌شوند؛ پورت 8001 فقط روی LAN باز است و هرگز port-forward نشود.

## عیب‌یابی

- «برد در دسترس نیست» → IP برد عوض شده (هات‌اسپات!)؛ `BOARD_URL` را به‌روز کنید و `docker compose up -d --force-recreate` بزنید (restart فایل env را دوباره نمی‌خواند).
- اعلان رویداد نمی‌آید → لاگ کانتینر: `event stored` دیده می‌شود؟ اگر نه، Bale URL روی برد را چک کنید؛ اگر بله، مشکل ارسال بله است (لاگ sendMessage).
- دکمه‌ها جواب نمی‌دهند ولی متن کار می‌کند → از دستورهای متنی استفاده کنید.
