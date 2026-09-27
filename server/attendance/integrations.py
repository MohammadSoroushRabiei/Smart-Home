"""Integrations: ارسال رکوردهای حضور و غیاب به Google Sheets (Apps Script webhook).

این سرور از الان فقط حضور و غیاب است: دیتابیس + شیت. اعلان‌های بله (ورود/خروج
و رویدادهای درب) به سرور بله (`server/bale_bot`) منتقل شده‌اند و رویدادهای درب
هم دیگر به این سرور نمی‌آیند.

اگر متغیر محیطی GOOGLE_SCRIPT_URL ست نشده باشد، تابع False برمی‌گرداند و رکورد
در outbox می‌ماند تا روزی کانفیگ شد ارسال شود.

یک شیت بساز، اسکریپت docs/attendance/attendance-webhook.gs را در آن Deploy کن
و آدرس /exec را در GOOGLE_SCRIPT_URL بگذار.
"""
import logging
import os

import requests

import jdate

log = logging.getLogger("attendance.integrations")


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
