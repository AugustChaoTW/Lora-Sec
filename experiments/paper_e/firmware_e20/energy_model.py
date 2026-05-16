#!/usr/bin/env python3
"""
E20 Energy Model — PIR Event-Triggered Battery Life Estimator
=============================================================
Paper E: Transmit 128-d embeddings instead of images; PIR-triggered duty
cycling extends battery life compared to an always-on baseline.

Model parameters (edit to match measured values):
  idle_uA      : deep sleep current with EXT0 wake enabled (ESP32-S3 datasheet: 10 µA)
  active_mA    : combined inference + LoRa TX current (~80 mA)
  active_ms    : active window per event = E19 inference (6203 ms) + TX (50 ms)
  battery_mAh  : assumed battery capacity (Li-Ion 18650 or similar)

Usage:
  python3 energy_model.py

Outputs a table to stdout and saves results to energy_model_results.json.
"""

import json
import math
from pathlib import Path

# ─── Parameters ───────────────────────────────────────────────────────────────
IDLE_UA        = 10.0        # µA  — deep sleep + EXT0 GPIO wake
ACTIVE_MA      = 80.0        # mA  — ESP32-S3 active + SX1262 TX
INFERENCE_MS   = 6203        # ms  — E19 measured TFLite latency
TX_MS          = 50          # ms  — LoRa TX 142 bytes @ SF9 BW125
ACTIVE_MS      = INFERENCE_MS + TX_MS   # 6253 ms total active per event
BATTERY_MAH    = 3000.0      # mAh — typical Li-Ion cell

# Event rates to evaluate (events per hour)
EVENT_RATES    = [1, 5, 10, 30, 60]

# ─── Derived constants ────────────────────────────────────────────────────────
IDLE_MA = IDLE_UA / 1000.0   # convert µA → mA


def battery_life(avg_current_ma: float, capacity_mah: float) -> float:
    """Return battery life in days given average current draw and capacity."""
    if avg_current_ma <= 0:
        return math.inf
    hours = capacity_mah / avg_current_ma
    return hours / 24.0


def compute_row(rate_per_hour: float) -> dict:
    """
    For a given PIR trigger rate (events/hour), compute:
      - period_s       : seconds between events
      - active_fraction: duty cycle (fraction of time in active state)
      - avg_current_ma : weighted average current
      - battery_days   : estimated battery life in days
    """
    period_ms        = 3_600_000.0 / rate_per_hour          # ms between events
    active_fraction  = ACTIVE_MS / period_ms                 # dimensionless [0,1]
    idle_fraction    = 1.0 - active_fraction

    avg_ma = active_fraction * ACTIVE_MA + idle_fraction * IDLE_MA

    return {
        "event_rate_per_hr" : rate_per_hour,
        "period_s"          : round(period_ms / 1000.0, 1),
        "duty_cycle_pct"    : round(active_fraction * 100.0, 4),
        "avg_current_mA"    : round(avg_ma, 4),
        "battery_life_days" : round(battery_life(avg_ma, BATTERY_MAH), 1),
    }


def always_on_row() -> dict:
    """Baseline: MCU + radio always active at ACTIVE_MA."""
    avg_ma = ACTIVE_MA
    return {
        "event_rate_per_hr" : "always-on",
        "period_s"          : 0,
        "duty_cycle_pct"    : 100.0,
        "avg_current_mA"    : round(avg_ma, 4),
        "battery_life_days" : round(battery_life(avg_ma, BATTERY_MAH), 1),
    }


def main():
    print("=" * 72)
    print("E20 Battery Life Model — PIR Event-Triggered Duty Cycling")
    print("=" * 72)
    print(f"  Idle current     : {IDLE_UA:.1f} µA  (deep sleep, EXT0 wake)")
    print(f"  Active current   : {ACTIVE_MA:.0f} mA  (inference + LoRa TX)")
    print(f"  Active duration  : {ACTIVE_MS} ms/event  "
          f"({INFERENCE_MS} ms inference + {TX_MS} ms TX)")
    print(f"  Battery capacity : {BATTERY_MAH:.0f} mAh")
    print()

    rows = [compute_row(r) for r in EVENT_RATES]
    baseline = always_on_row()

    # ─── Pretty table ──────────────────────────────────────────────────────────
    hdr = f"{'Rate(ev/hr)':<14} {'Period(s)':<12} {'DutyCycle(%)':<15} " \
          f"{'AvgCurrent(mA)':<18} {'BattLife(days)':<14}"
    print(hdr)
    print("-" * len(hdr))

    for row in rows:
        print(f"{row['event_rate_per_hr']:<14.0f} "
              f"{row['period_s']:<12.1f} "
              f"{row['duty_cycle_pct']:<15.4f} "
              f"{row['avg_current_mA']:<18.4f} "
              f"{row['battery_life_days']:<14.1f}")

    # Separator before baseline
    print("-" * len(hdr))
    print(f"{'always-on':<14} "
          f"{baseline['period_s']:<12} "
          f"{baseline['duty_cycle_pct']:<15.1f} "
          f"{baseline['avg_current_mA']:<18.4f} "
          f"{baseline['battery_life_days']:<14.1f}")
    print()

    # ─── Savings vs always-on ─────────────────────────────────────────────────
    print("Improvement over always-on baseline:")
    for row in rows:
        factor = row["battery_life_days"] / baseline["battery_life_days"]
        print(f"  @ {row['event_rate_per_hr']:>2.0f} ev/hr: "
              f"{row['battery_life_days']:.1f} days  "
              f"({factor:.1f}x longer)")
    print()

    # ─── Save JSON ────────────────────────────────────────────────────────────
    results = {
        "parameters": {
            "idle_uA"        : IDLE_UA,
            "active_mA"      : ACTIVE_MA,
            "active_ms"      : ACTIVE_MS,
            "inference_ms"   : INFERENCE_MS,
            "tx_ms"          : TX_MS,
            "battery_mAh"    : BATTERY_MAH,
        },
        "event_rates": rows,
        "always_on_baseline": baseline,
    }

    out_path = Path(__file__).parent / "energy_model_results.json"
    with open(out_path, "w") as f:
        json.dump(results, f, indent=2)
    print(f"Results saved to: {out_path}")
    print("=" * 72)


if __name__ == "__main__":
    main()
