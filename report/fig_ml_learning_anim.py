# -*- coding: utf-8 -*-
"""Animated GIF: online learning of user behavior on the smart-home device.

Simulates exactly the on-chip SGD of main/ml_model.c / ml_agent.c:
  - features x (12) built like build_features_base() at the moment of each action
  - every manual user action = one training sample (x, y)
  - prediction p = sigmoid(clip(w.x, +-30)); accuracy pushed on a 20-decision ring
  - update w <- clip(w + 0.08 * (y - p) * x, +-8)
  - promotion SHADOW -> AUTO when n >= 15 and rolling acc >= 85%

Weights start at 0 here so the learning is visible (the real device boots from
the offline-pretrained weights of ml/train.py).

Run:  python report/fig_ml_learning_anim.py  ->  report/figs/fig_ml_learning.gif
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from PIL import Image
import numpy as np
import os

OUT = os.path.join(os.path.dirname(__file__), "figs", "fig_ml_learning.gif")

INK, BLUE, GREEN, RED, GRAY, TXT = "#2E3B4E", "#2F5D8C", "#3E7C4F", "#A94442", "#8A97A6", "#4A5568"
ETA, WCLIP, N_MIN, ACC_MIN, WINDOW = 0.08, 8.0, 15, 85.0, 20

plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10, "text.color": INK})

# ----------------------------------------------------------------------------
# environment model (typical weekday/weekend day) + user habit policy
# ----------------------------------------------------------------------------
def lux_of(h):
    return 2.0 + 998.0 * np.exp(-((h - 12.5) / 3.4) ** 2)

def presence_of(h, wknd):
    if wknd:
        return (7.5 <= h < 10.0) or (17.0 <= h < 23.5)
    return (6.0 <= h < 9.0) or (17.0 <= h < 23.0)

def temp_of(h):
    return 21.0 + 7.0 * np.exp(-((h - 15.0) / 4.5) ** 2)

def user_wants_light(h, wknd):
    """habit: light ON when home, dark, evening or early morning"""
    return presence_of(h, wknd) and lux_of(h) < 60 and (18.0 <= h < 24.0 or 5.0 <= h < 8.0)

def features(h, wknd):
    x = np.zeros(12)
    x[0] = 1.0
    x[1], x[2] = np.sin(2 * np.pi * h / 24), np.cos(2 * np.pi * h / 24)
    x[3], x[4] = np.sin(4 * np.pi * h / 24), np.cos(4 * np.pi * h / 24)
    x[5], x[6] = np.sin(6 * np.pi * h / 24), np.cos(6 * np.pi * h / 24)
    x[7] = 1.0 if wknd else 0.0
    x[8] = 1.0 if presence_of(h, wknd) else 0.0
    x[9] = min(lux_of(h), 1000.0) / 1000.0
    t = (temp_of(h) - 15.0) / 15.0
    x[10] = min(max(t, 0.0), 1.0)
    x[11] = 0.40                      # humidity ~ constant
    return x

def predict(w, x):
    z = np.clip(w @ x, -30.0, 30.0)
    return 1.0 / (1.0 + np.exp(-z))

# ----------------------------------------------------------------------------
# synthesize 12 days of manual actions (seed fixed -> reproducible)
# ----------------------------------------------------------------------------
rng = np.random.default_rng(42)
DAYS = 20
events = []                            # (day, hour, y, wknd)
light_on = False
for d in range(DAYS):
    wknd = (d % 7) in (4, 5)           # Thu/Fri weekend, day 0 = Sunday
    if wknd:                            # late wake-up, presence ends 23:30
        sched = [rng.uniform(7.65, 7.95), rng.uniform(9.4, 9.9),
                 rng.uniform(18.15, 19.6), rng.uniform(23.55, 23.9)]
    else:
        sched = [rng.uniform(6.15, 7.30), rng.uniform(8.2, 8.8),
                 rng.uniform(18.15, 19.6), rng.uniform(23.05, 23.45)]
    for h in sched:
        y = 1 if user_wants_light(h, wknd) else 0
        if rng.random() < 0.05:        # 5% label noise (as in generate_data.py)
            y = 1 - y
        if y != int(light_on):
            events.append((d, h, y, wknd))
            light_on = bool(y)
    if rng.random() < 0.06 and light_on:   # rare guest turns the light OFF midday
        h = rng.uniform(12.0, 16.0)
        events.append((d, h, 0, wknd))
        light_on = False
N = len(events)

# ----------------------------------------------------------------------------
# run the exact on-chip learning loop once; keep full history
# ----------------------------------------------------------------------------
w = np.zeros(12)
w_hist = [w.copy()]
meta = []                              # per action: (day, hour, y, p_before, err)
acc_hist = [None]                      # rolling accuracy after each action
window = []
auto_from = None                       # frame index when promoted to AUTO
for k, (d, h, y, wknd) in enumerate(events):
    x = features(h, wknd)
    p = predict(w, x)
    window.append((p >= 0.5) == (y == 1))
    window = window[-WINDOW:]
    acc = 100.0 * np.mean(window)
    acc_hist.append(acc)
    err = float(y) - p
    meta.append((d, h, y, float(p), float(err)))
    w = np.clip(w + ETA * err * x, -WCLIP, WCLIP)
    w_hist.append(w.copy())
    if auto_from is None and len(window) >= N_MIN and acc >= ACC_MIN:
        auto_from = k + 1

# p(h) curve on a typical weekday for given weights
HH = np.arange(0.0, 24.0, 0.1)
XH = np.stack([features(h, False) for h in HH])
def curve(w):
    z = np.clip(XH @ w, -30.0, 30.0)
    return 1.0 / (1.0 + np.exp(-z))
p_init = curve(w_hist[0])

# ----------------------------------------------------------------------------
# figure
# ----------------------------------------------------------------------------
fig = plt.figure(figsize=(11.8, 6.4), dpi=105)
ax_p = fig.add_axes([0.055, 0.16, 0.52, 0.62])
ax_w = fig.add_axes([0.665, 0.16, 0.305, 0.62])

fig.suptitle("Online Learning of User Behavior — weights update at every manual action",
             fontsize=12.5, fontweight="bold", color=BLUE, y=0.975)
fig.text(0.5, 0.935,
         r"update per action:  $w \leftarrow \mathrm{clip}\left(w + \eta\,(y-p)\,x,\ \pm 8\right)$,"
         r"   $\eta = 0.08$   —   as implemented in main/ml_model.c",
         ha="center", fontsize=9.5, color=INK)
counters = fig.text(0.5, 0.900, "", ha="center", fontsize=9.2, color=TXT)
logline = fig.text(0.5, 0.050, "", ha="center", fontsize=9.0, color=INK)
fig.text(0.5, 0.012,
         "w starts at 0 here so learning is visible — the real device boots from the "
         "offline-pretrained weights (ml/train.py)",
         ha="center", fontsize=7.6, style="italic", color=GRAY)

# --- left axes: p(light ON) over the day
ax_p.set_xlim(0, 24)
ax_p.set_ylim(-0.10, 1.20)
ax_p.set_xlabel("hour of day (typical weekday)")
ax_p.set_ylabel("p(light ON)")
ax_p.set_xticks(range(0, 25, 3))
ax_p.axhspan(0.75, 1.20, color=GREEN, alpha=0.07, zorder=0)
ax_p.axhspan(-0.10, 0.25, color=RED, alpha=0.06, zorder=0)
for pv, lab in ((0.75, "0.75  act ON"), (0.25, "0.25  act OFF")):
    ax_p.axhline(pv, color=GRAY, lw=0.9, ls=(0, (4, 3)))
    ax_p.text(23.9, pv + 0.02, lab, ha="right", fontsize=7.6, color=GRAY, zorder=4)
ax_p.plot(HH, p_init, color=GRAY, lw=1.0, ls=(0, (2, 2)))
ax_p.text(23.85, 0.545, "initial w = 0", ha="right", fontsize=7.6, color=GRAY, zorder=4)
line, = ax_p.plot([], [], color=BLUE, lw=2.2, zorder=4)
sct_on = ax_p.scatter([], [], s=26, color=GREEN, zorder=5, alpha=0.75,
                      edgecolors="white", linewidths=0.4)
sct_off = ax_p.scatter([], [], s=26, color=RED, zorder=5, alpha=0.75,
                       edgecolors="white", linewidths=0.4)
curMarker, = ax_p.plot([], [], "o", ms=10, mfc="none", mec=INK, mew=1.6, zorder=6)
curVline = ax_p.axvline(0, color=INK, lw=1.0, ls=(0, (3, 3)), alpha=0.0, zorder=3)
badge = ax_p.text(0.015, 1.08, "", transform=ax_p.transAxes, ha="left", va="top",
                  fontsize=9.5, fontweight="bold", zorder=6,
                  bbox=dict(boxstyle="round,pad=0.3"))

# --- right axes: weight bars
WLABS = ["bias", "sin 2\u03c0", "cos 2\u03c0", "sin 4\u03c0", "cos 4\u03c0",
         "sin 6\u03c0", "cos 6\u03c0", "weekend", "presence", "lux", "temp", "hum"]
ax_w.set_xlim(-2, 2)
ax_w.set_xticks([-2, -1, 0, 1, 2])
ax_w.set_ylim(-0.7, 11.7)
ax_w.set_yticks(range(12))
ax_w.set_yticklabels(WLABS, fontsize=8.4)
ax_w.invert_yaxis()
ax_w.set_title("model weights w  (each update clipped to \u00b18)", fontsize=10, fontweight="bold")
ax_w.axvline(0, color=INK, lw=1.0)
bars = ax_w.barh(range(12), np.zeros(12), height=0.62,
                 color=[BLUE] * 12, edgecolor="none", zorder=3)

# ----------------------------------------------------------------------------
# render one frame
# ----------------------------------------------------------------------------
JIT = rng.normal(0, 0.12, N)          # per-event x-jitter so daily dots separate

def draw(i):
    """i = 0: before any action; i >= 1: after action i-1 was learned."""
    ww = w_hist[i]
    line.set_data(HH, curve(ww))
    if i >= 1:
        d, h, y, p_b, err = meta[i - 1]
        on_pts, off_pts = [], []
        for j, (dd, hh, yy, _) in enumerate(events[:i]):
            (on_pts if yy == 1 else off_pts).append(
                (hh + JIT[j], 1.07 if yy == 1 else -0.04))
        sct_on.set_offsets(np.asarray(on_pts, dtype=float).reshape(-1, 2))
        sct_off.set_offsets(np.asarray(off_pts, dtype=float).reshape(-1, 2))
        curVline.set_alpha(0.55)
        curVline.set_xdata([h, h])
        curMarker.set_data([h], [1.07 if y == 1 else -0.04])
        verb = "ON" if y == 1 else "OFF"
        logline.set_text(
            f"Day {d + 1} \u00b7 {int(h):02d}:{int((h % 1) * 60):02d} \u2014 user turned light "
            f"{verb}  (model p = {p_b:.2f},  err = {err:+.2f})  \u2192  one SGD step")
    else:
        sct_on.set_offsets(np.empty((0, 2)))
        sct_off.set_offsets(np.empty((0, 2)))
        curVline.set_alpha(0.0)
        curMarker.set_data([], [])
        logline.set_text("SHADOW mode \u2014 waiting for the first manual action\u2026")

    for b, wv in zip(bars, ww):
        b.set_width(wv)
        b.set_color(BLUE if wv >= 0 else RED)

    acc = acc_hist[i]
    acc_txt = f"{acc:.0f}%" if acc is not None else "\u2014"
    is_auto = auto_from is not None and i >= auto_from
    badge.set_text("mode: AUTO" if is_auto else "mode: SHADOW")
    badge.get_bbox_patch().set_facecolor("#DFF0E4" if is_auto else "#EFEFEF")
    badge.set_color(GREEN if is_auto else TXT)
    day = events[min(i, N - 1)][0] + 1 if i else 1
    counters.set_text(
        f"Day {day}/{DAYS}    actions: {i}    updates: {i}    "
        f"rolling accuracy (last {WINDOW}): {acc_txt}    "
        f"promotion gate: n \u2265 {N_MIN} and acc \u2265 {ACC_MIN:.0f}%")

frames = list(range(N + 1)) + [N] * 10  # short pause on the final state
anim = animation.FuncAnimation(fig, draw, frames=frames, interval=180, blit=False)
anim.save(OUT, writer=animation.PillowWriter(fps=5), savefig_kwargs={"facecolor": "white"})
plt.close(fig)

# Pillow's GIF encoder collapses identical consecutive frames, so give the
# final frame a long display duration instead of relying on duplicate frames
im = Image.open(OUT)
n = im.n_frames
durs = [180] * n
durs[-1] = 2200
frames_img = []
for i in range(n):
    im.seek(i)
    frames_img.append(im.convert("P", palette=Image.ADAPTIVE).copy())
im.close()
frames_img[0].save(OUT, save_all=True, append_images=frames_img[1:],
                   duration=durs, loop=0, optimize=False, disposal=2)

print(f"saved {OUT}  ({N} actions, promoted to AUTO at frame {auto_from})")
print("final weights:", np.round(w_hist[-1], 2))
for hh in (2.0, 6.5, 8.5, 12.0, 18.5, 23.2):
    print(f"  p(light ON | {hh:4.1f}h) = {predict(w_hist[-1], features(hh, False)):.2f}")
