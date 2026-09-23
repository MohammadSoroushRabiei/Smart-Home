# -*- coding: utf-8 -*-
"""Generate the 7 report figures (English labels, clean engineering style)."""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
import os

OUT = os.path.join(os.path.dirname(__file__), "figs")
os.makedirs(OUT, exist_ok=True)

# palette
INK    = "#2E3B4E"   # border / text
BLUE   = "#2F5D8C"   # accent
FILL1  = "#EDF2F8"   # light blue fill (device)
FILL2  = "#EAF3EC"   # light green fill (local infra)
FILL3  = "#FBF3E4"   # light amber fill (user devices)
FILL4  = "#F4EEF7"   # light purple (AI)
GRAY   = "#8A97A6"
DASH   = "#B98A2F"

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "font.size": 10.5,
    "text.color": INK,
    "axes.edgecolor": INK,
})

def box(ax, x, y, w, h, title, lines=(), fill=FILL1, ec=INK, lw=1.4, title_size=10.5, dashed=False, ls=None, gap=0.145, line_fs=9.2):
    ls = "dashed" if dashed else (ls or "solid")
    p = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.06",
                       linewidth=lw, edgecolor=ec, facecolor=fill, linestyle=ls, zorder=2)
    ax.add_patch(p)
    cy = y + h - 0.16
    if title:
        ax.text(x + w/2, cy, title, ha="center", va="top", fontsize=title_size,
                fontweight="bold", color=INK, zorder=3)
        cy -= 0.17 + gap
    for ln in lines:
        ax.text(x + w/2, cy, ln, ha="center", va="top", fontsize=line_fs, color="#4A5568", zorder=3)
        cy -= gap
    return (x, y, w, h)

def arrow(ax, p1, p2, label="", color=BLUE, lw=1.6, style="-|>", ls="solid", lab_dx=0.0, lab_dy=0.10, lab_size=8.8):
    a = FancyArrowPatch(p1, p2, arrowstyle=style, mutation_scale=14, linewidth=lw,
                        color=color, linestyle=ls, zorder=4, shrinkA=2, shrinkB=2)
    ax.add_patch(a)
    if label:
        mx, my = (p1[0]+p2[0])/2 + lab_dx, (p1[1]+p2[1])/2 + lab_dy
        ax.text(mx, my, label, ha="center", va="bottom", fontsize=lab_size, color=color, zorder=5)

def canvas(w, h):
    fig, ax = plt.subplots(figsize=(w, h))
    ax.set_xlim(0, 10); ax.set_ylim(0, 10 * h / w)
    ax.axis("off")
    return fig, ax

def save(fig, name):
    fig.savefig(os.path.join(OUT, name), dpi=200, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print("saved", name)

# ----------------------------------------------------------------------------
# Figure 1: overall system architecture  (report Figure 5-1)
# ----------------------------------------------------------------------------
fig, ax = canvas(11.5, 7.4)
H = 10 * 7.4 / 11.5  # ~6.43

# user phone (amber) — top-aligned with the server band
box(ax, 0.25, 3.55, 2.30, 1.95, "User Phone",
    ("Browser + camera", "(web panel / face auth)", "HA Companion App"),
    fill=FILL3, gap=0.26)

# wi-fi router (amber)
box(ax, 0.25, 1.15, 2.30, 1.30, "Wi-Fi Router",
    ("Local LAN", "no Internet needed"), fill=FILL3, gap=0.26)

# device (blue) — top-aligned
box(ax, 3.10, 2.60, 2.85, 2.90, "ESP32-S3 Device",
    ("Touch LCD 320x480", "(LVGL UI + QR)", "Face engine (ESP-DL)",
     "Relay lock / LED / Button", "BME280 (I2C)", "HTTPS server (on-board)"),
    fill=FILL1, gap=0.26)

# home server (green) — top-aligned
box(ax, 6.55, 3.30, 2.60, 2.20, "Home Server (Docker)",
    ("Mosquitto (MQTT)", "Home Assistant", "Attendance server", "(FastAPI + SQLite)"),
    fill=FILL2, gap=0.26)

# optional internet (amber dashed)
box(ax, 6.55, 1.05, 2.60, 1.30, "Optional Internet",
    ("Google Sheets", "Bale bot"), fill="#FDF8EE", ec=DASH, dashed=True, gap=0.26)

# arrows
arrow(ax, (2.55, 4.60), (3.10, 4.60), "HTTPS\n+ QR", lab_dy=0.10, lab_size=8.5)
arrow(ax, (2.55, 1.80), (3.10, 2.60), "LAN", lab_dx=-0.12, lab_size=8.5)
arrow(ax, (5.95, 4.65), (6.55, 4.65), "MQTT", lab_dy=0.10, lab_size=8.5)
arrow(ax, (5.95, 3.70), (6.55, 3.70), "HTTPS", lab_dy=0.10, lab_size=8.5)
arrow(ax, (7.85, 3.30), (7.85, 2.35), "sync / notify", color=DASH, ls="dashed",
      lab_dx=0.80, lab_dy=0.02, lab_size=8.5)
ax.text(4.52, 5.80, "Wi-Fi LAN", fontsize=9, style="italic", color=GRAY, ha="center")

ax.text(5.0, H - 0.14, "All core services run locally — data never leaves the home network",
        ha="center", fontsize=10.5, fontweight="bold", color=BLUE)
save(fig, "fig1_architecture.png")

# ----------------------------------------------------------------------------
# Figure 2: on-device face recognition pipeline  (report Figure 6-1)
# ----------------------------------------------------------------------------
fig, ax = canvas(12.2, 5.6)
H = 10 * 5.6 / 12.2  # ~4.59

y0, bh = 2.05, 1.55
bw, gp = 1.76, 0.16
xs = []
labels = [
    ("Phone camera", ("getUserMedia", "JPEG <= 300 KB")),
    ("HTTPS upload", ("POST /api/face/", "recognize (TLS)")),
    ("HW JPEG decode", ("RGB565 320x240", "buffer in PSRAM")),
    ("ESP-DL pipeline", ("HumanFaceDetect", "+ embedding (CNN)")),
    ("Match + decision", ("cosine vs face DB", "threshold 0.70")),
]
x = (10.0 - (5 * bw + 4 * gp)) / 2
for t, lns in labels:
    xs.append(x)
    box(ax, x, y0, bw, bh, t, lns, fill=FILL4, title_size=10, gap=0.26)
    x += bw + gp
for i in range(4):
    arrow(ax, (xs[i] + bw, y0 + bh/2), (xs[i+1], y0 + bh/2))

box(ax, 7.60, 0.30, 1.80, 0.85, "Unlock / event", (), fill=FILL2, title_size=9.5)
box(ax, 5.45, 0.30, 1.80, 0.85, "Reject / retry", (), fill="#F8ECEC", title_size=9.5)
arrow(ax, (xs[4] + bw/2, y0), (8.50, 1.15), "", color="#3E7C4F")
arrow(ax, (xs[4] + bw/2, y0), (6.35, 1.15), "", color="#A94442")

ax.text(6.0, H - 0.10, "Entire pipeline executes on the ESP32-S3 — images are processed locally",
        ha="center", fontsize=10.5, fontweight="bold", color=BLUE)
save(fig, "fig2_face_pipeline.png")

# ----------------------------------------------------------------------------
# Figure 3: multi-channel user interfaces  (report Figure 7-1)
# ----------------------------------------------------------------------------
fig, ax = canvas(11.0, 6.2)
H = 10 * 6.2 / 11.0  # ~5.64

box(ax, 3.85, 3.45, 2.30, 1.55, "ESP32-S3",
    ("app_state + MQTT", "single source of truth"), fill=FILL1, gap=0.28)

box(ax, 0.30, 3.20, 2.60, 1.80, "Touch LCD (on-device)",
    ("LVGL dashboard, keypad,", "settings, QR pages", "always available"),
    fill=FILL2, gap=0.26)
box(ax, 0.30, 0.55, 2.60, 1.80, "Mobile Web Panel",
    ("QR scan -> HTTPS page", "camera face auth,", "light/fan/lock control"),
    fill=FILL3, gap=0.26)
box(ax, 7.10, 3.20, 2.60, 1.80, "HA Web Dashboard",
    ("browser on LAN", "13 discovered entities,", "automations"),
    fill=FILL2, gap=0.26)
box(ax, 7.10, 0.55, 2.60, 1.80, "HA Companion App",
    ("phone app over LAN", "same entities +", "notifications"),
    fill=FILL3, gap=0.26)

arrow(ax, (2.90, 4.10), (3.85, 4.10), "LVGL", lab_dy=0.10, lab_size=8.5)
arrow(ax, (2.90, 1.45), (3.85, 3.55), "HTTPS", lab_dx=-0.18, lab_size=8.5)
arrow(ax, (6.15, 4.10), (7.10, 4.10), "MQTT", lab_dy=0.10, lab_size=8.5)
arrow(ax, (6.15, 3.55), (7.10, 1.45), "MQTT", lab_dx=0.18, lab_size=8.5)

ax.text(5.0, H - 0.14, "One state, four parallel user interfaces",
        ha="center", fontsize=11, fontweight="bold", color=BLUE)
save(fig, "fig3_ui_channels.png")

# ----------------------------------------------------------------------------
# Figure 4: attendance 3-layer architecture  (report Figure 8-1)
# ----------------------------------------------------------------------------
fig, ax = canvas(11.5, 6.6)
H = 10 * 6.6 / 11.5  # ~5.74

box(ax, 0.25, 2.55, 2.55, 2.30, "Layer 1 — Device",
    ("QR on LCD (10 min", "token + countdown)", "face match (0.70)",
     "NVS outbox (32)", "60 s anti-spam"), fill=FILL1, gap=0.24)
box(ax, 3.55, 2.55, 2.90, 2.30, "Layer 2 — Local server",
    ("FastAPI (Docker)", "POST /api/event", "SQLite outbox table", "worker flush: 15 s"),
    fill=FILL2, gap=0.26)
box(ax, 7.20, 3.55, 2.50, 1.45, "Google Sheets",
    ("archive, Jalali", "dates (Apps Script)"), fill="#FDF8EE", ec=DASH, dashed=True, gap=0.28)
box(ax, 7.20, 1.55, 2.50, 1.45, "Bale bot",
    ("in/out + door", "notifications"), fill="#FDF8EE", ec=DASH, dashed=True, gap=0.28)

arrow(ax, (2.80, 3.70), (3.55, 3.70), "HTTPS\n+ secret", lab_dy=0.06, lab_size=8.5)
arrow(ax, (6.45, 4.30), (7.20, 4.30), "sync", color=DASH, ls="dashed", lab_dy=0.10, lab_size=8.5)
arrow(ax, (6.45, 3.10), (7.20, 2.40), "notify", color=DASH, ls="dashed", lab_dy=0.10, lab_size=8.5)

ax.text(8.45, 5.18, "optional internet", ha="center", fontsize=8.5, style="italic", color=DASH)
ax.text(5.0, H - 0.14, "Attendance stack independent from HA / MQTT; internet only for optional delivery",
        ha="center", fontsize=10, fontweight="bold", color=BLUE)
ax.text(5.0, 0.75, "Server keeps credentials; the board holds none",
        ha="center", fontsize=9.5, style="italic", color="#4A5568")
save(fig, "fig4_attendance.png")

# ----------------------------------------------------------------------------
# Figure 5: behavior-learning agent (shadow -> auto)  (report Figure 6-2)
# ----------------------------------------------------------------------------
fig, ax = canvas(11.0, 5.8)
H = 10 * 5.8 / 11.0  # ~5.27

box(ax, 0.35, 2.85, 2.65, 1.70, "SHADOW mode",
    ("predict every 30 s,", "report to MQTT,", "evaluate — no action"), fill=FILL1, gap=0.26)
box(ax, 7.10, 2.85, 2.65, 1.70, "AUTO mode",
    ("act every 30 s:", "p >= 0.75 -> ON,", "p <= 0.25 -> OFF"), fill=FILL2, gap=0.26)
box(ax, 3.70, 2.85, 2.70, 1.70, "Promotion gate",
    (">= 15 decisions and", ">= 85% accuracy in", "last 20 decisions"), fill=FILL3, gap=0.26)
box(ax, 3.70, 0.55, 2.70, 1.70, "Online learning",
    ("SGD on-chip, lr=0.08", "user actions = samples", "NVS save: 30 s"), fill=FILL4, gap=0.26)

arrow(ax, (3.00, 3.70), (3.70, 3.70), "evaluate", lab_dy=0.12, lab_size=8.2)
arrow(ax, (6.40, 3.70), (7.10, 3.70), "promote", lab_dy=0.12, lab_size=8.2)
arrow(ax, (8.42, 2.85), (6.10, 2.25), "", color=GRAY)
arrow(ax, (4.00, 2.25), (1.70, 2.85), "", color=GRAY)

ax.text(5.0, H - 0.12, "Manual actions always execute immediately and are never overridden",
        ha="center", fontsize=10, fontweight="bold", color=BLUE)
ax.text(5.0, 0.15, "Door lock is never under model control (hard security rule)",
        ha="center", fontsize=9.5, style="italic", color="#A94442")
save(fig, "fig5_ml_agent.png")

# ----------------------------------------------------------------------------
# Figure 6: software stack  (report Figure 4-1)
# ----------------------------------------------------------------------------
fig, ax = canvas(10.0, 7.1)
H = 7.1

layers = [
    ("Local infrastructure",
     ("Docker: Mosquitto | Home Assistant", "Attendance server (FastAPI + SQLite)"), FILL2),
    ("Application modules",
     ("http_server | face_recognition | ml_agent | attendance",
      "app_state | lock | wifi_manager | mqtt_manager | display | bme280"), FILL1),
    ("Libraries / components",
     ("LVGL 9.5 | ESP-DL (face detect + recognize)",
      "esp_https_server | esp-mqtt | NVS | SPIFFS"), FILL4),
    ("OS / framework",
     ("ESP-IDF 6 + FreeRTOS", "(dual-core, tasks, queues, watchdogs)"), FILL1),
    ("Hardware",
     ("ESP32-S3 (N16R8) | ST7796 LCD 320x480 | GT911 touch",
      "BME280 | relay / LED / button"), "#F3F4F6"),
]
top, bh, gapx = 6.55, 1.08, 0.18
for i, (t, subs, f) in enumerate(layers):
    y = top - i * (bh + gapx) - bh
    box(ax, 0.60, y, 8.80, bh, t, subs, fill=f, title_size=11, gap=0.22, line_fs=9.0)
ax.text(5.0, H - 0.16, "Software stack (bottom = hardware, top = user-facing services)",
        ha="center", fontsize=10.5, fontweight="bold", color=BLUE)
save(fig, "fig6_stack.png")

# ----------------------------------------------------------------------------
# Figure 7: overall project block diagram (chapter 1)  (report Figure 1-1)
# ----------------------------------------------------------------------------
def sub_block(ax, x, y, w, h, text, fill="#FFFFFF", fs=8.8):
    p = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.05",
                       linewidth=1.0, edgecolor=INK, facecolor=fill, zorder=3)
    ax.add_patch(p)
    ax.text(x + w/2, y + h/2, text, ha="center", va="center", fontsize=fs, color=INK, zorder=4)

fig, ax = canvas(11.5, 8.0)
H = 10 * 8.0 / 11.5  # ~6.96

ax.text(5.0, H - 0.14, "All intelligence runs on the edge node — the phone only lends its camera, the servers stay local",
        ha="center", fontsize=10, fontweight="bold", color=BLUE)

# user phone (amber, top-left) — aligned with the Home Server band on the right
px, py, pw, ph = 0.15, 3.60, 2.05, 2.30
_ph_box = FancyBboxPatch((px, py), pw, ph, boxstyle="round,pad=0.02,rounding_size=0.06",
                         linewidth=1.4, edgecolor=INK, facecolor=FILL3, zorder=2)
ax.add_patch(_ph_box)
ax.text(px + pw/2, py + ph - 0.28, "User Smartphone", ha="center", va="center",
        fontsize=10.5, fontweight="bold", color=INK, zorder=3)
ax.text(px + pw/2, py + ph - 0.80, "Browser + camera (QR)", ha="center", va="center",
        fontsize=8.8, color="#4A5568", zorder=3)
ax.text(px + pw/2, py + ph - 1.32, "HA Companion App", ha="center", va="center",
        fontsize=8.8, color="#4A5568", zorder=3)

# edge node (big block with internal functional sub-blocks)
box(ax, 3.05, 0.75, 3.30, 5.30, "", (), fill=FILL1, lw=1.8)
ax.text(4.70, 5.78, "ESP32-S3  —  Edge Node", ha="center", va="center",
        fontsize=11.5, fontweight="bold", color=INK)
subs = [
    ("LVGL UI Engine", 5.00),
    ("Face Recognition (ESP-DL)", 4.32),
    ("Behavior-Learning Agent", 3.64),
    ("HTTPS Server (TLS)", 2.96),
    ("MQTT Client", 2.28),
    ("State + NVS / SPIFFS", 1.60),
]
for txt, yy in subs:
    sub_block(ax, 3.25, yy, 2.90, 0.55, txt)

# peripherals (left column, below the phone) — horizontal arrows into the node edge
per = [
    ("Touch LCD + GT911 (I2C)", 2.85),
    ("BME280 Sensor (I2C)", 2.16),
    ("Door-Lock Relay", 1.47),
    ("LED + Button", 0.78),
]
for txt, yy in per:
    sub_block(ax, 0.15, yy, 2.45, 0.55, txt, fill="#F3F4F6", fs=8.4)
    arrow(ax, (2.60, yy + 0.275), (3.05, yy + 0.275), color=GRAY, lw=1.2)

# home server (green)
box(ax, 6.85, 3.60, 2.95, 2.00, "Home Server (Docker)",
    ("Mosquitto (MQTT)", "Home Assistant", "Attendance server", "(FastAPI + SQLite)"),
    fill=FILL2, title_size=10.5, gap=0.22)

# optional internet (amber dashed)
box(ax, 6.85, 1.55, 2.95, 1.40, "Optional Internet",
    ("Google Sheets archive", "Bale notifications"), fill="#FDF8EE", ec=DASH, dashed=True, title_size=10, gap=0.24)

# arrows
arrow(ax, (2.20, 4.78), (3.05, 4.78), color=BLUE, lw=1.8)
ax.text(2.625, 5.50, "QR", ha="center", va="top", fontsize=8.2, color=BLUE, zorder=5)
ax.text(2.625, 5.22, "HTTPS", ha="center", va="top", fontsize=8.2, color=BLUE, zorder=5)
ax.text(2.625, 4.97, "face JPEG", ha="center", va="top", fontsize=8.2, color=BLUE, zorder=5)
arrow(ax, (6.35, 4.85), (6.85, 4.85), "MQTT", lab_dy=0.10, lab_size=8.5)
arrow(ax, (6.35, 3.95), (6.85, 3.95), "HTTPS", lab_dy=0.10, lab_size=8.5)
arrow(ax, (8.30, 3.60), (8.30, 2.95), "sync / notify", color=DASH, ls="dashed", lab_dx=0.78, lab_dy=0.02, lab_size=8.5)
ax.text(6.60, 6.15, "Wi-Fi LAN", fontsize=9, style="italic", color=GRAY, ha="center")

save(fig, "fig7_block_diagram.png")

print("all figures done")
