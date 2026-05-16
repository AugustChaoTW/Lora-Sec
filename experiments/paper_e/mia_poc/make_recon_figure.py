#!/usr/bin/env python3
"""
Generate publication-quality figure comparing original vs reconstructed images
for MIA attacks on MobileNetV3-Small 128-d embeddings.

Layout: 3 attacks (Fredrikson | DLG | InversionNet) × 3 samples × 2 rows (Orig | Recon)
Output: results/fig_mia_recon.pdf and results/fig_mia_recon.png
"""

import json
import os
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from PIL import Image

# ── Paths ─────────────────────────────────────────────────────────────────────
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RECON_DIR  = os.path.join(SCRIPT_DIR, "results", "recon")
JSON_PATH  = os.path.join(SCRIPT_DIR, "results", "mia_results.json")
OUT_PDF    = os.path.join(SCRIPT_DIR, "results", "fig_mia_recon.pdf")
OUT_PNG    = os.path.join(SCRIPT_DIR, "results", "fig_mia_recon.png")

BACKBONE   = "mobilenet_v3_small"
DIM        = 128

# Sample indices chosen for visual variety (spread across dataset)
SAMPLE_INDICES = [0, 3, 6]

ATTACKS = [
    ("fredrikson",    "Fredrikson"),
    ("dlg",           "DLG"),
    ("inversion_net", "InversionNet"),
]

# ── Load SSIM values from JSON ─────────────────────────────────────────────────
with open(JSON_PATH) as f:
    all_results = json.load(f)

# Build lookup: (backbone, attack) → {sample_idx: ssim}
ssim_lookup = {}
for entry in all_results:
    if entry["backbone"] != BACKBONE or entry["dim"] != DIM:
        continue
    attack_key = entry["attack"]
    ssim_lookup[attack_key] = {s["sample_idx"]: s["ssim"] for s in entry["samples"]}

# ── Figure setup ───────────────────────────────────────────────────────────────
# Layout: 3 attack-groups, each group = 3 samples side-by-side
# Rows: 0=Original, 1=Reconstructed
# We use a GridSpec with group separators implemented via subplot spacing.
#
# Actual column plan (18 image columns total, separated into 3 groups of 6):
#   [fred_s0_orig | fred_s0_recon | fred_s3_orig | fred_s3_recon | ... ]
# But to keep attack groups visually distinct we use a wider figure and
# explicit column-width ratios with small gap columns.

N_ATTACKS  = len(ATTACKS)
N_SAMPLES  = len(SAMPLE_INDICES)
IMG_COLS   = N_ATTACKS * N_SAMPLES  # 9 image columns

# We embed pairs (orig, recon) in sub-panels grouped by attack.
# Final grid: 2 rows × (3 attacks × 3 samples) image cells
# We'll build 3 separate GridSpec sub-grids inside one figure.

FIG_W = 7.16   # IEEE double-column width in inches
FIG_H = 2.8    # compact height

fig = plt.figure(figsize=(FIG_W, FIG_H))

# One outer GridSpec: 1 row × N_ATTACKS columns (one column per attack group)
outer_gs = gridspec.GridSpec(
    1, N_ATTACKS,
    figure=fig,
    left=0.01, right=0.99,
    top=0.82, bottom=0.05,
    wspace=0.10,          # gap between attack groups
)

FONT_SIZE_LABEL  = 6.5
FONT_SIZE_SSIM   = 5.5
FONT_SIZE_HEADER = 7.5
FONT_SIZE_ROW    = 6.5

# Attack-group column headers (placed above the outer_gs)
attack_header_y = 0.93   # in figure-fraction coordinates

for atk_col, (attack_key, attack_label) in enumerate(ATTACKS):
    # Compute x-center of this attack group in figure coordinates
    col_left  = outer_gs.get_subplot_params().left
    col_right = outer_gs.get_subplot_params().right
    col_width = (col_right - col_left) / N_ATTACKS
    x_center  = col_left + atk_col * col_width + col_width / 2
    fig.text(
        x_center, attack_header_y,
        attack_label,
        ha="center", va="bottom",
        fontsize=FONT_SIZE_HEADER,
        fontweight="bold",
    )

# Row labels (Orig / Recon) placed at far left
ROW_LABELS = ["Original", "Reconstructed"]
row_label_x = 0.002

# We'll compute row y-centers after creating subplots.
all_axes = []   # shape: [atk_col][row][sample]

for atk_col, (attack_key, attack_label) in enumerate(ATTACKS):
    inner_gs = gridspec.GridSpecFromSubplotSpec(
        2, N_SAMPLES,
        subplot_spec=outer_gs[atk_col],
        wspace=0.04,
        hspace=0.08,
    )
    row_axes = [[], []]
    for row in range(2):
        for s_idx, sample_num in enumerate(SAMPLE_INDICES):
            ax = fig.add_subplot(inner_gs[row, s_idx])
            row_axes[row].append(ax)
            ax.set_xticks([])
            ax.set_yticks([])
            for spine in ax.spines.values():
                spine.set_linewidth(0.4)
                spine.set_edgecolor("#555555")
    all_axes.append(row_axes)

# ── Load and plot images ───────────────────────────────────────────────────────
for atk_col, (attack_key, attack_label) in enumerate(ATTACKS):
    ssim_map = ssim_lookup.get(attack_key, {})
    for s_plot_idx, sample_num in enumerate(SAMPLE_INDICES):
        sid = f"{sample_num:02d}"
        prefix = f"{BACKBONE}_{DIM}d_{attack_key}"
        orig_path  = os.path.join(RECON_DIR, f"{prefix}_{sid}_orig.png")
        recon_path = os.path.join(RECON_DIR, f"{prefix}_{sid}_recon.png")

        for row, img_path in enumerate([orig_path, recon_path]):
            ax = all_axes[atk_col][row][s_plot_idx]
            if os.path.exists(img_path):
                img = Image.open(img_path).convert("L")   # grayscale
                ax.imshow(np.array(img), cmap="gray", vmin=0, vmax=255,
                          aspect="auto", interpolation="nearest")
            else:
                # Placeholder if image missing
                ax.imshow(np.zeros((64, 64)), cmap="gray", vmin=0, vmax=255)
                ax.text(0.5, 0.5, "N/A", ha="center", va="center",
                        transform=ax.transAxes, fontsize=5, color="red")

        # SSIM label below the reconstructed image
        ssim_val = ssim_map.get(sample_num, None)
        ax_recon = all_axes[atk_col][1][s_plot_idx]
        if ssim_val is not None:
            ax_recon.set_xlabel(
                f"SSIM={ssim_val:.3f}",
                fontsize=FONT_SIZE_SSIM,
                labelpad=2,
            )

# ── Row labels (placed in figure coordinates) ──────────────────────────────────
# Get bounding boxes of first-column axes after drawing
fig.canvas.draw()
renderer = fig.canvas.get_renderer()

for row_idx, label in enumerate(ROW_LABELS):
    ax_ref = all_axes[0][row_idx][0]
    bbox = ax_ref.get_position()
    y_center = (bbox.y0 + bbox.y1) / 2
    fig.text(
        row_label_x, y_center,
        label,
        ha="left", va="center",
        fontsize=FONT_SIZE_ROW,
        rotation=90,
        fontstyle="italic",
    )

# Thin horizontal separator line between orig and recon rows
# (draw a line in figure coords between row 0 and row 1 of first group)
ax0_last = all_axes[0][0][0]
ax1_first = all_axes[0][1][0]
y_sep = (ax0_last.get_position().y0 + ax1_first.get_position().y1) / 2

# ── Caption-ready title ────────────────────────────────────────────────────────
fig.text(
    0.5, 0.02,
    r"Fig. X: Original vs.\ reconstructed face crops under three MIA attacks"
    r" (MobileNetV3-Small, 128-d, $n=3$ representative samples).",
    ha="center", va="bottom",
    fontsize=5.0,
    color="#444444",
    style="italic",
)

# ── Save ───────────────────────────────────────────────────────────────────────
os.makedirs(os.path.dirname(OUT_PDF), exist_ok=True)

fig.savefig(OUT_PDF, dpi=300, bbox_inches="tight", format="pdf")
fig.savefig(OUT_PNG, dpi=200, bbox_inches="tight", format="png")

print(f"[OK] PDF saved: {OUT_PDF}")
print(f"[OK] PNG saved: {OUT_PNG}")

# ── Print SSIM summary ─────────────────────────────────────────────────────────
print("\nSSIM values for plotted samples:")
for attack_key, attack_label in ATTACKS:
    ssim_map = ssim_lookup.get(attack_key, {})
    vals = [ssim_map.get(i, float("nan")) for i in SAMPLE_INDICES]
    print(f"  {attack_label:15s}: " + "  ".join(f"s{i}={v:.4f}" for i, v in zip(SAMPLE_INDICES, vals)))
