#!/usr/bin/env python3
# rho_star_analysis.py — Verify and extend ρ* calculations
# Paper F: "Physically-Constrained Semantic Security Theory for IoT Mesh Routing"
# Issue #87 (F11)
#
# Implements the closed-form ρ*(E) = min(ρ_δ, ρ_B) from:
#   Definition 9  (physical_feasibility.tex, Def. auth-cost + feasibility)
#   Theorem 2     (physical_feasibility.tex, thm:feasibility-bound)
#
# C_auth(E) = (I_hmac * t_hmac + I_tx * t_tx) / 3_600_000   [mAh/msg]
#   where t_hmac, t_tx are in milliseconds (ms), I_* in mA
#
# ρ_δ  = δ_max / (t_tx / 1000)                               [pkt/s]
# ρ_B  = B_mAh / (C_auth * 86400 * L_days)                   [pkt/s]
# ρ*   = min(ρ_δ, ρ_B)

import numpy as np


def rho_star(B_mAh, I_tx_mA, t_tx_ms, delta_max, I_hmac_mA, t_hmac_ms, L_days):
    """
    Compute ρ*(E) and intermediate values.

    Parameters
    ----------
    B_mAh     : float  Battery capacity (mAh)
    I_tx_mA   : float  Transmit current (mA)
    t_tx_ms   : float  On-air time per packet (ms)
    delta_max : float  Maximum duty-cycle fraction (dimensionless, e.g. 0.01)
    I_hmac_mA : float  HMAC computation current (mA)
    t_hmac_ms : float  HMAC computation time per message (ms)
    L_days    : float  Target node lifetime (days)

    Returns
    -------
    rho      : float  ρ*(E)   [pkt/s]
    rho_d    : float  ρ_δ(E)  [pkt/s]
    rho_b    : float  ρ_B(E)  [pkt/s]
    C_auth   : float  C_auth(E) [mAh/msg]
    """
    # Authentication cost per message (mAh/msg)
    # Factor 3_600_000 converts mA·ms → mAh  (1 mAh = 3 600 000 mA·ms)
    C_auth_mAh = (I_hmac_mA * t_hmac_ms + I_tx_mA * t_tx_ms) / 3_600_000

    # Duty-cycle bound [pkt/s]: ρ_δ = δ_max / (t_tx_ms / 1000)
    t_tx_s = t_tx_ms / 1000.0
    rho_delta = delta_max / t_tx_s

    # Battery-lifetime bound [pkt/s]: ρ_B = B_mAh / (C_auth * 86400 * L_days)
    rho_battery = B_mAh / (C_auth_mAh * 86400 * L_days)

    rho = min(rho_delta, rho_battery)
    return rho, rho_delta, rho_battery, C_auth_mAh


# ---------------------------------------------------------------------------
# Device profiles
# Tuple: (B_mAh, I_tx_mA, t_tx_ms, delta_max, I_hmac_mA, t_hmac_ms, L_days)
#
# Notes:
#   t_tx (SF9/BW125, 142-byte frame) ≈ 1692 ms  [measured, EoRa PI / LoRaMesher Hello]
#   t_tx (SF7/BW125, 142-byte frame) ≈  461 ms  [calculated via Semtech LoRa calculator]
#   I_tx EoRa PI (SX1262 @ +14 dBm) ≈ 120 mA   [Ebyte datasheet]
#   I_tx Heltec V3 (SX1262 @ +14 dBm) ≈ 118 mA [Heltec datasheet]
#   I_tx RAK4631 (SX1262 @ +14 dBm) ≈  87 mA   [RAK datasheet, more efficient PA]
#   I_hmac ESP32-S3 HMAC-SHA256 ≈ 80 mA, t_hmac ≈ 0.5 ms  [measured]
#   I_hmac nRF52840 HMAC-SHA256 ≈ 20 mA, t_hmac ≈ 0.2 ms  [RAK4631 MCU, measured]
# ---------------------------------------------------------------------------
devices = {
    "EoRa PI (2×AA, SF9)":       (3000, 120,  1692, 0.01,  80, 0.5, 365),
    "EoRa PI (2×AA, SF7)":       (3000, 120,   461, 0.01,  80, 0.5, 365),
    "Heltec V3 (LiPo 1000mAh)":  (1000, 118,  1692, 0.01,  80, 0.5, 180),
    "RAK4631 (LiPo 3000mAh)":    (3000,  87,  1692, 0.01,  20, 0.2, 365),
}

# Default LoRaMesher Hello interval: 30 s → rate = 1/30 pkt/s
DEFAULT_HELLO_INTERVAL_S = 30.0
default_hello_rate = 1.0 / DEFAULT_HELLO_INTERVAL_S

print("=" * 78)
print("ρ* Verification — Paper F Issue #87 (F11)")
print("=" * 78)
print()

results = {}
for name, params in devices.items():
    rho, rho_d, rho_b, c = rho_star(*params)
    binding = "duty-cycle" if rho_d <= rho_b else "battery"
    interval_s = 1.0 / rho

    if default_hello_rate <= rho:
        sustainable = "YES"
        exceed_factor = None
    else:
        exceed_factor = default_hello_rate / rho
        sustainable = f"NO (exceeds ρ* by {exceed_factor:.1f}×)"

    results[name] = {
        "C_auth": c,
        "rho_delta": rho_d,
        "rho_battery": rho_b,
        "rho_star": rho,
        "interval_s": interval_s,
        "binding": binding,
        "sustainable": sustainable,
        "exceed_factor": exceed_factor,
    }

    print(f"Device: {name}")
    print(f"  C_auth       = {c:.6f} mAh/msg")
    print(f"  ρ_δ          = {rho_d:.4e} pkt/s  (1 pkt per {1/rho_d:.0f} s)")
    print(f"  ρ_B          = {rho_b:.4e} pkt/s  (1 pkt per {1/rho_b:.0f} s)")
    print(f"  ρ*           = {rho:.4e} pkt/s  (1 pkt per {interval_s:.0f} s ≈ {interval_s/60:.1f} min)")
    print(f"  Binding      = {binding}")
    print(f"  Default Hello (30 s = {default_hello_rate:.4e} pkt/s): {sustainable}")
    print()

# ---------------------------------------------------------------------------
# Sensitivity analysis: ρ* vs Hello interval for EoRa PI SF9
# (reference device, battery-bound)
# ---------------------------------------------------------------------------
print("=" * 78)
print("Sensitivity Analysis — EoRa PI SF9: minimum Hello interval for Φ=1")
print("=" * 78)
rho_ref, _, _, _ = rho_star(3000, 120, 1692, 0.01, 80, 0.5, 365)
min_interval_s = 1.0 / rho_ref
print(f"  ρ*(EoRa PI, SF9, 365 days) = {rho_ref:.4e} pkt/s")
print(f"  → Minimum Hello interval   = {min_interval_s:.0f} s  ({min_interval_s/60:.1f} min)")
print(f"  → LoRaMesher default (30 s) requires battery capacity ≥ "
      f"{3000 * (1/30) / rho_ref:.0f} mAh for Φ=1")
print()

print("=" * 78)
print("Design Guideline Table (365-day lifetime, 3000 mAh battery)")
print("=" * 78)
print(f"{'SF':<6} {'t_tx (ms)':<12} {'ρ_δ (pkt/s)':<14} {'ρ_B (pkt/s)':<14} "
      f"{'ρ* (pkt/s)':<14} {'Min interval':<14} {'Binding'}")
print("-" * 92)
sf_configs = [
    ("SF7",  461),
    ("SF8",  800),
    ("SF9", 1692),
    ("SF10", 2793),
    ("SF11", 5077),
    ("SF12", 9928),
]
for sf, t_tx in sf_configs:
    rho, rho_d, rho_b, c = rho_star(3000, 120, t_tx, 0.01, 80, 0.5, 365)
    interval_s = 1.0 / rho
    binding = "duty-cycle" if rho_d <= rho_b else "battery"
    print(f"{sf:<6} {t_tx:<12} {rho_d:<14.4e} {rho_b:<14.4e} "
          f"{rho:<14.4e} {interval_s:<10.0f} s    {binding}")
print()
print("Observation: ρ_B is SF-independent (battery cost dominated by I_tx * t_tx,")
print("  but also scales linearly with t_tx — see C_auth formula).")
print("  As SF increases, t_tx increases → C_auth increases → ρ_B decreases.")
print("  At the same time ρ_δ also decreases, so the binding constraint")
print("  shifts from battery (low SF) to duty-cycle (high SF) for some devices.")
print()
print("Design Guideline:")
print("  For a 365-day, 3000-mAh LoRa node (EoRa PI profile):")
print(f"  → Regardless of SF, minimum Hello interval ≥ "
      f"{1/rho_star(3000, 120, 1692, 0.01, 80, 0.5, 365)[0]:.0f} s (~10 min) at SF9.")
print("  → Use SF7 only if duty-cycle headroom is critical; battery lifetime")
print("    dominates and sets the floor at ~594 s for this profile.")
