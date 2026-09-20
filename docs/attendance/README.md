# حضور و غیاب — راهنمای راه‌اندازی

ثبت ورود/خروج کارکنان با چهره از دوربین موبایل + اعلان‌های بله، با معماری
سه‌لایه‌ای مستقل از Home Assistant و Mosquitto:

```
ESP32 ──HTTP+secret──> سرور لوکال (Docker) ──> SQLite (دائمی)
                                          ├──> Google Sheets (بایگانی)
                                          └──> ربات بله (اعلان)
```

- برد هیچ credential گوگل/بله‌ای ندارد؛ فقط URL سرور و یک secret مشترک.
- اگر سرور خاموش باشد → رکوردها در صف NVS روی برد (۳۲ رکورد) می‌مانند و
  بعد از ارسال موفق، اسلاتشان آزاد می‌شود.
- اگر اینترنت قطع باشد → رکوردها در SQLite ذخیره و بعداً به گوگل/بله می‌روند.
- اعلان‌های بله: ورود فرد، خروج فرد، باز شدن درب با چهره، باز شدن درب با رمز.
- **شیت گوگل فقط رکوردهای حضور و غیاب را می‌گیرد** (رویدادهای درب فقط در
  SQLite و بله).
- تاریخ‌ها در شیت و پیام‌های بله **شمسی** هستند؛ در SQLite به همان شکل
  میلادیِ دستگاه ذخیره می‌شوند.

## بخش ۱ — سمت برد (فریمور)

از دکمه‌ی **Attend** روی صفحه‌ی اصلی LCD:
- QR توکن‌دار حضور (۱۰ دقیقه، تازه‌سازی خودکار) → کارمند اسکن می‌کند →
  صفحه‌ی فارسی → «ثبت ورود» / «ثبت خروج» با دوربین گوشی.
- دکمه‌ی **Enroll New Face** همان صفحه اول **رمز تنظیمات** می‌خواهد، بعد
  QR ثبت‌نام (۳ دقیقه) نشان داده می‌شود. ویرایش/حذف افراد هم از داشبورد →
  System Settings → Enrolled Faces (پشت رمز).

تنظیمات برد: داشبورد وب → System Settings → کارت **Attendance Server**:
URL سرور (`http://<IP-لپ‌تاپ>:8000`)، Secret، فعال‌سازی، دکمه‌ی **Test Connection**.
«Queued records» تعداد رکوردهای معوق روی برد است.

## بخش ۲ — سرور لوکال روی داکر

```
server/attendance/
├── app.py              # FastAPI: /api/event، /health، /stats
├── db.py               # SQLite + outbox (google_synced / bale_synced)
├── integrations.py     # گوگل شیت (Apps Script webhook) + ربات بله
├── jdate.py            # تبدیل میلادی → شمسی (بدون وابستگی)
├── Dockerfile
├── docker-compose.yml
├── requirements.txt
└── .env.example        # کپی به .env و پر کردن مقادیر
```

راه‌اندازی:

```bash
cd server/attendance
cp .env.example .env     # بعداً مقادیر را پر کن
docker compose up -d --build
```

اگر می‌خواهی کنار HA و Mosquitto باشد، فقط بلوک `services:` از
`docker-compose.yml` را به compose اصلی‌ات اضافه کن (env_file و volume یادت نرود).

### تنظیم Google Sheets از طریق Apps Script (اختیاری ولی پیشنهادی)

نیازی به Google Cloud Console و service account نیست — فقط یک حساب گوگل معمولی:

1. یک Google Sheet جدید بساز (مثلاً «حضور و غیاب»).
2. در شیت: **Extensions → Apps Script** → محتوای
   `docs/attendance/attendance-webhook.gs` را پیست کن.
3. در خط اول اسکریپت، `SECRET` را به یک عبارت تصادفی تغییر بده و همان را
   در `.env` سرور جلوی `GOOGLE_SCRIPT_SECRET` بگذار.
4. **Deploy → New deployment → Web app**:
   - Execute as: **Me**
   - Who has access: **Anyone**
5. آدرس تولیدشده (شبیه `https://script.google.com/macros/s/…/exec`) را در
   `.env` سرور جلوی `GOOGLE_SCRIPT_URL` بگذار و کانتینر را ری‌استارت کن
   (`docker compose restart` یا `up -d`).

ردیف‌ها با هدر «تاریخ | ساعت | نام | نوع» خودکار ساخته می‌شوند (تاریخ شمسی، دو ستون مجزا؛ شباهت فقط در دیتابیس ذخیره می‌شود). هر بار که کد اسکریپت را عوض می‌کنی، Deploy → Manage deployments → edit → **New version** لازم است تا /exec جدید شود.

تست مستقیم webhook بدون برد:

```bash
curl -L -X POST "آدرس-/exec-اینجا" \
  -H "Content-Type: application/json" \
  -d '{"secret":"SECRET-اسکریپت","event":"test"}'
```

پاسخ `{"ok":true,"message":"test ok"}` یعنی سالم است (ردیف نمی‌نویسد).

### تنظیم ربات بله (اختیاری)

1. داخل بله، به **BotFather** پیام بده → `/newbot` → توکن را بردار → در
   `BALE_TOKEN` بگذار.
2. ربات را به گروه/چت مقصد اضافه کن؛ `BALE_CHAT_ID` گروه یا کاربر را بگذار
   (برای گروه: پیام به ربات بده و از
   `https://tapi.bale.ai/bot<TOKEN>/getUpdates` مقدار `chat.id` را بخوان).
3. اگر بله/گوگل ست نشده باشند، سرور همان‌طور که هست کار می‌کند (فقط SQLite)؛
   رکوردها تا کانفیگ شدن در outbox می‌مانند.

## بخش ۳ — تست

```bash
# سلامت سرور (باید {"ok":true,...} بدهد)
curl "http://<IP-سرور>:8000/health?secret=SECRET-شما"

# شبیه‌سازی رویداد ورود (بدون برد)
curl -X POST "http://<IP-سرور>:8000/api/event" \
  -H "Content-Type: application/json" \
  -d '{"secret":"SECRET-شما","event":"attendance_in","ts":"2026-09-20 09:15:00","name":"تست","id":1,"similarity":87}'

# آمار صف
curl "http://<IP-سرور>:8000/stats"
```

بعد برد: داشبورد → Attendance Server → URL و Secret → Save → **Test Connection**.

## نکات عملی

- **IP ثابت**: برای لپ‌تاپ/سرور در روتر رزرو DHCP کن؛ وگرنه با عوض شدن IP،
  برد آدرس را گم می‌کند.
- **لپ‌تاپ خاموش؟** رکوردهای آن بازه در صف NVS برد می‌مانند (۳۲ رکورد) و با
  بالا آمدن سرور ارسال می‌شوند. برای استقرار دائمی، سرور را روی ماشین
  همیشه‌روشن ببر — فقط کافی است volume و .env منتقل شود.
- **امنیت**: پورت 8000 را به بیرون اینترنت فوروارد نکن؛ فقط LAN. secret را
  حداقل ۲۰ کاراکتر تصادفی بگذار.
- ساعت رکورد از NTP دستگاه می‌آید؛ اگر دستگاه همگام نباشد ثبت حضور با خطای
  «ساعت دستگاه همگام نشده» رد می‌شود.
- ضداسپوفینگ/liveness نداریم (هم‌سطح سیستم درب) — عکس چاپی ممکن است فریب دهد.
