"""حضور و غیاب — سرور لوکال (FastAPI)

مسیرها:
  POST /api/event   - دریافت رویداد از ESP32 (گیت secret)
  GET  /health      - تست اتصال از برد/داشبورد (گیت secret در query)
  GET  /stats       - آمار (بدون secret؛ فقط برای دیدن از مرورگر LAN)

جریان: رویداد بلافاصله در SQLite ذخیره می‌شود؛ worker دوره‌ای رکوردهای
pending را به Google Sheets و بله می‌فرستد و موفق‌ها را علامت می‌زند.
اگر اینترنت قطع باشد رکوردها در SQLite امن می‌مانند و بعداً ارسال می‌شوند.
"""
import logging
import os
import threading
import time
from typing import Any

from fastapi import FastAPI, HTTPException, Query
from pydantic import BaseModel, Field

import db
import integrations

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(name)s: %(message)s",
)
log = logging.getLogger("attendance")

SECRET = os.environ.get("ATTENDANCE_SECRET", "")
SQLITE_PATH = os.environ.get("SQLITE_PATH", "/data/attendance.db")
WORKER_INTERVAL_S = float(os.environ.get("WORKER_INTERVAL_S", "15"))

VALID_EVENTS = {"attendance_in", "attendance_out", "door_face", "door_code"}
# شیت گوگل فقط برای حضور و غیاب است؛ رویدادهای درب فقط SQLite + بله
ATTENDANCE_EVENTS = {"attendance_in", "attendance_out"}

app = FastAPI(title="Smart-Home Attendance Server", docs_url=None, redoc_url=None)


# ---------------------------------------------------------------------
# مدل ورودی - عین payload ای که ESP32 می‌فرستد
# ---------------------------------------------------------------------

class EventIn(BaseModel):
    secret: str = ""
    event: str
    ts: str = ""
    name: str = Field(default="", max_length=64)
    id: int = 0
    similarity: int = 0


def _check_secret(provided: str) -> None:
    if not SECRET:
        log.error("ATTENDANCE_SECRET is not set - rejecting everything")
        raise HTTPException(status_code=503, detail="server not configured")
    if provided != SECRET:
        raise HTTPException(status_code=403, detail="bad secret")


# ---------------------------------------------------------------------
# مسیرها
# ---------------------------------------------------------------------

@app.post("/api/event")
def receive_event(ev: EventIn) -> dict[str, Any]:
    _check_secret(ev.secret)

    if ev.event == "test":
        # تست اتصال از داشبورد برد - چیزی ذخیره نمی‌شود
        return {"ok": True, "test": True}

    if ev.event not in VALID_EVENTS:
        raise HTTPException(status_code=400, detail="unknown event")

    row_id = db.insert_event(ev.event, ev.name.strip(), ev.id, ev.similarity, ev.ts)
    if ev.event not in ATTENDANCE_EVENTS:
        # رویداد درب به شیت نمی‌رود - همان لحظه از صف گوگل خارج می‌شود
        db.mark_synced("google", row_id)
    log.info("event stored: %s name=%r id=%d sim=%d%% ts=%r",
             ev.event, ev.name, ev.id, ev.similarity, ev.ts)
    # worker هر WORKER_INTERVAL_S یک‌بار صف را می‌کشد - اینجا لازم نیست منتظر بمانیم
    return {"ok": True}


@app.get("/health")
def health(secret: str = Query(default="")) -> dict[str, Any]:
    _check_secret(secret)
    return {"ok": True, "service": "attendance"}


@app.get("/stats")
def stats() -> dict[str, Any]:
    return db.stats() | {"google": bool(os.environ.get("GOOGLE_SCRIPT_URL")),
                         "bale": bool(os.environ.get("BALE_TOKEN"))}


# ---------------------------------------------------------------------
# worker ارسال به مقصدهای بیرونی
# ---------------------------------------------------------------------

def _flush_dest(dest: str, sender) -> None:
    """pending های یک مقصد را می‌فرستد؛ sender(row)->bool"""
    for row in db.fetch_pending(dest):
        row_id, event, name, person_id, similarity, device_ts = row
        try:
            ok = sender(event, name, similarity, device_ts)
        except Exception:  # noqa: BLE001 - worker نباید بمیرد
            log.exception("flush(%s) crashed on row %d", dest, row_id)
            ok = False
        if ok:
            db.mark_synced(dest, row_id)
            log.info("row %d -> %s OK", row_id, dest)


def _worker_loop() -> None:
    while True:
        time.sleep(WORKER_INTERVAL_S)
        try:
            _flush_dest("google", integrations.send_to_sheet)
        except Exception:  # noqa: BLE001
            log.exception("google flush cycle failed")
        try:
            _flush_dest("bale", integrations.send_to_bale)
        except Exception:  # noqa: BLE001
            log.exception("bale flush cycle failed")


@app.on_event("startup")
def on_startup() -> None:
    db.init(SQLITE_PATH)
    threading.Thread(target=_worker_loop, name="outbox-worker", daemon=True).start()
    log.info("attendance server started (db=%s)", SQLITE_PATH)
