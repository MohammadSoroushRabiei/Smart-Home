# -*- coding: utf-8 -*-
"""Standalone detail figure: behavior-learning model drawn as a single-layer
neural network (logistic regression), with the exact formulas implemented in
main/ml_agent.c, main/ml_model.c and ml/train.py.

Not part of figs.py / the report build — kept separate on purpose.
Run:  python report/fig_ml_model.py   ->  report/figs/fig_ml_model.png
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Circle, Rectangle
import numpy as np
import os

OUT = os.path.join(os.path.dirname(__file__), "figs")
os.makedirs(OUT, exist_ok=True)

INK   = "#2E3B4E"
BLUE  = "#2F5D8C"
FILL1 = "#EDF2F8"
FILL2 = "#EAF3EC"
FILL3 = "#FBF3E4"
FILL4 = "#F4EEF7"
GRAY  = "#8A97A6"
GREEN = "#3E7C4F"
RED   = "#A94442"
TXT   = "#4A5568"

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "font.size": 10,
    "text.color": INK,
    "axes.edgecolor": INK,
})

fig, ax = plt.subplots(figsize=(14, 9))
ax.set_xlim(0, 14)
ax.set_ylim(0, 9)
ax.axis("off")


def rbox(x, y, w, h, fill, ec=INK, lw=1.4, dashed=False, z=2):
    p = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.06",
                       linewidth=lw, edgecolor=ec, facecolor=fill,
                       linestyle="dashed" if dashed else "solid", zorder=z)
    ax.add_patch(p)


def arrow(p1, p2, color=BLUE, lw=1.6, ls="solid"):
    a = FancyArrowPatch(p1, p2, arrowstyle="-|>", mutation_scale=13,
                        linewidth=lw, color=color, linestyle=ls, zorder=4,
                        shrinkA=0, shrinkB=0)
    ax.add_patch(a)


# ============================================================================
# header / footer notes
# ============================================================================
ax.text(7.0, 8.72, "User-Behavior Learning — Logistic Regression as a Single-Layer Neural Network",
        ha="center", va="center", fontsize=12.5, fontweight="bold", color=BLUE)
ax.text(7.0, 8.42, "as implemented in  main/ml_agent.c  ·  main/ml_model.c  ·  ml/train.py",
        ha="center", va="center", fontsize=9, style="italic", color=GRAY)
ax.text(7.0, 2.24, "Door lock is never under model control (hard security rule)",
        ha="center", va="center", fontsize=9.5, style="italic", color=RED)

# ============================================================================
# LEFT — input features
# ============================================================================
rbox(0.25, 2.45, 3.60, 5.70, FILL1)
ax.text(2.05, 7.93, "Input Features  ($\\mathbf{x}\\in\\mathbb{R}^{12}$)",
        ha="center", va="center", fontsize=10.5, fontweight="bold", color=INK)

feats = [
    (7.55, "$x_0=1$   (bias)", 9.6, INK),
    (7.22, "— cyclic time (h = fractional hour) —", 8.2, GRAY),
    (6.88, r"$x_{1,2}=\sin,\ \cos\left(\frac{2\pi h}{24}\right)$", 9.6, TXT),
    (6.53, r"$x_{3,4}=\sin,\ \cos\left(\frac{4\pi h}{24}\right)$", 9.6, TXT),
    (6.18, r"$x_{5,6}=\sin,\ \cos\left(\frac{6\pi h}{24}\right)$", 9.6, TXT),
    (5.85, "— calendar / presence —", 8.2, GRAY),
    (5.52, r"$x_7=$ weekend (Thu/Fri) $\in\{0,1\}$", 9.6, TXT),
    (5.19, r"$x_8=$ presence $\in\{0,1\}$", 9.6, TXT),
    (4.86, "— environment —", 8.2, GRAY),
    (4.52, r"$x_9=\min(\mathrm{lux},\,1000)/1000$", 9.6, TXT),
    (4.15, r"$x_{10}=\mathrm{clip}\left(\frac{T-15}{15},\,0,\,1\right)$", 9.6, TXT),
    (3.78, r"$x_{11}=\mathrm{clip}\left(\frac{\mathrm{RH}}{100},\,0,\,1\right)$", 9.6, TXT),
    (3.38, "Time is encoded on a circle — no", 8.0, GRAY),
    (3.16, "midnight discontinuity. Context-only:", 8.0, GRAY),
    (2.94, "device states are excluded from x.", 8.0, GRAY),
    (2.72, "T: °C · RH: % · lux: LDR", 8.0, GRAY),
]
for y, s, fs, c in feats:
    ax.text(0.50, y, s, ha="left", va="center", fontsize=fs,
            color=c, zorder=3, style="italic" if c == GRAY else "normal")

# ============================================================================
# MIDDLE — the network itself
# ============================================================================
rbox(4.05, 2.45, 4.90, 5.70, FILL4)
ax.text(6.50, 8.00, "Single-Layer Neural Network",
        ha="center", va="center", fontsize=10, fontweight="bold", color=INK)
ax.text(6.50, 7.76, "(one sigmoid neuron per device)",
        ha="center", va="center", fontsize=8.5, color=TXT)

# input nodes x0..x11
ys_in = np.linspace(7.42, 3.38, 12)
for k, yy in enumerate(ys_in):
    ax.add_patch(Circle((4.62, yy), 0.17, facecolor="white", edgecolor=INK,
                        linewidth=1.0, zorder=5))
    ax.text(4.62, yy, "$x_{%d}$" % k, ha="center", va="center", fontsize=7.2, zorder=6)

# neurons
L = (6.40, 6.40)   # light head
F = (6.40, 4.05)   # fan head
R = 0.42
for c, col in ((L, BLUE), (F, GREEN)):
    ax.add_patch(Circle(c, R, facecolor="white", edgecolor=col, linewidth=1.8, zorder=5))
    ax.text(c[0], c[1], r"$\sigma$", ha="center", va="center", fontsize=16,
            color=col, zorder=6)

# edges input -> neurons
for c, col in ((L, BLUE), (F, GREEN)):
    for yy in ys_in:
        x0, y0 = 4.62, yy
        x1, y1 = c
        d = np.hypot(x1 - x0, y1 - y0)
        ux, uy = (x1 - x0) / d, (y1 - y0) / d
        ax.plot([x0 + 0.18 * ux, x1 - (R + 0.03) * ux],
                [y0 + 0.18 * uy, y1 - (R + 0.03) * uy],
                color=col, lw=0.75, alpha=0.45, zorder=3)

# head labels
ax.text(6.40, 7.00, "light — $\\mathbf{w}^{(L)}\\!\\in\\mathbb{R}^{12}$",
        ha="center", va="center", fontsize=8.8, color=BLUE, zorder=6,
        bbox=dict(boxstyle="round,pad=0.15", fc="white", ec="none"))
ax.text(6.40, 3.46, "fan — $\\mathbf{w}^{(F)}\\!\\in\\mathbb{R}^{12}$",
        ha="center", va="center", fontsize=8.8, color=GREEN, zorder=6,
        bbox=dict(boxstyle="round,pad=0.15", fc="white", ec="none"))

# outputs
for c, col, lab, sub in ((L, BLUE, "$p_L$", "P(light ON)"), (F, GREEN, "$p_F$", "P(fan ON)")):
    oc = (7.62, c[1])
    ax.add_patch(Circle(oc, 0.30, facecolor="white", edgecolor=col, linewidth=1.6, zorder=5))
    ax.text(oc[0], oc[1], lab, ha="center", va="center", fontsize=10, color=col, zorder=6)
    arrow((c[0] + R + 0.03, c[1]), (oc[0] - 0.32, c[1]), color=col, lw=1.4)
    ax.text(8.02, c[1], sub, ha="left", va="center", fontsize=8.4, color=TXT, zorder=6)

# prediction formulas (per head)
ax.text(6.50, 2.98, r"$z=\sum_{i=0}^{11} w_i\,x_i\ ,\ \ \ z\leftarrow\mathrm{clip}(z,\ \pm 30)$",
        ha="center", va="center", fontsize=9.6, color=INK, zorder=6)
ax.text(6.50, 2.62, r"$p=\sigma(z)=\frac{1}{1+e^{-z}}$",
        ha="center", va="center", fontsize=10.5, color=INK, zorder=6)

arrow((3.87, 5.30), (4.03, 5.30))
arrow((8.88, 6.40), (9.13, 6.40))

# ============================================================================
# RIGHT TOP — decision rule + sigmoid plot
# ============================================================================
rbox(9.15, 4.75, 4.60, 3.40, FILL1)
ax.text(11.45, 7.93, "Decision Rule — every 30 s in AUTO mode",
        ha="center", va="center", fontsize=10, fontweight="bold", color=INK)
dec = [
    (7.52, r"$p\geq 0.75\ \Rightarrow$ switch ON (if off)"),
    (7.20, r"$p\leq 0.25\ \Rightarrow$ switch OFF (if on)"),
    (6.88, r"$0.25<p<0.75\ \Rightarrow$ hold (no action)"),
]
for y, s in dec:
    ax.text(9.45, y, s, ha="left", va="center", fontsize=9.6, color=TXT, zorder=3)

# sigmoid mini-plot drawn in canvas coordinates
zx = lambda z: 9.80 + (z + 6) / 12 * 3.55
py = lambda p: 5.05 + p * 1.55
zc = np.log(3.0)  # sigma(z) = 0.75 / 0.25

ax.add_patch(Rectangle((zx(zc), 5.05), zx(6) - zx(zc), 1.55, facecolor=GREEN, alpha=0.13, zorder=3))
ax.add_patch(Rectangle((9.80, 5.05), zx(-zc) - 9.80, 1.55, facecolor=RED, alpha=0.13, zorder=3))

zs = np.linspace(-6, 6, 200)
ax.plot(zx(zs), py(1 / (1 + np.exp(-zs))), color=BLUE, lw=2.0, zorder=5)

for p in (0.75, 0.25):
    ax.plot([9.80, 13.35], [py(p), py(p)], color=GRAY, lw=0.9, ls=(0, (4, 3)), zorder=4)
ax.plot([zx(zc), zx(zc)], [5.05, py(0.75)], color=GRAY, lw=0.9, ls=(0, (4, 3)), zorder=4)
ax.plot([zx(-zc), zx(-zc)], [5.05, py(0.25)], color=GRAY, lw=0.9, ls=(0, (4, 3)), zorder=4)

ax.plot([9.80, 13.35], [5.05, 5.05], color=INK, lw=1.0, zorder=4)   # z axis
ax.plot([9.80, 9.80], [5.05, 6.60], color=INK, lw=1.0, zorder=4)   # p axis
for p, lab in ((1.0, "1.0"), (0.75, "0.75"), (0.5, "0.5"), (0.25, "0.25"), (0.0, "0")):
    ax.text(9.70, py(p), lab, ha="right", va="center", fontsize=6.8, color=TXT)
for z, lab in ((-6, "−6"), (0, "0"), (6, "+6")):
    ax.text(zx(z), 4.92, lab, ha="center", va="center", fontsize=6.8, color=TXT)
ax.text(13.44, 5.05, "z", ha="left", va="center", fontsize=9, style="italic", color=INK)
ax.text(9.32, 5.82, r"$p=\sigma(z)$", ha="center", va="center", fontsize=8.5, color=INK,
        rotation=90)
ax.text(12.62, py(0.88), "ON", ha="center", va="center", fontsize=9, fontweight="bold", color=GREEN)
ax.text(10.53, py(0.10), "OFF", ha="center", va="center", fontsize=9, fontweight="bold", color=RED)

# ============================================================================
# RIGHT BOTTOM — promotion gate
# ============================================================================
rbox(9.15, 2.45, 4.60, 2.10, FILL3)
ax.text(11.45, 4.28, "SHADOW → AUTO Promotion Gate",
        ha="center", va="center", fontsize=9.8, fontweight="bold", color=INK)
gate = [
    (3.90, "rolling window = last 20 decisions (bit ring)"),
    (3.56, r"$\mathrm{acc}=100\cdot n_{\mathrm{hit}}/n$   (%)"),
    (3.22, r"promote when  $n\geq 15$  and  acc $\geq 85\%$"),
    (2.88, "light and fan promote independently"),
]
for y, s in gate:
    ax.text(9.45, y, s, ha="left", va="center", fontsize=9.3, color=TXT, zorder=3)
arrow((11.45, 4.75), (11.45, 4.57), color=GRAY, lw=1.2)

# ============================================================================
# BOTTOM — learning band
# ============================================================================
rbox(0.25, 0.30, 4.55, 1.75, FILL2)
ax.text(2.525, 1.87, "Offline Pretraining — PC (ml/train.py)",
        ha="center", va="center", fontsize=9.8, fontweight="bold", color=INK)
ax.text(2.525, 1.46, r"$w\leftarrow w-\alpha_t\left(\frac{1}{n}X^{T}(\sigma(Xw)-y)+\lambda w\right)$",
        ha="center", va="center", fontsize=9.8, color=INK, zorder=3)
ax.text(2.525, 1.11, r"$\alpha_t=1.0\cdot 0.9995^{\,\mathrm{epoch}}$,   $\lambda=3\times10^{-5}$,   1500 epochs",
        ha="center", va="center", fontsize=9.0, color=TXT, zorder=3)
ax.text(2.525, 0.76, "6912 synthetic samples · val: light 94.2%, fan 93.6%",
        ha="center", va="center", fontsize=8.6, color=TXT, zorder=3)

rbox(5.00, 0.30, 4.55, 1.75, FILL4)
ax.text(7.275, 1.87, "Online Learning — on-chip SGD (ml_model.c)",
        ha="center", va="center", fontsize=9.8, fontweight="bold", color=INK)
ax.text(7.275, 1.46, r"manual action = one training sample $(x,\ y\in\{0,1\})$",
        ha="center", va="center", fontsize=9.0, color=TXT, zorder=3)
ax.text(7.275, 1.11, r"$w_i\leftarrow\mathrm{clip}\left(w_i+\eta\,(y-p)\,x_i,\ \pm 8\right)$,   $\eta=0.08$",
        ha="center", va="center", fontsize=9.8, color=INK, zorder=3)
ax.text(7.275, 0.76, "ML's own actions are skipped — no self-feedback",
        ha="center", va="center", fontsize=8.6, color=TXT, zorder=3)

rbox(9.75, 0.30, 4.00, 1.75, "#F3F4F6")
ax.text(11.75, 1.87, "Persistence — NVS  \"mlbrain\"",
        ha="center", va="center", fontsize=9.8, fontweight="bold", color=INK)
ax.text(11.75, 1.46, "blobs: 2 × 12 floats + update counters (ver 2)",
        ha="center", va="center", fontsize=8.6, color=TXT, zorder=3)
ax.text(11.75, 1.11, "saved at most once per 30 s, only when dirty",
        ha="center", va="center", fontsize=8.6, color=TXT, zorder=3)
ax.text(11.75, 0.76, "on mismatch → built-in pretrained weights",
        ha="center", va="center", fontsize=8.6, color=TXT, zorder=3)

arrow((4.82, 1.18), (5.00, 1.18), lw=1.3)
arrow((9.57, 1.18), (9.75, 1.18), lw=1.3)

# ============================================================================
fig.savefig(os.path.join(OUT, "fig_ml_model.png"), dpi=200, bbox_inches="tight",
            facecolor="white")
plt.close(fig)
print("saved fig_ml_model.png")
