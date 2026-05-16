"""
DP epsilon sweep: MobileNetV3-S + InversionNet at eps in {10, 5, 2, 1, 0.5}.
No-DP baseline already in mia_results.json (dp_epsilon=null).
Results appended to results/dp_sweep.json.
Usage: python3 run_dp_sweep.py
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from mia_attack import run_experiment

RESULTS_PATH = Path(__file__).parent / "results" / "dp_sweep.json"
BACKBONE = "mobilenet_v3_small"
ATTACK   = "inversion_net"
DIM      = 128
N        = 10
EPSILONS = [10.0, 5.0, 2.0, 1.0, 0.5]  # no-DP baseline is already in mia_results.json

def load():
    if RESULTS_PATH.exists():
        with open(RESULTS_PATH) as f:
            return json.load(f)
    return []

def done(results, eps):
    return any(r.get("dp_epsilon") == eps for r in results)

def main():
    all_results = load()

    for eps in EPSILONS:
        if done(all_results, eps):
            print(f"  SKIP eps={eps} (already done)")
            continue
        print(f"\n── DP eps={eps} ──────────────────────────")
        try:
            r = run_experiment(BACKBONE, DIM, ATTACK, n_samples=N, dp_epsilon=eps)
            all_results.append(r)
            with open(RESULTS_PATH, "w") as f:
                json.dump(all_results, f, indent=2)
            print(f"  saved → {RESULTS_PATH}")
        except Exception as e:
            print(f"  ERROR: {e}")
            all_results.append({"backbone": BACKBONE, "dim": DIM,
                                 "attack": ATTACK, "dp_epsilon": eps, "error": str(e)})
            with open(RESULTS_PATH, "w") as f:
                json.dump(all_results, f, indent=2)

    print("\n" + "="*55)
    print("DP SWEEP SUMMARY (MobileNetV3-S + InversionNet)")
    print("="*55)
    # no-DP baseline from mia_results.json
    base_path = Path(__file__).parent / "results" / "mia_results.json"
    baseline = {}
    if base_path.exists():
        with open(base_path) as f:
            for r in json.load(f):
                if (r.get("backbone") == BACKBONE and
                    r.get("attack") == ATTACK and
                    r.get("dp_epsilon") is None and
                    "error" not in r):
                    baseline = r
                    break
    if baseline:
        print(f"  eps=inf  sigma=0.000  acc={baseline['accuracy']:.3f}  "
              f"SSIM_mean={baseline['ssim_mean']:.4f}  SSIM_max={baseline['ssim_max']:.4f}  (no DP)")

    import numpy as np
    sigma_of = lambda e: round(2 * (2 * np.log(1.25 / 1e-5)) ** 0.5 / e, 3)
    for r in sorted(all_results, key=lambda x: -(x.get("dp_epsilon") or 0)):
        if "error" in r:
            print(f"  eps={r['dp_epsilon']}  ERROR")
            continue
        eps_v = r.get("dp_epsilon", "?")
        print(f"  eps={eps_v:<4}  sigma={sigma_of(eps_v):>6.3f}  "
              f"acc={r['accuracy']:.3f}  "
              f"SSIM_mean={r['ssim_mean']:.4f}  SSIM_max={r['ssim_max']:.4f}  "
              f"{'✅' if r['ssim_max'] < 0.4 else '❌'}")

if __name__ == "__main__":
    main()
