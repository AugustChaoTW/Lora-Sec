import json
import math
import os
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List


@dataclass
class Scenario:
    topology: str
    attack_type: str


class ExperimentRunner:
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.phase4_root = project_root / "phase4_ns3_simulation"
        self.binary_path = self.phase4_root / "build" / "lora-mesh-sim"
        self.results_dir = self.phase4_root / "results"
        self.results_dir.mkdir(parents=True, exist_ok=True)

    def ensure_binary(self) -> None:
        src_files = [
            self.phase4_root / "scratch" / "lora-mesh-sim.cc",
            self.phase4_root / "src" / "routing" / "loramesh-routing.cc",
            self.phase4_root / "src" / "attack" / "attacker-node.cc",
        ]
        include_flags = [
            f"-I{self.phase4_root / 'src'}",
            f"-I{self.phase4_root / 'src' / 'routing'}",
            f"-I{self.phase4_root / 'src' / 'attack'}",
            f"-I{self.phase4_root / 'src' / 'common'}",
        ]
        pkg = (
            subprocess.check_output(
                ["pkg-config", "--cflags", "--libs", "ns3-core", "ns3-network"],
                text=True,
                cwd=self.project_root,
            )
            .strip()
            .split()
        )
        cmd = (
            ["g++", "-std=c++17", "-O2"]
            + include_flags
            + [str(s) for s in src_files]
            + pkg
            + [
                "-o",
                str(self.binary_path),
            ]
        )
        subprocess.run(cmd, cwd=self.project_root, check=True)

    def setup_network(
        self, topology: str, attack_type: str, use_patch: bool
    ) -> Dict[str, str]:
        return {
            "topology": topology,
            "attack": attack_type,
            "usePatch": "1" if use_patch else "0",
        }

    def run_simulation(
        self,
        config: Dict[str, str],
        duration_seconds: int,
        seed: int,
        output_path: Path,
    ) -> None:
        cmd = [
            str(self.binary_path),
            f"--topology={config['topology']}",
            f"--attack={config['attack']}",
            f"--usePatch={config['usePatch']}",
            f"--duration={duration_seconds}",
            "--helloPeriod=10",
            "--samplePeriod=10",
            "--payloadBytes=64",
            f"--seed={seed}",
            f"--output={output_path}",
        ]
        subprocess.run(cmd, cwd=self.project_root, check=True)

    def collect_metrics(self, path: Path) -> Dict:
        with path.open("r", encoding="utf-8") as fp:
            return json.load(fp)


def mean_std(values: List[float]) -> Dict[str, float]:
    if not values:
        return {"mean": 0.0, "std": 0.0}
    mean = sum(values) / len(values)
    var = sum((v - mean) ** 2 for v in values) / len(values)
    return {"mean": mean, "std": math.sqrt(var)}


def generate_markdown(summary: Dict, output_file: Path) -> None:
    lines = []
    lines.append("# EXPERIMENTAL RESULTS BASELINE")
    lines.append("")
    lines.append("## 實驗配置")
    lines.append("")
    lines.append("- 平台：ns-3.41 C++ API（以 Simulator/NodeContainer 驅動）")
    lines.append("- 場景：6 scenarios × 3 runs = 18")
    lines.append("- 指標：攻擊成功率、端到端延遲、吞吐量、PDR")
    lines.append("")
    lines.append("## Baseline 統計（平均 ± 標準差）")
    lines.append("")
    lines.append("| 拓撲 | 攻擊 | 攻擊成功率(%) | 延遲(ms) | 吞吐量(bps) | PDR |")
    lines.append("|---|---|---:|---:|---:|---:|")

    for row in summary["scenario_summary"]:
        lines.append(
            "| {topology} | {attack} | {asr_mean:.2f} ± {asr_std:.2f} | {lat_mean:.2f} ± {lat_std:.2f} | {th_mean:.2f} ± {th_std:.2f} | {pdr_mean:.3f} ± {pdr_std:.3f} |".format(
                topology=row["topology"],
                attack=row["attack"],
                asr_mean=row["attack_success_percent"]["mean"],
                asr_std=row["attack_success_percent"]["std"],
                lat_mean=row["latency_ms"]["mean"],
                lat_std=row["latency_ms"]["std"],
                th_mean=row["throughput_bps"]["mean"],
                th_std=row["throughput_bps"]["std"],
                pdr_mean=row["pdr"]["mean"],
                pdr_std=row["pdr"]["std"],
            )
        )

    lines.append("")
    lines.append("## 攻擊成功率比較")
    lines.append("")
    max_asr = max(
        (r["attack_success_percent"]["mean"] for r in summary["scenario_summary"]),
        default=1.0,
    )
    for row in summary["scenario_summary"]:
        width = (
            int((row["attack_success_percent"]["mean"] / max_asr) * 40)
            if max_asr > 0
            else 0
        )
        bar = "█" * max(0, width)
        lines.append(
            f"- {row['topology']}/{row['attack']}: {bar} {row['attack_success_percent']['mean']:.2f}%"
        )

    lines.append("")
    lines.append("## 關鍵發現")
    lines.append("")
    findings = summary["findings"]
    for item in findings:
        lines.append(f"- {item}")

    output_file.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    project_root = Path(__file__).resolve().parents[2]
    runner = ExperimentRunner(project_root)
    runner.ensure_binary()

    scenarios = [
        Scenario("linear", "spoofing"),
        Scenario("linear", "replay"),
        Scenario("tree", "spoofing"),
        Scenario("tree", "replay"),
        Scenario("grid", "spoofing"),
        Scenario("grid", "selective"),
    ]

    duration_seconds = 300
    seeds = [11, 22, 33]

    per_scenario = []
    all_runs = []

    for scenario in scenarios:
        run_metrics = []
        for idx, seed in enumerate(seeds, start=1):
            config = runner.setup_network(
                scenario.topology, scenario.attack_type, use_patch=False
            )
            output_path = (
                runner.results_dir
                / f"baseline_{scenario.topology}_{scenario.attack_type}_run{idx}.json"
            )
            runner.run_simulation(config, duration_seconds, seed, output_path)
            m = runner.collect_metrics(output_path)
            run_metrics.append(m)
            all_runs.append(m)

        attack_values = []
        for m in run_metrics:
            if scenario.attack_type == "selective":
                attack_values.append(m["attack_success_selective_drop_percent"])
            else:
                attack_values.append(m["attack_success_redirect_percent"])

        row = {
            "topology": scenario.topology,
            "attack": scenario.attack_type,
            "attack_success_percent": mean_std(attack_values),
            "latency_ms": mean_std([m["avg_latency_ms"] for m in run_metrics]),
            "throughput_bps": mean_std([m["throughput_bps"] for m in run_metrics]),
            "pdr": mean_std([m["pdr"] for m in run_metrics]),
            "runs": [
                {
                    "seed": m["seed"],
                    "file": f"baseline_{scenario.topology}_{scenario.attack_type}_run{i + 1}.json",
                }
                for i, m in enumerate(run_metrics)
            ],
        }
        per_scenario.append(row)

    vulnerable = sorted(
        per_scenario, key=lambda x: x["attack_success_percent"]["mean"], reverse=True
    )
    findings = [
        f"最高攻擊成功率場景：{vulnerable[0]['topology']}/{vulnerable[0]['attack']}，平均 {vulnerable[0]['attack_success_percent']['mean']:.2f}%",
        f"最低攻擊成功率場景：{vulnerable[-1]['topology']}/{vulnerable[-1]['attack']}，平均 {vulnerable[-1]['attack_success_percent']['mean']:.2f}%",
        "Baseline 在未認證 DV 路由下可穩定重現路由操縱與流量劣化。",
    ]

    summary = {
        "experiment_count": len(all_runs),
        "scenario_count": len(scenarios),
        "runs_per_scenario": 3,
        "duration_seconds": duration_seconds,
        "scenario_summary": per_scenario,
        "findings": findings,
    }

    summary_path = runner.results_dir / "baseline_summary.json"
    summary_path.write_text(
        json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )

    markdown_path = project_root / "EXPERIMENTAL_RESULTS_BASELINE.md"
    generate_markdown(summary, markdown_path)

    print(f"已完成 {len(all_runs)} 個實驗，摘要輸出：{summary_path}")
    print(f"報告輸出：{markdown_path}")


if __name__ == "__main__":
    main()
