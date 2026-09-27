"""تبدیل تاریخ میلادی به شمسی - بدون وابستگی خارجی.

الگوریتم استاندارد (هم‌ارز jdf.scr.ir). تاریخ‌ها در SQLite به همان شکل
میلادیِ ارسالی از برد ذخیره می‌شوند و فقط در لحظه‌ی نمایش (شیت و بله)
شمسی می‌شوند.
"""
import time


def gregorian_to_jalali(gy: int, gm: int, gd: int) -> tuple[int, int, int]:
    g_d_m = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334]
    gy2 = gy + 1 if gm > 2 else gy
    days = (355666 + (365 * gy) + ((gy2 + 3) // 4) - ((gy2 + 99) // 100)
            + ((gy2 + 399) // 400) + gd + g_d_m[gm - 1])
    jy = -1595 + (33 * (days // 12053))
    days %= 12053
    jy += 4 * (days // 1461)
    days %= 1461
    if days > 365:
        jy += (days - 1) // 365
        days = (days - 1) % 365
    if days < 186:
        jm = 1 + (days // 31)
        jd = 1 + (days % 31)
    else:
        jm = 7 + ((days - 186) // 30)
        jd = 1 + ((days - 186) % 30)
    return jy, jm, jd


def to_jalali_str(ts_str: str) -> str:
    """'2026-09-20 23:15:00' → '1405/06/29 23:15:00'؛ ورودی غیرقابل‌پارس
    دست‌نخورده برمی‌گردد تا چیزی گم نشود."""
    date_part, time_part = to_jalali_parts(ts_str)
    if time_part is None:
        return date_part
    return f"{date_part} {time_part}"


def to_jalali_parts(ts_str: str) -> tuple[str, str | None]:
    """'2026-09-20 23:15:00' → ('1405/06/29', '23:15:00') برای دو ستون
    مجزای تاریخ/ساعت. اگر ورودی قابل‌پارس نبود، خودش به‌عنوان تاریخ با
    زمان None برمی‌گردد."""
    if not ts_str:
        return ts_str, None
    try:
        t = time.strptime(ts_str, "%Y-%m-%d %H:%M:%S")
    except (ValueError, TypeError):
        return ts_str, None
    jy, jm, jd = gregorian_to_jalali(t.tm_year, t.tm_mon, t.tm_mday)
    date_str = f"{jy:04d}/{jm:02d}/{jd:02d}"
    time_str = f"{t.tm_hour:02d}:{t.tm_min:02d}:{t.tm_sec:02d}"
    return date_str, time_str
