#!/usr/bin/env python3
import argparse
import csv
import json
import os
import subprocess
import time
from dataclasses import asdict, dataclass
from multiprocessing import Pool, cpu_count
from pathlib import Path
from typing import Dict, Iterable, List, Optional


STATE_FLAGS = {
    "baseline": {"use_patch": "0", "use_metric_version": "0"},
    "patched": {"use_patch": "1", "use_metric_version": "0"},
    "metricversion": {"use_patch": "1", "use_metric_version": "1"},
}

ATTACK_SUCCESS_FIELD = {
    "selective": "attack_success_selective_drop_percent",
}


@dataclass(frozen=True)
class Scenario:
    topology: str
    attack: str
    state: str
    seed: int


@dataclass
class ScenarioResult:
    scenario: Scenario
    output_file: str
    packets_generated: int
    packets_delivered: int
    packets_dropped: int
    pdr: float
    pdr_percent: float
    avg_latency_ms: float
    throughput_pkt_per_sec: float
    throughput_bps: float
    attack_success_percent: float
    attack_effect_percent: float
    final_route_entries: int
    error: Optional[str] = None


def build_output_path(results_dir: Path, prefix: str, scenario: Scenario) -> Path:
    return (
        results_dir
        / f"{prefix}_{scenario.topology}_{scenario.attack}_{scenario.state}_seed={scenario.seed}.json"
    )


def attack_success_key(attack: str) -> str:
    return ATTACK_SUCCESS_FIELD.get(attack, "attack_success_redirect_percent")


def run_scenario(job: Dict) -> ScenarioResult:
    scenario = Scenario(**job["scenario"])
    results_dir = Path(job["results_dir"])
    output_path = build_output_path(results_dir, job["prefix"], scenario)
    flags = STATE_FLAGS[scenario.state]

    cmd = [
        job["binary_path"],
        f"--topology={scenario.topology}",
        f"--attack={scenario.attack}",
        f"--state={scenario.state}",
        f"--usePatch={flags['use_patch']}",
        f"--useMetricVersion={flags['use_metric_version']}",
        f"--seed={scenario.seed}",
        f"--duration={job['duration']}",
        f"--output={output_path}",
    ]

    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = "/opt/ns-allinone-3.40/ns-3.40/build/lib:" + env.get(
        "LD_LIBRARY_PATH", ""
    )

    try:
        completed = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=job["timeout"],
            env=env,
            cwd=job["workdir"],
        )
        if completed.returncode != 0:
            raise RuntimeError(
                completed.stderr.strip()
                or completed.stdout.strip()
                or "simulation failed"
            )

        if not output_path.exists():
            raise RuntimeError(f"output not created: {output_path}")

        with output_path.open("r", encoding="utf-8") as fp:
            data = json.load(fp)

        route_samples = data.get("route_table_size_timeseries", [])
        final_route_entries = route_samples[-1]["total_entries"] if route_samples else 0

        return ScenarioResult(
            scenario=scenario,
            output_file=str(output_path),
            packets_generated=data.get("packets_generated", 0),
            packets_delivered=data.get("packets_delivered", 0),
            packets_dropped=data.get("packets_dropped", 0),
            pdr=data.get("pdr", 0.0),
            pdr_percent=data.get("pdr_percent", data.get("pdr", 0.0) * 100.0),
            avg_latency_ms=data.get("avg_latency_ms", 0.0),
            throughput_pkt_per_sec=data.get("throughput_pkt_per_sec", 0.0),
            throughput_bps=data.get("throughput_bps", 0.0),
            attack_success_percent=data.get(attack_success_key(scenario.attack), 0.0),
            attack_effect_percent=0.0,
            final_route_entries=final_route_entries,
        )
    except Exception as exc:
        return ScenarioResult(
            scenario=scenario,
            output_file=str(output_path),
            packets_generated=0,
            packets_delivered=0,
            packets_dropped=0,
            pdr=0.0,
            pdr_percent=0.0,
            avg_latency_ms=0.0,
            throughput_pkt_per_sec=0.0,
            throughput_bps=0.0,
            attack_success_percent=0.0,
            attack_effect_percent=0.0,
            final_route_entries=0,
            error=str(exc),
        )


def build_scenarios(
    topologies: Iterable[str],
    attacks: Iterable[str],
    states: Iterable[str],
    seeds: Iterable[int],
) -> List[Scenario]:
    scenarios = []
    for topology in topologies:
        for attack in attacks:
            for state in states:
                for seed in seeds:
                    scenarios.append(
                        Scenario(
                            topology=topology, attack=attack, state=state, seed=seed
                        )
                    )
    return scenarios


def result_index(results: Iterable[ScenarioResult]) -> Dict[str, ScenarioResult]:
    return {
        f"{result.scenario.topology}:{result.scenario.attack}:{result.scenario.state}:{result.scenario.seed}": result
        for result in results
    }


def attach_attack_effects(results: List[ScenarioResult]) -> None:
    indexed = result_index(results)
    for result in results:
        if result.scenario.attack == "none" or result.error:
            result.attack_effect_percent = 0.0
            continue

        control = pick_result(
            indexed,
            result.scenario.topology,
            "none",
            result.scenario.state,
            result.scenario.seed,
        )
        if control.error or control.pdr <= 0.0:
            result.attack_effect_percent = 0.0
            continue

        result.attack_effect_percent = max(
            0.0, (control.pdr - result.pdr) / control.pdr * 100.0
        )


def pick_result(
    indexed: Dict[str, ScenarioResult],
    topology: str,
    attack: str,
    state: str,
    seed: int,
) -> ScenarioResult:
    return indexed[f"{topology}:{attack}:{state}:{seed}"]


def write_team_c_csv(csv_path: Path, results: List[ScenarioResult]) -> None:
    metrics = (
        ("pdr_percent", lambda r: r.pdr_percent),
        ("latency_ms", lambda r: r.avg_latency_ms),
        ("throughput_pkt_per_sec", lambda r: r.throughput_pkt_per_sec),
        ("attack_effect_percent", lambda r: r.attack_effect_percent),
        ("packets_generated", lambda r: r.packets_generated),
        ("packets_delivered", lambda r: r.packets_delivered),
        ("final_route_entries", lambda r: r.final_route_entries),
    )

    with csv_path.open("w", newline="", encoding="utf-8") as fp:
        writer = csv.writer(fp)
        writer.writerow(["topology", "attack", "patch_version", "metric", "value"])
        for result in results:
            for metric_name, metric_fn in metrics:
                writer.writerow(
                    [
                        result.scenario.topology,
                        result.scenario.attack,
                        result.scenario.state,
                        metric_name,
                        metric_fn(result),
                    ]
                )


def evaluate_quality(
    results: List[ScenarioResult],
    topologies: List[str],
    states: List[str],
    seed: int,
) -> Dict:
    indexed = result_index(results)
    failed = [result for result in results if result.error]
    meaningful = [
        result
        for result in results
        if not result.error and result.packets_generated > 0
    ]
    non_attack = [
        result
        for result in results
        if result.scenario.attack == "none" and not result.error
    ]

    comparisons = []
    spoofing_clear = True
    replay_clear = True

    for topology in topologies:
        baseline_spoof = pick_result(indexed, topology, "spoofing", "baseline", seed)
        patched_spoof = pick_result(indexed, topology, "spoofing", "patched", seed)
        metric_spoof = pick_result(indexed, topology, "spoofing", "metricversion", seed)
        baseline_replay = pick_result(indexed, topology, "replay", "baseline", seed)
        patched_replay = pick_result(indexed, topology, "replay", "patched", seed)
        metric_replay = pick_result(indexed, topology, "replay", "metricversion", seed)

        spoofing_gap = (
            baseline_spoof.attack_effect_percent - patched_spoof.attack_effect_percent
        )
        replay_gap = (
            patched_replay.attack_effect_percent - metric_replay.attack_effect_percent
        )

        spoofing_clear = (
            spoofing_clear
            and spoofing_gap >= 20.0
            and patched_spoof.attack_effect_percent <= 1.0
        )
        replay_clear = (
            replay_clear
            and replay_gap >= 20.0
            and metric_replay.attack_effect_percent <= 1.0
        )

        comparisons.append(
            {
                "topology": topology,
                "spoofing": {
                    "baseline_attack_effect": baseline_spoof.attack_effect_percent,
                    "patched_attack_effect": patched_spoof.attack_effect_percent,
                    "metricversion_attack_effect": metric_spoof.attack_effect_percent,
                    "baseline_pdr": baseline_spoof.pdr_percent,
                    "patched_pdr": patched_spoof.pdr_percent,
                    "metricversion_pdr": metric_spoof.pdr_percent,
                },
                "replay": {
                    "baseline_attack_effect": baseline_replay.attack_effect_percent,
                    "patched_attack_effect": patched_replay.attack_effect_percent,
                    "metricversion_attack_effect": metric_replay.attack_effect_percent,
                    "baseline_pdr": baseline_replay.pdr_percent,
                    "patched_pdr": patched_replay.pdr_percent,
                    "metricversion_pdr": metric_replay.pdr_percent,
                },
            }
        )

    data_quality_ok = (
        len(failed) == 0
        and len(meaningful) == len(results)
        and all(result.pdr_percent >= 0.0 for result in results)
        and all(result.avg_latency_ms >= 0.0 for result in results)
        and all(result.throughput_pkt_per_sec >= 0.0 for result in results)
        and all(result.final_route_entries > 0 for result in results)
        and all(result.packets_generated > 0 for result in results)
        and all(result.packets_delivered > 0 for result in non_attack)
    )

    decision = "360_full"
    decision_reason = "Results are usable, but the attack contrast is not yet strong enough for a 27-only stop."
    if data_quality_ok and spoofing_clear and replay_clear:
        decision = "27_only"
        decision_reason = (
            "The 27-scenario matrix already isolates the two protections cleanly: authentication blocks spoofing, "
            "and MetricVersion blocks replay."
        )
    elif not data_quality_ok:
        decision = "diagnose_or_rerun"
        decision_reason = "Some scenarios failed or produced weak data, so a rerun/diagnosis is required before scaling."

    representative = []
    for topology in topologies:
        representative.append(
            {
                "topology": topology,
                "none_metricversion_pdr_percent": pick_result(
                    indexed, topology, "none", "metricversion", seed
                ).pdr_percent,
                "spoofing_baseline_effect_percent": pick_result(
                    indexed, topology, "spoofing", "baseline", seed
                ).attack_effect_percent,
                "spoofing_patched_effect_percent": pick_result(
                    indexed, topology, "spoofing", "patched", seed
                ).attack_effect_percent,
                "replay_patched_effect_percent": pick_result(
                    indexed, topology, "replay", "patched", seed
                ).attack_effect_percent,
                "replay_metricversion_effect_percent": pick_result(
                    indexed, topology, "replay", "metricversion", seed
                ).attack_effect_percent,
            }
        )

    return {
        "total_scenarios": len(results),
        "successful_scenarios": len(results) - len(failed),
        "failed_scenarios": len(failed),
        "data_quality_ok": data_quality_ok,
        "spoofing_clear": spoofing_clear,
        "replay_clear": replay_clear,
        "decision": decision,
        "decision_reason": decision_reason,
        "comparisons": comparisons,
        "representative_results": representative,
        "failed_details": [
            {
                "topology": result.scenario.topology,
                "attack": result.scenario.attack,
                "state": result.scenario.state,
                "seed": result.scenario.seed,
                "error": result.error,
            }
            for result in failed
        ],
    }


def write_summary_report(
    report_path: Path,
    results: List[ScenarioResult],
    analysis: Dict,
    duration_seconds: int,
    elapsed_seconds: float,
) -> None:
    indexed = result_index(results)
    seed = results[0].scenario.seed if results else 1
    topologies = sorted({result.scenario.topology for result in results})

    lines = [
        "# Apr 2-3 ns-3 Experiment Summary",
        "",
        f"- Duration per scenario: {duration_seconds} seconds",
        f"- Scenario count: {analysis['total_scenarios']}",
        f"- Successful scenarios: {analysis['successful_scenarios']}",
        f"- Wall-clock runtime: {elapsed_seconds:.2f} seconds",
        f"- Decision: **{analysis['decision']}**",
        "",
        "## Data Quality",
        "",
        f"- All outputs generated: {'yes' if analysis['failed_scenarios'] == 0 else 'no'}",
        f"- Metric fields populated: {'yes' if analysis['data_quality_ok'] else 'no'}",
        f"- Spoofing contrast clear: {'yes' if analysis['spoofing_clear'] else 'no'}",
        f"- Replay contrast clear: {'yes' if analysis['replay_clear'] else 'no'}",
        "",
        "## Representative Results",
        "",
    ]

    for topology in topologies:
        none_secure = pick_result(indexed, topology, "none", "metricversion", seed)
        spoof_base = pick_result(indexed, topology, "spoofing", "baseline", seed)
        spoof_patch = pick_result(indexed, topology, "spoofing", "patched", seed)
        replay_patch = pick_result(indexed, topology, "replay", "patched", seed)
        replay_metric = pick_result(indexed, topology, "replay", "metricversion", seed)
        lines.extend(
            [
                f"### {topology.title()}",
                f"- No attack / metricversion: PDR {none_secure.pdr_percent:.1f}%, latency {none_secure.avg_latency_ms:.1f} ms, throughput {none_secure.throughput_pkt_per_sec:.3f} pkt/s",
                f"- Spoofing impact: baseline {spoof_base.attack_effect_percent:.1f}% → patched {spoof_patch.attack_effect_percent:.1f}%",
                f"- Replay impact: patched {replay_patch.attack_effect_percent:.1f}% → metricversion {replay_metric.attack_effect_percent:.1f}%",
                "",
            ]
        )

    lines.extend(
        [
            "## Recommendation",
            "",
            f"{analysis['decision_reason']}",
            "",
            "## Team Handoff",
            "",
            "- **Team A**: Start the Evaluation section using `results/apr2_3_summary.md` and `results/EXPERIMENTAL_RESULTS.json`.",
            "- **Team B**: Prepare the 360-scenario batch only if the decision stays `360_full`; otherwise keep a ready-to-run command only.",
            "- **Team C**: Use `results/apr2_3_team_c_metrics.csv` to generate final Figure 3-4 inputs.",
            "",
        ]
    )

    if analysis["failed_details"]:
        lines.extend(["## Failures", ""])
        for failed in analysis["failed_details"]:
            lines.append(
                f"- {failed['topology']}/{failed['attack']}/{failed['state']}/seed={failed['seed']}: {failed['error']}"
            )
        lines.append("")

    report_path.write_text("\n".join(lines), encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run the Apr 2-3 27-scenario ns-3 matrix"
    )
    parser.add_argument(
        "--topologies",
        nargs="+",
        default=["linear", "tree", "grid"],
    )
    parser.add_argument(
        "--attacks",
        nargs="+",
        default=["none", "spoofing", "replay"],
    )
    parser.add_argument(
        "--states",
        nargs="+",
        default=["baseline", "patched", "metricversion"],
    )
    parser.add_argument("--seeds", nargs="+", type=int, default=[1])
    parser.add_argument("--duration", type=int, default=150)
    parser.add_argument("--workers", type=int, default=min(10, max(1, cpu_count() - 2)))
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--prefix", default="apr2_3")
    parser.add_argument("--results-dir", default="results")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    phase4_root = Path(__file__).resolve().parent
    binary_path = phase4_root / "build" / "lora-mesh-sim"
    results_dir = (phase4_root / args.results_dir).resolve()
    results_dir.mkdir(parents=True, exist_ok=True)

    if not binary_path.exists():
        raise SystemExit(f"Binary not found: {binary_path}")

    scenarios = build_scenarios(args.topologies, args.attacks, args.states, args.seeds)
    jobs = [
        {
            "scenario": asdict(scenario),
            "binary_path": str(binary_path),
            "results_dir": str(results_dir),
            "duration": args.duration,
            "timeout": args.timeout,
            "prefix": args.prefix,
            "workdir": str(phase4_root),
        }
        for scenario in scenarios
    ]

    print(f"[INFO] Running {len(scenarios)} scenarios with {args.workers} workers")
    start = time.time()
    with Pool(processes=min(args.workers, len(scenarios))) as pool:
        results = pool.map(run_scenario, jobs)
    elapsed = time.time() - start

    results.sort(
        key=lambda result: (
            result.scenario.topology,
            result.scenario.attack,
            result.scenario.state,
            result.scenario.seed,
        )
    )

    attach_attack_effects(results)
    analysis = evaluate_quality(results, args.topologies, args.states, args.seeds[0])
    output_payload = {
        "summary": {
            "duration_seconds": args.duration,
            "elapsed_seconds": elapsed,
            **analysis,
        },
        "scenarios": [asdict(result) for result in results],
    }

    json_path = results_dir / "EXPERIMENTAL_RESULTS.json"
    csv_path = results_dir / "apr2_3_team_c_metrics.csv"
    report_path = results_dir / "apr2_3_summary.md"

    json_path.write_text(json.dumps(output_payload, indent=2), encoding="utf-8")
    write_team_c_csv(csv_path, results)
    write_summary_report(report_path, results, analysis, args.duration, elapsed)

    print(f"[INFO] Results JSON: {json_path}")
    print(f"[INFO] Team C CSV:  {csv_path}")
    print(f"[INFO] Summary:     {report_path}")
    print(f"[INFO] Decision:    {analysis['decision']}")

    if analysis["failed_scenarios"] > 0:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
