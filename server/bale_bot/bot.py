#!/usr/bin/env python3
"""ربات کنترلی بله برای خانه هوشمند — سرویس مستقل از سرور attendance.

اعلان‌های ورود/خروج همچنان از سرور attendance می‌آیند (integrations.py همان
بات و همان چت را استفاده می‌کند)؛ این سرویس فقط «کنترل» است و عمداً از MQTT
استفاده نمی‌کند تا با قطع بودن بروکر هم کار کند:

- چراغ/فن: POST مستقیم روی HTTPS خود برد (/api/light/set و /api/fan/set)
- درب:     تأیید دوم با دکمه + رمزی که کاربر در چت می‌فرستد
           → POST /api/lock/unlock (رمز در هیچ فایلی ذخیره نمی‌شود؛
           مستقیم به برد می‌رود و در پاسخ ۴۰۳ برد یک ثانیه تأخیر ضد brute-force هست)
- وضعیت:   GET /api/status برد

گواهی برد self-signed است؛ تأیید گواهی عمداً خاموش می‌شود.
متغیرهای محیطی و راهنمای اجرا: README.md
"""
from __future__ import annotations

import json
import logging
import os
import sys
import time
from typing import Any

import requests
import urllib3

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
                         data={"password": password}, timeout=10)
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
    "/light on|off — چراغ\n"
    "/fan on|off — فن\n"
    "/cancel — لغو عملیات درب\n\n"
    "باز کردن درب: بعد از /open رمز را در چت بفرستید — خودِ رمز تأیید است."
)

# منوی دکمه‌ی «منو/دستورها» پایین چت (به‌جای فقط «شروع مجدد»)
BOT_COMMANDS = [
    {"command": "start", "description": "راهنما و منوی کنترل"},
    {"command": "status", "description": "وضعیت کامل خانه هوشمند"},
    {"command": "open", "description": "باز کردن درب با رمز"},
    {"command": "light", "description": "چراغ: on یا off"},
    {"command": "fan", "description": "فن: on یا off"},
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
