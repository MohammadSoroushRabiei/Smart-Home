# -*- coding: utf-8 -*-
"""Generate the 6 report figures (English labels, clean engineering style)."""
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

def box(ax, x, y, w, h, title, lines=(), fill=FILL1, ec=INK, lw=1.4, title_size=10.5, dashed=False, ls=None):
    ls = "dashed" if dashed else (ls or "solid")
    p = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.06",
                       linewidth=lw, edgecolor=ec, facecolor=fill, linestyle=ls, zorder=2)
    ax.add_patch(p)
    cy = y + h - 0.16
    if title:
        ax.text(x + w/2, cy, title, ha="center", va="top", fontsize=title_size,
                fontweight="bold", color=INK, zorder=3)
        cy -= 0.17 + 0.145
    for ln in lines:
        ax.text(x + w/2, cy, ln, ha="center", va="top", fontsize=9.2, color="#4A5568", zorder=3)
        cy -= 0.145
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
# Figure 1: overall system architecture
# ----------------------------------------------------------------------------
fig, ax = canvas(11.5, 7.6)
H = 10 * 7.6 / 11.5  # ~6.6

# user devices (amber)
b_phone = box(ax, 0.25, 2.35, 2.30, 2.45, "User Phone",
              ("Browser + camera", "(web panel / face auth)", "HA Companion App"), fill=FILL3)
b_wifi = box(ax, 0.25, 0.45, 2.30, 1.30, "Wi-Fi Router", ("Local LAN", "no Internet needed"), fill=FILL3)

# device (blue)
b_esp = box(ax, 3.10, 1.35, 2.85, 4.55, "ESP32-S3 Device",
            ("Touch LCD 320x480", "(LVGL UI + QR)", "Face engine (ESP-DL)", "Relay lock / LED / Button",
             "BME280 (I2C)", "HTTPS server (on-board)"), fill=FILL1)

# home server (green)
b_srv = box(ax, 6.55, 2.60, 2.60, 3.30, "Home Server (Docker)",
            ("Mosquitto (MQTT)", "Home Assistant", "Attendance server", "(FastAPI + SQLite)"), fill=FILL2)

# optional internet (amber dashed)
b_int = box(ax, 6.55, 0.45, 2.60, 1.55, "Optional Internet",
            ("Google Sheets", "Bale bot"), fill="#FDF8EE", ec=DASH, dashed=True)

# arrows
arrow(ax, (2.55, 3.55), (3.10, 3.55), "HTTPS + QR")
arrow(ax, (2.55, 1.10), (3.35, 1.35), "LAN")
arrow(ax, (5.95, 4.25), (6.55, 4.25), "MQTT")
arrow(ax, (5.95, 2.30), (6.55, 2.90), "HTTPS\n(events)", lab_dy=0.05)
arrow(ax, (7.85, 2.60), (7.85, 2.00), "", color=DASH, ls="dashed")

ax.text(5.0, H - 0.28, "All core services run locally — data never leaves the home network",
        ha="center", fontsize=10.5, fontweight="bold", color=BLUE)
save(fig, "fig1_architecture.png")

# ----------------------------------------------------------------------------
# Figure 2: on-device face recognition pipeline
# ----------------------------------------------------------------------------
fig, ax = canvas(12.0, 4.9)
H = 10 * 4.9 / 12.0

y0, bh = 1.15, 1.55
bw, gap, x = 1.76, 0.20, 0.30
xs = []
labels = [
    ("Phone camera", ("getUserMedia", "JPEG <= 300 KB")),
    ("HTTPS upload", ("POST /api/face/", "recognize (TLS)")),
    ("HW JPEG decode", ("RGB565 320x240", "buffer in PSRAM")),
    ("ESP-DL pipeline", ("HumanFaceDetect", "+ embedding (CNN)")),
    ("Match + decision", ("cosine vs face DB", "threshold 0.70")),
]
boxes = []
for t, lns in labels:
    xs.append(x)
    boxes.append(box(ax, x, y0, bw, bh, t, lns, fill=FILL4, title_size=10))
    x += bw + gap
for i in range(4):
    arrow(ax, (xs[i] + bw, y0 + bh/2), (xs[i+1], y0 + bh/2))

box(ax, 3.10, 0.05, 1.85, 0.80, "Unlock / event", (), fill=FILL2, title_size=9.5)
box(ax, 5.20, 0.05, 1.85, 0.80, "Reject / retry", (), fill="#F8ECEC", title_size=9.5)
arrow(ax, (xs[4] + bw/2, y0), (4.30, 0.90), "", color="#3E7C4F")
arrow(ax, (xs[4] + bw/2, y0), (5.85, 0.90), "", color="#A94442")

ax.text(5.0, H - 0.10, "Entire pipeline executes on the ESP32-S3 — images are processed locally",
        ha="center", fontsize=10.5, fontweight="bold", color=BLUE)
save(fig, "fig2_face_pipeline.png")

# ----------------------------------------------------------------------------
# Figure 3: multi-channel user interfaces
# ----------------------------------------------------------------------------
fig, ax = canvas(11.0, 6.0)
H = 10 * 6.0 / 11.0

b_core = box(ax, 3.85, 3.30, 2.30, 1.50, "ESP32-S3", ("app_state + MQTT", "single source of truth"), fill=FILL1)

b1 = box(ax, 0.30, 3.05, 2.60, 1.85, "Touch LCD (on-device)",
         ("LVGL dashboard, keypad,", "settings, QR pages", "always available"), fill=FILL2)
b2 = box(ax, 0.30, 0.55, 2.60, 1.85, "Mobile Web Panel",
         ("QR scan -> HTTPS page", "camera face auth,", "light/fan/lock control"), fill=FILL3)
b3 = box(ax, 7.15, 3.05, 2.60, 1.85, "HA Web Dashboard",
         ("browser on LAN", "13 discovered entities,", "automations"), fill=FILL2)
b4 = box(ax, 7.15, 0.55, 2.60, 1.85, "HA Companion App",
         ("phone app over LAN", "same entities +", "notifications"), fill=FILL3)

arrow(ax, (2.90, 3.95), (3.85, 3.95), "LVGL", lab_dy=0.08)
arrow(ax, (2.90, 1.45), (3.85, 3.40), "HTTPS", lab_dx=-0.15)
arrow(ax, (6.15, 3.95), (7.15, 3.95), "MQTT", lab_dy=0.08)
arrow(ax, (6.15, 3.40), (7.15, 1.45), "MQTT", lab_dx=0.15)

ax.text(5.0, H - 0.18, "One state, four parallel user interfaces",
        ha="center", fontsize=11, fontweight="bold", color=BLUE)
save(fig, "fig3_ui_channels.png")

# ----------------------------------------------------------------------------
# Figure 4: attendance 3-layer architecture
# ----------------------------------------------------------------------------
fig, ax = canvas(11.5, 6.4)
H = 10 * 6.4 / 11.5

b_dev = box(ax, 0.25, 2.55, 2.55, 2.30, "Layer 1 — Device",
            ("QR on LCD (10 min", "token + countdown)", "face match (0.70)", "NVS outbox (32)", "60 s anti-spam"), fill=FILL1)
b_srv = box(ax, 3.55, 2.55, 2.90, 2.30, "Layer 2 — Local server",
            ("FastAPI (Docker)", "POST /api/event", "SQLite outbox table", "worker flush: 15 s"), fill=FILL2)
b_g = box(ax, 7.20, 3.55, 2.50, 1.55, "Google Sheets", ("archive, Jalali", "dates (Apps Script)"), fill="#FDF8EE", ec=DASH, dashed=True)
b_b = box(ax, 7.20, 1.65, 2.50, 1.55, "Bale bot", ("in/out + door", "notifications"), fill="#FDF8EE", ec=DASH, dashed=True)

arrow(ax, (2.80, 3.70), (3.55, 3.70), "HTTPS\n+ secret", lab_dy=0.02)
arrow(ax, (6.45, 4.35), (7.20, 4.35), "sync", color=DASH, ls="dashed", lab_dy=0.06)
arrow(ax, (6.45, 3.10), (7.20, 2.45), "notify", color=DASH, ls="dashed", lab_dy=0.06)

ax.text(5.0, H - 0.18, "Attendance stack independent from HA / MQTT; internet only for optional delivery",
        ha="center", fontsize=10, fontweight="bold", color=BLUE)
ax.text(8.45, 5.28, "optional internet", ha="center", fontsize=8.5, style="italic", color=DASH)
ax.text(5.0, 1.05, "Server keeps credentials; the board holds none",
        ha="center", fontsize=9.5, style="italic", color="#4A5568")
save(fig, "fig4_attendance.png")

# ----------------------------------------------------------------------------
# Figure 5: behavior-learning agent (shadow -> auto)
# ----------------------------------------------------------------------------
fig, ax = canvas(11.0, 5.4)
H = 10 * 5.4 / 11.0

b_sh = box(ax, 0.35, 2.55, 2.70, 1.90, "SHADOW mode",
           ("predict every 30 s,", "report to MQTT,", "evaluate — no action"), fill=FILL1)
b_auto = box(ax, 6.90, 2.55, 2.70, 1.90, "AUTO mode",
             ("act every 30 s:", "p >= 0.75 -> ON,", "p <= 0.25 -> OFF"), fill=FILL2)
b_gate = box(ax, 3.55, 2.80, 2.85, 1.50, "Promotion gate",
             (">= 15 decisions and", ">= 85% accuracy in", "last 20 decisions"), fill=FILL3)
b_train = box(ax, 3.55, 0.40, 2.85, 1.55, "Online learning",
              ("SGD on-chip, lr=0.08", "user actions = samples", "NVS save: 30 s"), fill=FILL4)

arrow(ax, (3.05, 3.55), (3.55, 3.55), "evaluate", lab_dy=0.08)
arrow(ax, (6.40, 3.55), (6.90, 3.55), "promote", lab_dy=0.08)
arrow(ax, (7.60, 2.55), (6.05, 1.95), "", color=GRAY)
arrow(ax, (3.90, 1.95), (1.70, 2.55), "", color=GRAY)

ax.text(5.0, H - 0.12, "Manual actions always execute immediately and are never overridden",
        ha="center", fontsize=10, fontweight="bold", color=BLUE)
ax.text(5.0, 0.10, "Door lock is never under model control (hard security rule)",
        ha="center", fontsize=9.5, style="italic", color="#A94442")
save(fig, "fig5_ml_agent.png")

# ----------------------------------------------------------------------------
# Figure 6: software stack
# ----------------------------------------------------------------------------
fig, ax = canvas(9.8, 6.2)
H = 10 * 6.2 / 9.8

layers = [
    ("Local infrastructure", "Docker: Mosquitto | Home Assistant | Attendance server (FastAPI + SQLite)", FILL2),
    ("Application modules", "http_server | face_recognition | ml_agent | attendance | app_state | lock | wifi/mqtt managers", FILL1),
    ("Libraries / components", "LVGL 9.5 | ESP-DL (detect + recognize) | esp_https_server | esp-mqtt | NVS | SPIFFS", FILL4),
    ("OS / framework", "ESP-IDF 6 + FreeRTOS (dual-core, tasks, queues, watchdogs)", FILL1),
    ("Hardware", "ESP32-S3 (N16R8) | ST7796 LCD 320x480 | GT911 touch | BME280 | relay / LED / button", "#F3F4F6"),
]
top, bh, gap = H - 0.48, 0.92, 0.28
for i, (t, sub, f) in enumerate(layers):
    y = top - i * (bh + gap) - bh
    box(ax, 1.30, y, 7.4, bh, t, (sub,), fill=f, title_size=11)
ax.text(5.0, H - 0.13, "Software stack (bottom = hardware, top = user-facing services)",
        ha="center", fontsize=10.5, fontweight="bold", color=BLUE)
save(fig, "fig6_stack.png")

# ----------------------------------------------------------------------------
# Figure 7: overall project block diagram (chapter 1)
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
box(ax, 6.85, 3.60, 2.95, 2.30, "Home Server (Docker)",
    ("Mosquitto (MQTT)", "Home Assistant", "Attendance server", "(FastAPI + SQLite)"), fill=FILL2, title_size=10.5)

# optional internet (amber dashed)
box(ax, 6.85, 1.35, 2.95, 1.70, "Optional Internet",
    ("Google Sheets archive", "Bale notifications"), fill="#FDF8EE", ec=DASH, dashed=True, title_size=10)

# arrows
arrow(ax, (2.20, 4.78), (3.05, 4.78), color=BLUE, lw=1.8)
ax.text(2.625, 5.50, "QR", ha="center", va="top", fontsize=8.2, color=BLUE, zorder=5)
ax.text(2.625, 5.22, "HTTPS", ha="center", va="top", fontsize=8.2, color=BLUE, zorder=5)
ax.text(2.625, 4.97, "face JPEG", ha="center", va="top", fontsize=8.2, color=BLUE, zorder=5)
arrow(ax, (6.35, 4.85), (6.85, 4.85), "MQTT", lab_dy=0.10, lab_size=8.5)
arrow(ax, (6.35, 3.95), (6.85, 3.95), "HTTPS", lab_dy=0.10, lab_size=8.5)
arrow(ax, (8.30, 3.60), (8.30, 3.05), "sync / notify", color=DASH, ls="dashed", lab_dx=0.78, lab_dy=0.02, lab_size=8.5)
ax.text(6.60, 6.15, "Wi-Fi LAN", fontsize=9, style="italic", color=GRAY, ha="center")

save(fig, "fig7_block_diagram.png")

print("all figures done")
