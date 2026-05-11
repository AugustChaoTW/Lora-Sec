#!/usr/bin/env python3
"""
Scale Experiment: 25-node linear chain + 49-node grid
5 seeds × 2 topologies × 3 attacks × 3 security modes = 90 runs
Duration per run: 400s (includes auto-computed convergence wait)
"""
import json
import os
import subprocess
import time
from dataclasses import asdict, dataclass
from multiprocessing import Pool, cpu_count
from pathlib import Path
from typing import Dict, List, Optional
import statistics

TOPOLOGIES = ["linear25", "grid49"]
ATTACKS    = ["none", "spoofing", "replay"]
STATES     = ["baseline", "patched", "metricversion"]
SEEDS      = [1, 2, 3, 4, 5]
DURATION   = 400   # seconds per run (auto dataStart handles convergence)

STATE_FLAGS = {
    "baseline":     {"usePatch": "0", "useMetricVersion": "0"},
    "patched":      {"usePatch": "1", "useMetricVersion": "0"},
    "metricversion":{"usePatch": "1", "useMetricVersion": "1"},
}


@dataclass(frozen=True)
class Scenario:
    topology: str
    attack: str
    state: str
    seed: int


@dataclass
class Result:
    scenario: Scenario
    pdr_percent: float
    avg_latency_ms: float
    packets_generated: int
    packets_delivered: int
    attack_redirect_pct: float
    error: Optional[str] = None


def run_one(job: Dict) -> Result:
    s = Scenario(**job["scenario"])
    flags = STATE_FLAGS[s.state]
    out = job["results_dir"] / f"scale_{s.topology}_{s.attack}_{s.state}_seed{s.seed}.json"

    cmd = [
        job["binary"],
        f"--topology={s.topology}",
        f"--attack={s.attack}",
        f"--state={s.state}",
        f"--usePatch={flags['usePatch']}",
        f"--useMetricVersion={flags['useMetricVersion']}",
        f"--seed={s.seed}",
        f"--duration={job['duration']}",
        f"--output={out}",
    ]

    try:
        r = subprocess.run(cmd, capture_output=True, text=True,
                           timeout=job["duration"] + 60, cwd=job["workdir"])
        if r.returncode != 0:
            raise RuntimeError(r.stderr.strip() or r.stdout.strip() or "failed")
        if not out.exists():
            raise RuntimeError("no output file")
        data = json.loads(out.read_text())
        return Result(
            scenario=s,
            pdr_percent=data.get("pdr_percent", 0.0),
            avg_latency_ms=data.get("avg_latency_ms", 0.0),
            packets_generated=data.get("packets_generated", 0),
            packets_delivered=data.get("packets_delivered", 0),
            attack_redirect_pct=data.get("attack_success_redirect_percent", 0.0),
        )
    except Exception as e:
        return Result(scenario=s, pdr_percent=0, avg_latency_ms=0,
                      packets_generated=0, packets_delivered=0,
                      attack_redirect_pct=0, error=str(e))


def pdr_stats(results: List[Result]) -> str:
    vals = [r.pdr_percent for r in results if not r.error]
    if not vals:
        return "N/A"
    mean = statistics.mean(vals)
    if len(vals) > 1:
        std = statistics.stdev(vals)
        return f"{mean:.1f}±{std:.1f}%"
    return f"{mean:.1f}%"


def latency_stats(results: List[Result]) -> str:
    vals = [r.avg_latency_ms for r in results if not r.error]
    if not vals:
        return "N/A"
    mean = statistics.mean(vals)
    if len(vals) > 1:
        std = statistics.stdev(vals)
        return f"{mean:.0f}±{std:.0f}ms"
    return f"{mean:.0f}ms"


def main():
    phase4 = Path(__file__).resolve().parent
    binary = phase4 / "build" / "lora-mesh-sim"
    results_dir = phase4 / "results" / "scale"
    results_dir.mkdir(parents=True, exist_ok=True)

    if not binary.exists():
        raise SystemExit(f"Binary not found: {binary}")

    scenarios = [
        Scenario(topology=t, attack=a, state=s, seed=seed)
        for t in TOPOLOGIES
        for a in ATTACKS
        for s in STATES
        for seed in SEEDS
    ]

    jobs = [{
        "scenario": asdict(sc),
        "binary": str(binary),
        "results_dir": results_dir,
        "duration": DURATION,
        "workdir": str(phase4),
    } for sc in scenarios]

    workers = min(max(1, cpu_count() - 2), len(jobs))
    print(f"[scale] {len(jobs)} runs, {workers} workers, {DURATION}s each")
    t0 = time.time()
    with Pool(processes=workers) as pool:
        results = pool.map(run_one, jobs)
    elapsed = time.time() - t0

    errors = [r for r in results if r.error]
    if errors:
        print(f"[WARN] {len(errors)} failed scenarios:")
        for r in errors:
            print(f"  {r.scenario}: {r.error}")

    # Summary table
    print(f"\n[scale] Done in {elapsed:.0f}s  ({len(results)-len(errors)}/{len(results)} ok)\n")
    print(f"{'Topology':10s} {'Attack':8s} {'State':15s} {'PDR (5 seeds)':18s} {'Latency':14s}")
    print("-" * 70)
    for topo in TOPOLOGIES:
        for attack in ATTACKS:
            for state in STATES:
                group = [r for r in results
                         if r.scenario.topology == topo
                         and r.scenario.attack == attack
                         and r.scenario.state == state]
                print(f"{topo:10s} {attack:8s} {state:15s} {pdr_stats(group):18s} {latency_stats(group)}")
        print()

    # Save summary JSON
    summary = {
        "total_runs": len(results),
        "successful": len(results) - len(errors),
        "elapsed_sec": round(elapsed, 1),
        "topologies": TOPOLOGIES,
        "seeds": SEEDS,
        "duration_sec": DURATION,
        "results": [{
            "topology": r.scenario.topology,
            "attack": r.scenario.attack,
            "state": r.scenario.state,
            "seed": r.scenario.seed,
            "pdr_percent": r.pdr_percent,
            "avg_latency_ms": r.avg_latency_ms,
            "packets_generated": r.packets_generated,
            "packets_delivered": r.packets_delivered,
            "error": r.error,
        } for r in results]
    }
    out_json = results_dir / "scale_summary.json"
    out_json.write_text(json.dumps(summary, indent=2))
    print(f"[scale] Results → {out_json}")


if __name__ == "__main__":
    main()
