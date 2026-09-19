#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
آموزش دو مدل رگرسیون لجستیک (چراغ و فن) روی داده‌ی فرضی و تولید
main/ml_model_weights.h برای استقرار روی ESP32.

بردار ویژگی - باید *دقیقاً* با ml_model.h و ml_agent.c در فیرمور یکی باشد:
  0: bias = 1
  1: sin(2*pi*hour/24)
  2: cos(2*pi*hour/24)
  3: آخر هفته (پنج‌شنبه/جمعه)
  4: حضور
  5: min(lux, 1000) / 1000
  6: clamp((temp - 15) / 15, 0, 1)
  7: clamp(hum / 100, 0, 1)
  8: وضعیت فعلی همان دستگاه (هسترزیس)

آموزش: full-batch gradient descent + L2 (numpy) - دقت انتظاری ~93-95%
(سقف طبیعی به‌خاطر نویز 4-5% برچسب‌ها).

اجرا:  python ml/train.py     (اول generate_data.py را اجرا کنید)
"""

import csv
import math
import os
from datetime import datetime

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
DATA_PATH = os.path.join(HERE, "data", "synthetic_log.csv")
HEADER_PATH = os.path.abspath(os.path.join(HERE, "..", "main", "ml_model_weights.h"))

N_FEATURES = 9
FEATURE_NAMES = ["bias", "sin_hour", "cos_hour", "is_weekend", "presence",
                 "lux_n", "temp_n", "hum_n", "cur_state"]
EPOCHS = 400
LR = 0.5
LR_DECAY = 0.995
L2 = 1e-4
VAL_DAYS = 6          # آخرین ۶ روز برای اعتبارسنجی


def sigmoid(z):
    return 1.0 / (1.0 + np.exp(-np.clip(z, -30.0, 30.0)))


def train_model(X, y):
    w = np.zeros(N_FEATURES)
    n = len(y)
    for epoch in range(EPOCHS):
        p = sigmoid(X @ w)
        grad = X.T @ (p - y) / n + L2 * w
        w -= LR * (LR_DECAY ** epoch) * grad
    return w


def metrics(w, X, y):
    p = sigmoid(X @ w)
    pred = (p >= 0.5).astype(np.float64)
    acc = float(np.mean(pred == y))
    eps = 1e-9
    logloss = float(-np.mean(y * np.log(p + eps) + (1 - y) * np.log(1 - p + eps)))
    return acc, logloss


def fmt_weights(w):
    return ", ".join(f"{v:.6f}f" for v in w)


def export_header(w_light, w_fan, n_train, acc_l, acc_f):
    now = datetime.now().strftime("%Y-%m-%d %H:%M")
    content = f"""#pragma once
// ======================================================================
//  خودکار تولید شده توسط ml/train.py - لطفاً دستی ویرایش نکنید.
//  مدل: رگرسیون لجستیک (هیبرید: پیش‌آموزش آفلاین + یادگیری آنلاین SGD)
//  تاریخ آموزش: {now} | نمونه‌های آموزش: {n_train}
//  دقت اعتبارسنجی: چراغ {acc_l * 100:.1f}% | فن {acc_f * 100:.1f}%
// ======================================================================

#define ML_MODEL_VERSION   1
#define ML_N_FEATURES      {N_FEATURES}
#define ML_TRAIN_SAMPLES   {n_train}

// وزن‌های اولیه‌ی مدل چراغ (با داده فرضی آموزش دیده)
static const float ML_LIGHT_W[ML_N_FEATURES] = {{
    {fmt_weights(w_light)}
}};

// وزن‌های اولیه‌ی مدل فن (با داده فرضی آموزش دیده)
static const float ML_FAN_W[ML_N_FEATURES] = {{
    {fmt_weights(w_fan)}
}};
"""
    with open(HEADER_PATH, "w") as f:
        f.write(content)
    print(f"OK: header written -> {HEADER_PATH}")


def main():
    Xl, Xf, y_light, y_fan, days = load_dataset_split_features()
    max_day = days.max()
    train_mask = days <= (max_day - VAL_DAYS)
    val_mask = ~train_mask

    print(f"samples: train={int(train_mask.sum())}  val={int(val_mask.sum())}")

    w_light = train_model(Xl[train_mask], y_light[train_mask])
    w_fan = train_model(Xf[train_mask], y_fan[train_mask])

    for name, w, X, y in (("LIGHT", w_light, Xl, y_light), ("FAN", w_fan, Xf, y_fan)):
        a_tr, l_tr = metrics(w, X[train_mask], y[train_mask])
        a_va, l_va = metrics(w, X[val_mask], y[val_mask])
        print(f"\n[{name}]  train acc={a_tr * 100:.2f}%  loss={l_tr:.4f}")
        print(f"[{name}]  val   acc={a_va * 100:.2f}%  loss={l_va:.4f}")

    print("\nوزن‌ها به تفکیک ویژگی:")
    print(f"{'feature':<12}{'light':>12}{'fan':>12}")
    for i, name in enumerate(FEATURE_NAMES):
        print(f"{name:<12}{w_light[i]:>12.4f}{w_fan[i]:>12.4f}")

    acc_l, _ = metrics(w_light, Xl[val_mask], y_light[val_mask])
    acc_f, _ = metrics(w_fan, Xf[val_mask], y_fan[val_mask])
    export_header(w_light, w_fan, int(train_mask.sum()), acc_l, acc_f)


def load_dataset_split_features():
    """دو ماتریس ویژگی می‌سازد که فقط ستون آخر (وضعیت فعلی) فرق دارد:
    یکی با cur_light برای مدل چراغ، یکی با cur_fan برای مدل فن."""
    xs, cur_l, cur_f, y_light, y_fan, days = [], [], [], [], [], []
    with open(DATA_PATH) as f:
        for row in csv.DictReader(f):
            hour = float(row["hour"])
            lux = float(row["lux"])
            temp = float(row["temp"])
            hum = float(row["hum"])
            x = [
                1.0,
                math.sin(2 * math.pi * hour / 24.0),
                math.cos(2 * math.pi * hour / 24.0),
                float(row["is_weekend"]),
                float(row["presence"]),
                min(lux, 1000.0) / 1000.0,
                min(max((temp - 15.0) / 15.0, 0.0), 1.0),
                min(max(hum / 100.0, 0.0), 1.0),
                0.0,
            ]
            xs.append(x)
            cur_l.append(float(row["cur_light"]))
            cur_f.append(float(row["cur_fan"]))
            y_light.append(int(row["label_light"]))
            y_fan.append(int(row["label_fan"]))
            days.append(int(row["day"]))
    X = np.array(xs, dtype=np.float64)
    Xl = X.copy(); Xl[:, 8] = cur_l
    Xf = X.copy(); Xf[:, 8] = cur_f
    return (Xl, Xf,
            np.array(y_light, dtype=np.float64),
            np.array(y_fan, dtype=np.float64),
            np.array(days))


if __name__ == "__main__":
    main()
