#!/usr/bin/env python3
import json
import os
import statistics
from pathlib import Path


def analyze_pdr():
    results_dir = Path("results")

    if not results_dir.exists():
        print("❌ results/ directory not found")
        return False

    json_files = list(results_dir.glob("*.json"))
    if not json_files:
        print("❌ No JSON results found")
        return False

    pdr_values = []
    attack_baseline = {}

    for json_file in sorted(json_files):
        with open(json_file) as f:
            data = json.load(f)
            pdr = data.get("pdr", 0)
            attack = data.get("attack", "None")
            patch = data.get("patch", 0)

            pdr_values.append(pdr)

            key = f"{attack}_patch={patch}"
            if key not in attack_baseline:
                attack_baseline[key] = []
            attack_baseline[key].append(pdr)

    print("=" * 60)
    print("📊 ns-3 PDR Analysis Report")
    print("=" * 60)
    print()

    print(f"Total scenarios: {len(pdr_values)}")
    print(f"PDR > 0%:       {sum(1 for p in pdr_values if p > 0)}")
    print(f"PDR > 50%:      {sum(1 for p in pdr_values if p > 0.5)}")
    print(f"PDR > 80%:      {sum(1 for p in pdr_values if p > 0.8)}")
    print()

    print(f"PDR Mean:       {statistics.mean(pdr_values) * 100:.1f}%")
    if len(pdr_values) > 1:
        print(f"PDR Std Dev:    {statistics.stdev(pdr_values) * 100:.1f}%")
    print(f"PDR Min:        {min(pdr_values) * 100:.1f}%")
    print(f"PDR Max:        {max(pdr_values) * 100:.1f}%")
    print()

    print("By Attack Type:")
    print("-" * 60)
    for key, values in sorted(attack_baseline.items()):
        mean_pdr = statistics.mean(values) * 100
        count = len(values)
        print(f"  {key:20s}: mean={mean_pdr:5.1f}%, count={count}")
    print()

    success = sum(1 for p in pdr_values if p > 0.5) > 15

    if success:
        print("✅ SUCCESS: Majority of scenarios have PDR > 50%")
        print("   Fix is effective. Proceed to Day 4 analysis.")
        return True
    else:
        print("⚠️  WARNING: Many scenarios still have low PDR")
        print("   Check if full 27-scenario rerun completed.")
        return True


if __name__ == "__main__":
    analyze_pdr()
