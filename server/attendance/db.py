"""SQLite storage for the attendance server.

جدول events رکوردها را نگه می‌دارد؛ google_synced / bale_synced فلگ
عددی‌اند (0 = pending، 1 = ارسال شده) و worker دوره‌ای pending ها را
می‌کشد. تاریخ‌ها همان میلادیِ دستگاه ذخیره می‌شوند و فقط در نمایش شمسی
می‌شوند.
"""
import sqlite3
import threading
import time
from pathlib import Path

_SCHEMA = """
CREATE TABLE IF NOT EXISTS events (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    event         TEXT NOT NULL,          -- attendance_in/out | door_face | door_code
    name          TEXT NOT NULL DEFAULT '',
    person_id     INTEGER NOT NULL DEFAULT 0,
    similarity    INTEGER NOT NULL DEFAULT 0,   -- درصد 0..100
    device_ts     TEXT NOT NULL DEFAULT '',     -- زمان ثبت روی برد (میلادی)
    google_synced INTEGER NOT NULL DEFAULT 0,
    bale_synced   INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_events_pending_google
    ON events (google_synced) WHERE google_synced = 0;
CREATE INDEX IF NOT EXISTS idx_events_pending_bale
    ON events (bale_synced) WHERE bale_synced = 0;
"""

_lock = threading.Lock()
_conn: sqlite3.Connection | None = None


def _migrate_old_schema() -> None:
    """نسخه‌های قدیمی: received_at + سینک‌های زمانی TEXT → فلگ عددی.
    رکوردها و وضعیت pending شان حفظ می‌شوند."""
    cols = [r[1] for r in _conn.execute("PRAGMA table_info(events)")]
    if not cols or "received_at" not in cols:
        return

    _conn.executescript("""
        ALTER TABLE events RENAME TO events_old;
    """)
    _conn.executescript(_SCHEMA)
    _conn.execute("""
        INSERT INTO events (id, event, name, person_id, similarity,
                            device_ts, google_synced, bale_synced)
        SELECT id, event, name, person_id, similarity, device_ts,
               CASE WHEN google_synced IS NULL THEN 0 ELSE 1 END,
               CASE WHEN bale_synced   IS NULL THEN 0 ELSE 1 END
        FROM events_old
    """)
    _conn.execute("DROP TABLE events_old")
    _conn.commit()


def init(db_path: str) -> None:
    global _conn
    Path(db_path).parent.mkdir(parents=True, exist_ok=True)
    _conn = sqlite3.connect(db_path, check_same_thread=False)
    # جدول قدیمی ممکن است از قبل وجود داشته باشد - migration قبل از CREATE
    _migrate_old_schema()
    _conn.executescript(_SCHEMA)
    _conn.commit()


def _now() -> str:
    return time.strftime("%Y-%m-%d %H:%M:%S")


def insert_event(event: str, name: str, person_id: int,
                 similarity: int, device_ts: str) -> int:
    """رکورد را ذخیره و شناسه‌اش را برمی‌گرداند"""
    with _lock:
        cur = _conn.execute(
            "INSERT INTO events (event, name, person_id, similarity, device_ts)"
            " VALUES (?, ?, ?, ?, ?)",
            (event, name, person_id, similarity, device_ts),
        )
        _conn.commit()
        return cur.lastrowid


def fetch_pending(dest: str, limit: int = 20) -> list[tuple]:
    """رکوردهای ارسال‌نشده به مقصد (dest = google | bale)"""
    col = f"{dest}_synced"
    with _lock:
        cur = _conn.execute(
            f"SELECT id, event, name, person_id, similarity, device_ts"
            f" FROM events WHERE {col} = 0 ORDER BY id LIMIT ?",
            (limit,),
        )
        return cur.fetchall()


def mark_synced(dest: str, row_id: int) -> None:
    col = f"{dest}_synced"
    with _lock:
        _conn.execute(
            f"UPDATE events SET {col} = 1 WHERE id = ?", (row_id,)
        )
        _conn.commit()


def stats() -> dict:
    with _lock:
        total = _conn.execute("SELECT COUNT(*) FROM events").fetchone()[0]
        pend_g = _conn.execute(
            "SELECT COUNT(*) FROM events WHERE google_synced = 0").fetchone()[0]
        pend_b = _conn.execute(
            "SELECT COUNT(*) FROM events WHERE bale_synced = 0").fetchone()[0]
    return {"total": total, "google_pending": pend_g, "bale_pending": pend_b}
