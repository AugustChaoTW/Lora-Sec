"""
Run full 5-backbone × 3-attack MIA grid at dim=128, n_samples=10.
Skips combos already present in mia_results.json.
Usage: python3 run_mia_grid.py
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from mia_attack import run_experiment

RESULTS_PATH = Path(__file__).parent / "results" / "mia_results.json"

BACKBONES = [
    "mobilenet_v3_small",
    "efficientnet_lite0",
    "squeezenet",
    "resnet18",
    "mobilenetv2",
]
ATTACKS = ["fredrikson", "dlg", "inversion_net"]
DIM = 128
N_SAMPLES = 10

def load_existing():
    if RESULTS_PATH.exists():
        with open(RESULTS_PATH) as f:
            return json.load(f)
    return []

def already_done(results, backbone, attack, dim):
    return any(
        r["backbone"] == backbone and r["attack"] == attack and r["dim"] == dim
        for r in results
    )

def main():
    all_results = load_existing()
    total = len(BACKBONES) * len(ATTACKS)
    done = 0

    for bb in BACKBONES:
        for atk in ATTACKS:
            if already_done(all_results, bb, atk, DIM):
                print(f"  SKIP {bb}/{atk} (already in results)")
                done += 1
                continue
            print(f"\n[{done+1}/{total}] {bb} × {atk} dim={DIM}")
            try:
                r = run_experiment(bb, DIM, atk, n_samples=N_SAMPLES)
                all_results.append(r)
                with open(RESULTS_PATH, "w") as f:
                    json.dump(all_results, f, indent=2)
                print(f"  → saved ({RESULTS_PATH})")
            except Exception as e:
                print(f"  ERROR: {e}")
                all_results.append({
                    "backbone": bb, "dim": DIM, "attack": atk,
                    "error": str(e), "privacy_claim_holds": None
                })
                with open(RESULTS_PATH, "w") as f:
                    json.dump(all_results, f, indent=2)
            done += 1

    print("\n" + "="*60)
    print("FINAL SUMMARY (dim=128)")
    print("="*60)
    header = f"{'Backbone':<22} {'Attack':<14} {'SSIM':>6} {'LPIPS':>6} {'Acc':>5} {'Claim'}"
    print(header)
    print("-" * len(header))
    for r in all_results:
        if r.get("dim") != DIM or "error" in r:
            continue
        claim = "✅" if r.get("privacy_claim_holds") else "❌"
        print(f"  {r['backbone']:<20} {r['attack']:<14} "
              f"{r.get('ssim_mean', 0):>6.4f} {r.get('lpips_mean', 0):>6.4f} "
              f"{r.get('accuracy', 0):>5.3f} {claim}")

if __name__ == "__main__":
    main()
