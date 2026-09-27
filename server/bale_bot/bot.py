#!/usr/bin/env python3
"""ربات کنترلی و مرکز رویدادهای بله — سرویس مستقل از سرور attendance.

تقسیم کار:
- سرور attendance: فقط حضور و غیاب (دیتابیس + Google Sheets).
- این سرویس: کنترل + اعلان‌های بله + دیتابیس (لاگ امنیتی + داده‌ی تحلیل).

- کنترل: چراغ/فن/درب/وضعیت مستقیم روی HTTPS خود برد — بدون MQTT، تا با
  قطع بودن بروکر هم کار کند.
- رویدادها: برد رویدادهای درب (موفق/ناموفق) و ورود/خروج را به
  POST /api/event همین سرویس می‌فرستد؛ همه در SQLite ثبت و به بله اعلام
  می‌شوند (لاگ امنیتی: باز شدن درب، رمز اشتباه، چهره‌ی ناشناس و …).
- تحلیل: هر BALE_SNAPSHOT_INTERVAL_S ثانیه وضعیت کامل برد از /api/status
  خوانده و در جدول snapshots ذخیره می‌شود (برای تحلیل داده‌ی آینده).

گواهی برد self-signed است؛ تأیید گواهی عمداً خاموش می‌شود.
متغیرهای محیطی و راهنمای اجرا: README.md
"""
from __future__ import annotations

import json
import logging
import os
import sqlite3
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any
from urllib.parse import parse_qs, urlparse

import requests
import urllib3

import jdate

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

log = logging.getLogger("bale_bot")


def _load_env_file() -> None:
    """بارگذاری .env کنار bot.py (مقادیر ست‌شده در محیط بازنویسی نمی‌شوند)."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".env")
    if not os.path.isfile(path):
        return
    try:
        with open(path, "r", encoding="utf-8-sig") as fh:
            for line in fh:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                key, _, value = line.partition("=")
                key = key.strip()
                value = value.strip().strip('"').strip("'")
                if key and key not in os.environ:
                    os.environ[key] = value
    except OSError:
        log.exception("Reading %s failed", path)


_load_env_file()

# ---------------------------------------------------------------------------
# کانفیگ از محیط
# ---------------------------------------------------------------------------

BALE_TOKEN = os.environ.get("BALE_TOKEN", "").strip()
ALLOWED_CHATS = {
    c.strip() for c in os.environ.get("BALE_ALLOWED_CHATS", "").split(",") if c.strip()
}
BOARD_URL = os.environ.get("BOARD_URL", "").strip()
if BOARD_URL and "://" not in BOARD_URL:
    BOARD_URL = "https://" + BOARD_URL  # کاربر ممکن است فقط IP بنویسد
BOARD_URL = BOARD_URL.rstrip("/")
POLL_TIMEOUT_S = int(os.environ.get("BALE_POLL_TIMEOUT_S", "25"))
_DEFAULT_OFFSET_FILE = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "bale_state.json")
OFFSET_FILE = os.environ.get("BALE_OFFSET_FILE", _DEFAULT_OFFSET_FILE)

# دریافت رویداد از برد + دیتابیس + snapshot تحلیلی
BALE_BOT_SECRET = os.environ.get("BALE_BOT_SECRET", "").strip()
BALE_EVENT_PORT = int(os.environ.get("BALE_EVENT_PORT", "8001"))
BALE_SNAPSHOT_INTERVAL_S = int(os.environ.get("BALE_SNAPSHOT_INTERVAL_S", "600"))
_DEFAULT_DB_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "data", "bale_bot.db")
DB_PATH = os.environ.get("BALE_DB_PATH", _DEFAULT_DB_PATH)

BALE_API = "https://tapi.bale.ai/bot{token}/{method}"

FLOW_TTL_S = 120  # مهلت هر مرحله از جریان باز کردن درب

http = requests.Session()
http.verify = False

# جریان باز کردن درب (یک کاربر در آن واحد کافی است)
_flow: dict[str, Any] | None = None  # {"chat_id","stage": confirm|password,"expires"}


# ---------------------------------------------------------------------------
# کلاینت API بله (تلگرام‌سازگار)
# ---------------------------------------------------------------------------

def bale_call(method: str, **payload: Any) -> Any:
    timeout = (10, POLL_TIMEOUT_S + 20) if method == "getUpdates" else (10, 20)
    resp = http.post(BALE_API.format(token=BALE_TOKEN, method=method),
                     json=payload, timeout=timeout)
    if resp.status_code != 200:
        raise RuntimeError(f"Bale API {method}: HTTP {resp.status_code} {resp.text[:200]}")
    data = resp.json()
    if not data.get("ok"):
        raise RuntimeError(f"Bale API {method} error: {data}")
    return data.get("result")


def send_message(chat_id: str | int, text: str, reply_markup: dict | None = None) -> None:
    payload: dict[str, Any] = {"chat_id": chat_id, "text": text}
    if reply_markup is not None:
        payload["reply_markup"] = reply_markup
    try:
        bale_call("sendMessage", **payload)
    except Exception:  # noqa: BLE001 - شکست ارسال نباید حلقه‌ی polling را بکشد
        log.exception("sendMessage to %s failed", chat_id)


def answer_callback(callback_id: str) -> None:
    try:
        bale_call("answerCallbackQuery", callback_query_id=callback_id)
    except Exception:  # noqa: BLE001 - فقط UX است، ضروری نیست
        log.debug("answerCallbackQuery failed", exc_info=True)


# ---------------------------------------------------------------------------
# کلاینت HTTPS برد
# ---------------------------------------------------------------------------

def board_problem() -> str | None:
    """اگر کنترل ممکن نیست، متن خطای فارسی؛ وگرنه None."""
    if not BOARD_URL:
        return "آدرس برد (BOARD_URL) در تنظیمات ربات تنظیم نشده است."
    return None


def board_set_device(kind: str, on: bool) -> tuple[bool, str]:
    """فرمان چراغ/فن. خروجی: (state واقعیِ گزارش‌شده‌ی برد، متن خطا)."""
    try:
        resp = http.post(f"{BOARD_URL}/api/{kind}/set",
                         data={"on": "1" if on else "0"}, timeout=8)
    except requests.RequestException as exc:
        return False, f"برد در دسترس نیست ({exc.__class__.__name__})."
    if resp.status_code != 200:
        return False, f"برد پاسخ HTTP {resp.status_code} داد."
    try:
        data = resp.json()
    except ValueError:
        return False, "پاسخ برد نامعتبر بود."
    return bool(data.get(kind)), ""


def board_unlock(password: str) -> tuple[int, str]:
    """باز کردن درب. خروجی: (کد HTTP، پیام نتیجه برای کاربر)."""
    try:
        resp = http.post(f"{BOARD_URL}/api/lock/unlock",
                         data={"password": password, "source": "bot"}, timeout=10)
    except requests.RequestException:
        return 0, "⚠️ برد در دسترس نیست (وای‌فای برد و BOARD_URL را چک کنید)."
    if resp.status_code == 200:
        return 200, "✅ درب باز شد.\n(بعد از ۵ ثانیه خودکار قفل می‌شود)"
    if resp.status_code == 403:
        return 403, "❌ رمز اشتباه است."
    return resp.status_code, f"⚠️ پاسخ غیرمنتظره از برد: HTTP {resp.status_code}"


def board_status() -> tuple[dict | None, str]:
    try:
        resp = http.get(f"{BOARD_URL}/api/status", timeout=8)
        resp.raise_for_status()
        return resp.json(), ""
    except requests.RequestException as exc:
        return None, (f"برد در دسترس نیست ({exc.__class__.__name__}). "
                      "آدرس BOARD_URL و وای‌فای برد را چک کنید.")
    except ValueError:
        return None, "پاسخ وضعیت برد نامعتبر بود."


def status_text(data: dict) -> str:
    sensor = data.get("sensor") or {}
    if sensor.get("valid"):
        env_line = (f"🌡 دما: {sensor.get('temperature', '—')}°C   "
                    f"💧 رطوبت: {sensor.get('humidity', '—')}%   "
                    f"🎈 فشار: {sensor.get('pressure', '—')} hPa")
    else:
        env_line = "🌡 سنسور محیط: داده ندارد"
    return "\n".join([
        "📊 وضعیت خانه هوشمند",
        f"💡 چراغ: {'روشن ✅' if data.get('light') else 'خاموش ⚪'}",
        f"🌀 فن: {'روشن ✅' if data.get('fan') else 'خاموش ⚪'}",
        f"🚪 درب: {'باز 🔓' if data.get('lock') else 'قفل 🔒'}",
        env_line,
        f"☀️ نور: {data.get('lux', '—')} lux",
        f"🏠 حضور: {'در خانه' if data.get('presence') else 'بیرون'}",
        f"🤖 عامل ML: {'خودکار (AUTO)' if (data.get('ml') or {}).get('auto') else 'مشاور (SHADOW)'}",
        f"📡 MQTT: {(data.get('mqtt') or {}).get('state', '—')} — کنترل از بله مستقل از MQTT کار می‌کند",
    ])


# ---------------------------------------------------------------------------
# دیتابیس: لاگ رویدادها (امنیتی) + snapshots تحلیل داده
# ---------------------------------------------------------------------------

def _db_connect() -> sqlite3.Connection:
    os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)
    return sqlite3.connect(DB_PATH, timeout=10)


def db_init() -> None:
    conn = _db_connect()
    try:
        conn.execute("PRAGMA journal_mode=WAL")
        with conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS events (
                    id          INTEGER PRIMARY KEY AUTOINCREMENT,
                    received_at TEXT NOT NULL,
                    event       TEXT NOT NULL,
                    name        TEXT NOT NULL DEFAULT '',
                    person_id   INTEGER NOT NULL DEFAULT 0,
                    similarity  INTEGER NOT NULL DEFAULT 0,
                    device_ts   TEXT NOT NULL DEFAULT '',
                    source      TEXT NOT NULL DEFAULT ''
                )""")
            conn.execute("""
                CREATE TABLE IF NOT EXISTS snapshots (
                    id          INTEGER PRIMARY KEY AUTOINCREMENT,
                    ts          TEXT NOT NULL,
                    temperature REAL, humidity REAL, pressure REAL,
                    lux         REAL, presence INTEGER,
                    light INTEGER, fan INTEGER, "lock" INTEGER,
                    ml_auto INTEGER, p_light REAL, p_fan REAL,
                    mqtt_state  TEXT
                )""")
    finally:
        conn.close()


def db_insert_event(event: str, name: str, person_id: int, similarity: int,
                    device_ts: str, source: str) -> None:
    conn = _db_connect()
    try:
        with conn:
            conn.execute(
                "INSERT INTO events (received_at, event, name, person_id,"
                " similarity, device_ts, source)"
                " VALUES (datetime('now','localtime'),?,?,?,?,?,?)",
                (event, name, person_id, similarity, device_ts, source))
    finally:
        conn.close()


def db_insert_snapshot(data: dict) -> None:
    sensor = data.get("sensor") or {}
    ml = data.get("ml") or {}
    conn = _db_connect()
    try:
        with conn:
            conn.execute(
                "INSERT INTO snapshots (ts, temperature, humidity, pressure,"
                " lux, presence, light, fan, \"lock\", ml_auto, p_light,"
                " p_fan, mqtt_state)"
                " VALUES (datetime('now','localtime'),?,?,?,?,?,?,?,?,?,?,?,?)",
                (sensor.get("temperature"), sensor.get("humidity"),
                 sensor.get("pressure"), data.get("lux"),
                 1 if data.get("presence") else 0,
                 1 if data.get("light") else 0, 1 if data.get("fan") else 0,
                 1 if data.get("lock") else 0, 1 if ml.get("auto") else 0,
                 ml.get("p_light"), ml.get("p_fan"),
                 (data.get("mqtt") or {}).get("state", "")))
    finally:
        conn.close()


def _snapshot_loop() -> None:
    while True:
        time.sleep(BALE_SNAPSHOT_INTERVAL_S)
        if not BOARD_URL:
            continue
        data, _error = board_status()
        if data is None:
            log.warning("Snapshot skipped - board not reachable")
            continue
        try:
            db_insert_snapshot(data)
            log.info("Snapshot stored")
        except sqlite3.Error:
            log.exception("Snapshot insert failed")


# ---------------------------------------------------------------------------
# دریافت رویداد از برد (HTTP) + اعلان بله
# ---------------------------------------------------------------------------

# متن اعلان هر رویداد - همان واژگانی که برد می‌فرستد
_EVENT_TEXT = {
    "door_unlocked_by_face": "🚪 باز شدن درب با چهره",
    "door_unlocked_by_code": "🔑 باز شدن درب با رمز",
    "face_not_recognized": "⛔ چهره‌ی ناشناس - دسترسی داده نشد",
    "wrong_code_entered": "⛔ رمز اشتباه - دسترسی داده نشد",
    "attendance_in": "🟢 ورود",
    "attendance_out": "🔴 خروج",
}


def notify_event(event: str, name: str, device_ts: str) -> None:
    title = _EVENT_TEXT.get(event, f"📣 {event}")
    who = f": {name}" if name else ""
    when = f"\n🕒 {jdate.to_jalali_str(device_ts)}" if device_ts else ""
    for chat_id in sorted(ALLOWED_CHATS):
        send_message(chat_id, f"{title}{who}{when}")


class _EventReceiver(BaseHTTPRequestHandler):
    """POST /api/event — همان قرارداد payload سرور attendance + فیلد source؛
    GET /health?secret=... — تست اتصال از برد/داشبورد."""

    def do_POST(self) -> None:
        if self.path != "/api/event":
            self._json(404, {"ok": False, "error": "not found"})
            return
        try:
            length = int(self.headers.get("Content-Length") or 0)
            body = json.loads(self.rfile.read(length) or b"{}")
        except (ValueError, json.JSONDecodeError):
            self._json(400, {"ok": False, "error": "bad json"})
            return
        if not BALE_BOT_SECRET or body.get("secret") != BALE_BOT_SECRET:
            self._json(403, {"ok": False, "error": "bad secret"})
            return

        event = str(body.get("event", ""))
        if event == "test":
            self._json(200, {"ok": True, "test": True})
            return
        if event not in _EVENT_TEXT:
            self._json(400, {"ok": False, "error": "unknown event"})
            return

        name = str(body.get("name", "")).strip()[:64]
        try:
            person_id = int(body.get("id", 0))
            similarity = int(body.get("similarity", 0))
        except (TypeError, ValueError):
            person_id, similarity = 0, 0
        device_ts = str(body.get("ts", ""))[:32]
        source = str(body.get("source", ""))[:16]

        db_insert_event(event, name, person_id, similarity, device_ts, source)
        log.info("event stored: %s name=%r source=%r ts=%r",
                 event, name, source, device_ts)
        notify_event(event, name, device_ts)
        self._json(200, {"ok": True})

    def do_GET(self) -> None:
        if self.path.startswith("/health"):
            qs = parse_qs(urlparse(self.path).query)
            if not BALE_BOT_SECRET or qs.get("secret", [""])[0] != BALE_BOT_SECRET:
                self._json(403, {"ok": False, "error": "bad secret"})
                return
            self._json(200, {"ok": True, "service": "bale-bot"})
            return
        self._json(404, {"ok": False, "error": "not found"})

    def _json(self, code: int, obj: dict) -> None:
        payload = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, fmt: str, *args: Any) -> None:
        log.debug("%s %s", self.address_string(), fmt % args)


# ---------------------------------------------------------------------------
# کیبوردها و متن‌ها
# ---------------------------------------------------------------------------

MENU = {"inline_keyboard": [
    [{"text": "💡 چراغ روشن", "callback_data": "light_on"},
     {"text": "💡 چراغ خاموش", "callback_data": "light_off"}],
    [{"text": "🌀 فن روشن", "callback_data": "fan_on"},
     {"text": "🌀 فن خاموش", "callback_data": "fan_off"}],
    [{"text": "🔓 باز کردن درب", "callback_data": "door_open"},
     {"text": "📊 وضعیت", "callback_data": "status"}],
]}

HELP_TEXT = (
    "🤖 ربات کنترل خانه هوشمند\n\n"
    "با دکمه‌های منو (یا دستورها) چراغ، فن و درب را کنترل کنید:\n"
    "/status — وضعیت کامل\n"
    "/open — باز کردن درب (با رمز)\n"
    "/light_on و /light_off — چراغ\n"
    "/fan_on و /fan_off — فن\n"
    "(شکل /light on یا /fan off هم کار می‌کند)\n"
    "/cancel — لغو عملیات درب\n\n"
    "باز کردن درب: بعد از /open رمز را در چت بفرستید — خودِ رمز تأیید است."
)

# منوی دکمه‌ی «منو/دستورها» پایین چت (به‌جای فقط «شروع مجدد»)
BOT_COMMANDS = [
    {"command": "start", "description": "راهنما و منوی کنترل"},
    {"command": "status", "description": "وضعیت کامل خانه هوشمند"},
    {"command": "open", "description": "باز کردن درب با رمز"},
    {"command": "light_on", "description": "چراغ روشن"},
    {"command": "light_off", "description": "چراغ خاموش"},
    {"command": "fan_on", "description": "فن روشن"},
    {"command": "fan_off", "description": "فن خاموش"},
    {"command": "cancel", "description": "لغو عملیات درب"},
]


# ---------------------------------------------------------------------------
# فرمان‌ها و جریان درب
# ---------------------------------------------------------------------------

def device_command(chat_id: str, kind: str, on: bool) -> None:
    problem = board_problem()
    if problem:
        send_message(chat_id, f"⚠️ {problem}")
        return
    actual, error = board_set_device(kind, on)
    if error:
        send_message(chat_id, f"⚠️ {error}")
        return
    label = "چراغ" if kind == "light" else "فن"
    if actual == on:
        send_message(chat_id, f"{label} {'روشن ✅' if on else 'خاموش ⚪'} شد.")
    else:
        send_message(chat_id,
                     "⚠️ فرمان ثبت شد ولی برد وضعیت متفاوتی گزارش می‌دهد "
                     "(شاید عامل ML آن را عوض کرده باشد). با /status چک کنید.")


def show_status(chat_id: str) -> None:
    problem = board_problem()
    if problem:
        send_message(chat_id, f"⚠️ {problem}")
        return
    data, error = board_status()
    if data is None:
        send_message(chat_id, f"⚠️ {error}")
    else:
        send_message(chat_id, status_text(data))


def start_door_flow(chat_id: str) -> None:
    global _flow
    problem = board_problem()
    if problem:
        send_message(chat_id, f"⚠️ {problem}")
        return
    _flow = {"chat_id": chat_id, "stage": "password", "expires": time.time() + FLOW_TTL_S}
    send_message(chat_id, "🔐 رمز درب را بفرستید (۲ دقیقه فرصت دارید).\n"
                          "خودِ رمز تأیید است — لغو: /cancel")


def door_cancelled(chat_id: str) -> None:
    global _flow
    _flow = None
    send_message(chat_id, "لغو شد.")


def handle_password(chat_id: str, password: str) -> None:
    global _flow
    _flow = None
    code, message = board_unlock(password)
    send_message(chat_id, message)
    if code == 200:
        send_message(chat_id, "💡 برای امنیت بیشتر می‌توانید پیام رمز را از چت پاک کنید.")


def handle_command(chat_id: str, text: str) -> None:
    parts = text.split()
    cmd = parts[0].lstrip("/").split("@")[0].lower()
    args = parts[1:]
    if cmd in ("start", "help"):
        send_message(chat_id, HELP_TEXT, MENU)
    elif cmd == "status":
        show_status(chat_id)
    elif cmd == "open":
        start_door_flow(chat_id)
    elif cmd == "cancel":
        door_cancelled(chat_id)
    elif cmd in ("light_on", "light_off", "fan_on", "fan_off"):
        kind, state = cmd.split("_")
        device_command(chat_id, kind, state == "on")
    elif cmd in ("light", "fan"):
        state = args[0].lower() if args else ""
        if state in ("on", "1", "روشن"):
            device_command(chat_id, cmd, True)
        elif state in ("off", "0", "خاموش"):
            device_command(chat_id, cmd, False)
        else:
            send_message(chat_id, f"مثال: /{cmd} on یا /{cmd} off")
    else:
        send_message(chat_id, "دستور ناشناخته است.", MENU)


# ---------------------------------------------------------------------------
# رسیدگی به آپدیت‌های بله
# ---------------------------------------------------------------------------

def handle_callback(query: dict) -> None:
    answer_callback(query.get("id", ""))
    message = query.get("message") or {}
    chat_id = str((message.get("chat") or {}).get("id", ""))
    if not chat_id:
        return
    data = query.get("data", "")
    if data == "light_on":
        device_command(chat_id, "light", True)
    elif data == "light_off":
        device_command(chat_id, "light", False)
    elif data == "fan_on":
        device_command(chat_id, "fan", True)
    elif data == "fan_off":
        device_command(chat_id, "fan", False)
    elif data == "door_open":
        start_door_flow(chat_id)
    elif data == "status":
        show_status(chat_id)
    else:
        log.warning("Unknown callback_data: %s", data)


def handle_message(message: dict) -> None:
    global _flow
    chat_id = str((message.get("chat") or {}).get("id", ""))
    text = (message.get("text") or "").strip()
    if not chat_id or not text:
        return

    # انقضای جریان درب
    if _flow and _flow["chat_id"] == chat_id and time.time() > _flow["expires"]:
        _flow = None
        send_message(chat_id, "⌛ مهلت جریان باز کردن درب تمام شد؛ دوباره تلاش کنید.")

    # مرحله‌ی رمز بر هر پیام متنی مقدم است (به‌جز دستورهای اسلش)
    if _flow and _flow["chat_id"] == chat_id and _flow["stage"] == "password":
        if text.startswith("/"):
            handle_command(chat_id, text)  # مثلاً /cancel
        else:
            handle_password(chat_id, text)
        return

    if text.startswith("/"):
        handle_command(chat_id, text)
    else:
        send_message(chat_id, "از دکمه‌های منو استفاده کنید 👇", MENU)


def extract_chat_id(update: dict) -> str | None:
    if "message" in update:
        chat = update["message"].get("chat") or {}
        return str(chat.get("id", "")) or None
    if "callback_query" in update:
        chat = (update["callback_query"].get("message") or {}).get("chat") or {}
        return str(chat.get("id", "")) or None
    return None


def handle_update(update: dict) -> None:
    chat_id = extract_chat_id(update)
    if chat_id is None:
        return
    if chat_id not in ALLOWED_CHATS:
        # بدون وایت‌لیست هیچ کنترلی انجام نمی‌شود؛ فقط chat_id گزارش می‌شود
        # (حالت یادگیری: chat_id را در BALE_ALLOWED_CHATS بگذارید)
        log.warning("Unauthorized chat_id=%s", chat_id)
        send_message(chat_id,
                     "⛔ این چت در لیست مجاز ربات نیست.\n"
                     f"chat_id این چت برای BALE_ALLOWED_CHATS:\n{chat_id}")
        return
    if "callback_query" in update:
        handle_callback(update["callback_query"])
    elif "message" in update:
        handle_message(update["message"])


# ---------------------------------------------------------------------------
# offset و حلقه‌ی polling
# ---------------------------------------------------------------------------

def load_offset() -> int:
    try:
        with open(OFFSET_FILE, "r", encoding="utf-8") as fh:
            return int(json.load(fh).get("offset", 0))
    except (OSError, ValueError):
        return 0


def save_offset(offset: int) -> None:
    tmp = OFFSET_FILE + ".tmp"
    try:
        with open(tmp, "w", encoding="utf-8") as fh:
            json.dump({"offset": offset}, fh)
        os.replace(tmp, OFFSET_FILE)
    except OSError:
        log.exception("Saving offset failed")


def main() -> None:
    logging.basicConfig(
        level=os.environ.get("BALE_LOG_LEVEL", "INFO").upper(),
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )
    if not BALE_TOKEN:
        sys.exit("BALE_TOKEN is not set")
    if not ALLOWED_CHATS:
        log.warning("BALE_ALLOWED_CHATS is empty - the bot will only report chat ids "
                    "and refuse every command until a chat is whitelisted")
    log.info("Bale control bot starting (board=%s, allowed_chats=%d, MQTT not used)",
             BOARD_URL or "<unset>", len(ALLOWED_CHATS))

    db_init()
    try:
        receiver = ThreadingHTTPServer(("0.0.0.0", BALE_EVENT_PORT), _EventReceiver)
        threading.Thread(target=receiver.serve_forever, name="event-receiver",
                         daemon=True).start()
        log.info("Event receiver listening on 0.0.0.0:%d (db=%s, secret=%s)",
                 BALE_EVENT_PORT, DB_PATH, "set" if BALE_BOT_SECRET else "NOT SET")
    except OSError:
        log.exception("Failed to start event receiver on port %d", BALE_EVENT_PORT)
    threading.Thread(target=_snapshot_loop, name="snapshots",
                     daemon=True).start()
    log.info("Analysis snapshots every %d s", BALE_SNAPSHOT_INTERVAL_S)

    try:
        bale_call("setMyCommands", commands=BOT_COMMANDS)
        log.info("Bot command menu registered (%d commands)", len(BOT_COMMANDS))
    except Exception:  # noqa: BLE001 - منوی دستورها حیاتی نیست
        log.warning("setMyCommands failed - menu button falls back to /start only",
                    exc_info=True)

    for chat_id in sorted(ALLOWED_CHATS):
        send_message(chat_id, "🤖 ربات کنترل خانه هوشمند آنلاین شد.", MENU)

    offset = load_offset()
    while True:
        try:
            updates = bale_call("getUpdates", offset=offset, timeout=POLL_TIMEOUT_S) or []
        except requests.RequestException as exc:
            log.warning("getUpdates failed: %s", exc)
            time.sleep(5)
            continue
        except RuntimeError as exc:
            log.error("%s", exc)
            time.sleep(5)
            continue
        for update in updates:
            new_offset = update.get("update_id")
            if isinstance(new_offset, int):
                offset = new_offset + 1
            try:
                handle_update(update)
            except Exception:  # noqa: BLE001 - یک آپدیت بد نباید کل ربات را بیندازد
                log.exception("Update handling failed")
        if updates:
            save_offset(offset)


if __name__ == "__main__":
    main()
