#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
تولید داده‌ی فرضی برای پیش‌آموزش مدل رفتار کاربر (چراغ و فن).

سناریوی شبیه‌سازی‌شده (منطبق با Documents/ml-design.md):
  - حضور: روزهای کاری یک بیرون‌رفت طولانی (کار ~۸ تا ~۱۷)،
    آخر هفته (پنج‌شنبه/جمعه) دو بیرون‌رفت کوتاه‌تر
  - نور محیط (lux): تابعی از ساعت طلوع/غروب + ضریب آب‌وهوایی روز
  - دما: چرخه‌ی روزانه (گرمای بعدازظهر) + رانش ماهانه + نویز
  - رطوبت: تقریباً عکس دما + نویز

«سیاست کاربر» (برچسب‌ها - همان رفتاری که مدل باید یاد بگیرد):
  - چراغ روشن  <=>  حضور ∧ تاریکی (lux < 60) ∧ عصر/شب یا بامداد (18-24 یا 5-8)
  - فن روشن    <=>  حضور ∧ دمای بالاتر از 27 درجه
  - ~4-5% خطای انسانی (برچسب معکوس) برای واقع‌گرایی

خروجی: ml/data/synthetic_log.csv - با seed ثابت، کاملاً تکرارپذیر.

اجرا:  python ml/generate_data.py
"""

import csv
import math
import os
import random
from datetime import date, timedelta

SEED = 42
N_DAYS = 30
SAMPLE_MIN = 5                    # فاصله‌ی نمونه‌ها: هر ۵ دقیقه
START_DATE = date(2026, 8, 20)    # ثابت برای تکرارپذیری

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
OUT_PATH = os.path.join(OUT_DIR, "synthetic_log.csv")

# ثابت‌های محیط - باید با main/virtual_devices.c یکی بماند
SUNRISE_H = 6.4
SUNSET_H = 18.6

# ثابت‌های «سیاست کاربر» - فقط برای تولید برچسب فرضی؛ فیرمور این‌ها را نمی‌شناسد
LIGHT_DARK_LUX = 60.0
LIGHT_HOURS = ((18.0, 24.0), (5.0, 8.0))
FAN_TEMP_C = 27.0
LABEL_NOISE_LIGHT = 0.04
LABEL_NOISE_FAN = 0.05


def day_schedule(d: date):
    """برنامه‌ی یک روز: (آخر‌هفته؟، بازه‌های غیبت [(h1,h2)]، ضریب آب‌وهوا).

    seed بر اساس تاریخ است تا الگوی هر روز در اجراهای مختلف یکسان بماند
    (دقیقاً مثل همان کاری که virtual_devices.c روی میکرو می‌کند).
    """
    rng = random.Random((d.toordinal() * 2654435761) & 0xFFFFFFFF)
    weekend = d.weekday() in (3, 4)          # پنج‌شنبه=3، جمعه=4
    if weekend:
        s1 = 9.0 + rng.random() * 2.0
        s2 = 16.0 + rng.random() * 2.0
        away = [(s1, s1 + 1.0 + rng.random() * 2.0),
                (s2, s2 + 0.8 + rng.random() * 1.5)]
    else:
        s = 8.0 + rng.random() * 0.75
        away = [(s, s + 8.5 + rng.random() * 1.5)]   # ≈ ساعت کاری
    weather = 0.35 + rng.random() * 0.65             # 0.35=ابری تا 1=آفتابی
    return weekend, away, weather


def lux_at(t: float, weather: float, rng: random.Random) -> float:
    """نور محیط در ساعت t (لوکس داخل خانه، نزدیک پنجره)."""
    if SUNRISE_H < t < SUNSET_H:
        base = 40.0 + 820.0 * weather * math.sin(
            math.pi * (t - SUNRISE_H) / (SUNSET_H - SUNRISE_H))
    else:
        base = 1.5
    return max(0.0, base * (0.92 + 0.16 * rng.random()))


def temp_at(t: float, day: int, rng: random.Random) -> float:
    """دمای اتاق: رانش ماهانه + قله‌ی بعدازظهر + نویز."""
    base_day = 23.0 + 3.0 * math.sin(2 * math.pi * day / 30.0)
    daily = 4.0 * math.exp(-((t - 15.0) ** 2) / 18.0)
    return base_day + daily + rng.gauss(0, 0.25)


def humidity_at(temp: float, rng: random.Random) -> float:
    return min(80.0, max(20.0, 46.0 - (temp - 24.0) * 1.2 + rng.gauss(0, 1.5)))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    rng = random.Random(SEED)
    rows = []
    cur_light, cur_fan = 0, 0

    for day in range(N_DAYS):
        d = START_DATE + timedelta(days=day)
        weekend, away, weather = day_schedule(d)
        for minute in range(0, 24 * 60, SAMPLE_MIN):
            t = minute / 60.0
            presence = not any(h1 <= t <= h2 for h1, h2 in away)
            lux = lux_at(t, weather, rng)
            temp = temp_at(t, day, rng)
            hum = humidity_at(temp, rng)

            # برچسب = «تصمیم مطلوب کاربر در این لحظه»
            in_light_hours = any(a <= t < b for a, b in LIGHT_HOURS)
            light = int(presence and lux < LIGHT_DARK_LUX and in_light_hours)
            if rng.random() < LABEL_NOISE_LIGHT:
                light = 1 - light
            fan = int(presence and temp > FAN_TEMP_C)
            if rng.random() < LABEL_NOISE_FAN:
                fan = 1 - fan

            rows.append([day, f"{t:.3f}", int(weekend), int(presence),
                         f"{lux:.1f}", f"{temp:.2f}", f"{hum:.1f}",
                         cur_light, cur_fan, light, fan])
            cur_light, cur_fan = light, fan

    with open(OUT_PATH, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["day", "hour", "is_weekend", "presence", "lux",
                    "temp", "hum", "cur_light", "cur_fan",
                    "label_light", "label_fan"])
        w.writerows(rows)

    n = len(rows)
    on_l = sum(r[9] for r in rows)
    on_f = sum(r[10] for r in rows)
    print(f"OK: {n} samples -> {OUT_PATH}")
    print(f"    light ON: {on_l} ({100 * on_l / n:.1f}%) | "
          f"fan ON: {on_f} ({100 * on_f / n:.1f}%)")


if __name__ == "__main__":
    main()
