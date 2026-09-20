"""Integrations: Google Sheets (via Apps Script webhook) + Bale bot.

هر دو اختیاری‌اند؛ اگر متغیرهای محیطی مربوطه ست نشده باشند، تابع مربوطه
False برمی‌گرداند و رکورد در outbox می‌ماند تا روزی کانفیگ شد ارسال شود.

گوگل از طریق Apps Script کار می‌کند (نه service account) تا نیازی به دسترسی
Google Cloud Console نباشد: یک شیت بساز، اسکریپت docs/attendance/attendance-webhook.gs
را در آن Deploy کن و آدرس /exec را در GOOGLE_SCRIPT_URL بگذار.
"""
import logging
import os

import requests

import jdate

log = logging.getLogger("attendance.integrations")

# ---------------------------------------------------------------------
# Google Sheets — از طریق Apps Script webhook
# ---------------------------------------------------------------------

_BALE_API = "https://tapi.bale.ai/bot{token}/sendMessage"


def _script_config() -> tuple[str, str] | None:
    url = os.environ.get("GOOGLE_SCRIPT_URL", "").strip()
    if not url:
        return None
    # اگر secret مخصوص اسکریپت ست نشده، همان secret سرور استفاده می‌شود
    secret = os.environ.get("GOOGLE_SCRIPT_SECRET", "") or os.environ.get("ATTENDANCE_SECRET", "")
    return url, secret


def send_to_sheet(event: str, name: str, similarity: int, device_ts: str) -> bool:
    """یک ردیف به شیت اضافه می‌کند. True یعنی موفق؛ False یعنی رکورد pending می‌ماند."""
    cfg = _script_config()
    if cfg is None:
        return False
    url, secret = cfg

    jdate_str, jtime_str = jdate.to_jalali_parts(device_ts)
    # شباهت عمداً در شیت نمی‌آید (فقط SQLite)؛ امضای تابع با send_to_bale
    # یکسان می‌ماند تا worker هر دو را یکسان صدا بزند
    payload = {
        "secret": secret,
        "event": event,
        # تاریخ و ساعت شمسی، دو ستون مجزا (در SQLite میلادی می‌ماند)
        "date": jdate_str,
        "time": jtime_str,
        "name": name,
    }
    try:
        # ریدایرکت 302 گوگل با پیش‌فرض requests دنبال می‌شود
        resp = requests.post(url, json=payload, timeout=20)
        if resp.status_code != 200:
            log.error("Apps Script POST failed: HTTP %d %s",
                      resp.status_code, resp.text[:200])
            return False
        return True
    except Exception as exc:  # noqa: BLE001
        log.error("Apps Script POST error: %s", exc)
        return False


# ---------------------------------------------------------------------
# Bale bot (Telegram-compatible API)
# ---------------------------------------------------------------------

_EVENT_EMOJI = {
    "attendance_in": "\U0001F7E2 ورود",
    "attendance_out": "\U0001F534 خروج",
    "door_face": "\U0001F6AA باز شدن درب با چهره",
    "door_code": "\U0001F511 باز شدن درب با رمز",
}


def _bale_config() -> tuple[str, str] | None:
    token = os.environ.get("BALE_TOKEN", "")
    chat_id = os.environ.get("BALE_CHAT_ID", "")
    if not token or not chat_id:
        return None
    return token, chat_id


def send_to_bale(event: str, name: str, similarity: int, device_ts: str) -> bool:
    """هم‌امضا با send_to_sheet تا worker هر دو را یکسان صدا بزند؛
    similarity برای اعلان بله استفاده نمی‌شود."""
    cfg = _bale_config()
    if cfg is None:
        return False
    token, chat_id = cfg

    title = _EVENT_EMOJI.get(event, event)
    who = f": {name}" if name else ""
    text = f"{title}{who}\n🕒 {jdate.to_jalali_str(device_ts)}"

    try:
        resp = requests.post(
            _BALE_API.format(token=token),
            json={"chat_id": chat_id, "text": text},
            timeout=10,
        )
        if resp.status_code != 200:
            log.error("Bale send failed: HTTP %d %s", resp.status_code, resp.text[:200])
            return False
        return True
    except Exception as exc:  # noqa: BLE001
        log.error("Bale send error: %s", exc)
        return False
