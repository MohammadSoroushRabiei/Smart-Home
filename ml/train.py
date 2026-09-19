#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
آموزش دو مدل رگرسیون لجستیک (چراغ و فن) روی داده‌ی فرضی و تولید
main/ml_model_weights.h برای استقرار روی ESP32.

بردار ویژگی - باید *دقیقاً* با ml_model.h و ml_agent.c در فیرمور یکی باشد:
  0: bias = 1
  1-6: sin/cos ساعت با ۳ هارمونیک (2πh/24, 4πh/24, 6πh/24)
       - هارمونیک‌ها برای بازنمایی دو پنجره‌ی شبانه‌ی چراغ (18-24 و 5-8)
  7: آخر هفته (پنج‌شنبه/جمعه)
  8: حضور
  9: min(lux, 1000) / 1000
  10: clamp((temp - 15) / 15, 0, 1)
  11: clamp(hum / 100, 0, 1)

عمداً «وضعیت فعلی دستگاه» در ویژگی‌ها نیست: با حضورش، شواهدِ بافت (ساعت/
حضور/نور/دما) به نفع هسترزیس سرقت می‌شود و مدل در حالت خودکار منفعل می‌ماند.
پایداری به‌جای هسترزیس با آستانه‌های اطمینان (0.75/0.25) در ml_agent.c تأمین
می‌شود.

آموزش: full-batch gradient descent + L2 (numpy) - دقت انتظاری ~94%
(سقف بایز = نویز 4-5% برچسب‌ها).

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

N_FEATURES = 12
FEATURE_NAMES = ["bias", "sin_hour", "cos_hour", "sin_hour2", "cos_hour2",
                 "sin_hour3", "cos_hour3",
                 "is_weekend", "presence", "lux_n", "temp_n", "hum_n"]
EPOCHS = 1500
LR = 1.0
LR_DECAY = 0.9995
L2 = 3e-5
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
//  مدل: رگرسیون لجستیک فقط-بافت (هیبرید: پیش‌آموزش آفلاین + SGD آنلاین)
//  تاریخ آموزش: {now} | نمونه‌های آموزش: {n_train}
//  دقت اعتبارسنجی: چراغ {acc_l * 100:.1f}% | فن {acc_f * 100:.1f}%
// ======================================================================

#define ML_MODEL_VERSION   2
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
    X, y_light, y_fan, days = load_dataset()
    assert X.shape[1] == N_FEATURES

    max_day = days.max()
    train_mask = days <= (max_day - VAL_DAYS)
    val_mask = ~train_mask

    print(f"samples: train={int(train_mask.sum())}  val={int(val_mask.sum())}")

    w_light = train_model(X[train_mask], y_light[train_mask])
    w_fan = train_model(X[train_mask], y_fan[train_mask])

    for name, w, y in (("LIGHT", w_light, y_light), ("FAN", w_fan, y_fan)):
        a_tr, l_tr = metrics(w, X[train_mask], y[train_mask])
        a_va, l_va = metrics(w, X[val_mask], y[val_mask])
        print(f"\n[{name}]  train acc={a_tr * 100:.2f}%  loss={l_tr:.4f}")
        print(f"[{name}]  val   acc={a_va * 100:.2f}%  loss={l_va:.4f}")

    print("\nوزن‌ها به تفکیک ویژگی:")
    print(f"{'feature':<12}{'light':>12}{'fan':>12}")
    for i, name in enumerate(FEATURE_NAMES):
        print(f"{name:<12}{w_light[i]:>12.4f}{w_fan[i]:>12.4f}")

    acc_l, _ = metrics(w_light, X[val_mask], y_light[val_mask])
    acc_f, _ = metrics(w_fan, X[val_mask], y_fan[val_mask])
    export_header(w_light, w_fan, int(train_mask.sum()), acc_l, acc_f)


def load_dataset():
    xs, y_light, y_fan, days = [], [], [], []
    with open(DATA_PATH) as f:
        for row in csv.DictReader(f):
            hour = float(row["hour"])
            lux = float(row["lux"])
            temp = float(row["temp"])
            hum = float(row["hum"])
            xs.append([
                1.0,
                math.sin(2 * math.pi * hour / 24.0),
                math.cos(2 * math.pi * hour / 24.0),
                math.sin(4 * math.pi * hour / 24.0),
                math.cos(4 * math.pi * hour / 24.0),
                math.sin(6 * math.pi * hour / 24.0),
                math.cos(6 * math.pi * hour / 24.0),
                float(row["is_weekend"]),
                float(row["presence"]),
                min(lux, 1000.0) / 1000.0,
                min(max((temp - 15.0) / 15.0, 0.0), 1.0),
                min(max(hum / 100.0, 0.0), 1.0),
            ])
            y_light.append(int(row["label_light"]))
            y_fan.append(int(row["label_fan"]))
            days.append(int(row["day"]))
    return (np.array(xs, dtype=np.float64),
            np.array(y_light, dtype=np.float64),
            np.array(y_fan, dtype=np.float64),
            np.array(days))


if __name__ == "__main__":
    main()
