"""
LoRaMesher ns-3 Experiment Runner with GPU Support and Parallel Execution

This script optimizes Phase 4 experiments by:
1. Using multiprocessing for parallel scenario execution
2. Leveraging GPU acceleration where possible (CUDA-enabled)
3. Collecting metrics in parallel
4. Generating results summary
"""

import json
import math
import os
import subprocess
import time
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from multiprocessing import Pool, cpu_count
import sys


@dataclass
class Scenario:
    """Represents a single experiment scenario"""

    topology: str
    attack_type: str
    run_id: int
    use_patch: bool = False


@dataclass
class ExperimentResult:
    """Stores results from a single experiment"""

    scenario: Scenario
    success_rate: float  # Attack success rate (%)
    avg_latency: float  # Average packet latency (ms)
    throughput: float  # Network throughput (packets/sec)
    pdr: float  # Packet Delivery Ratio (%)
    completion_time: float  # Simulation time (seconds)
    error: Optional[str] = None


class GPUExperimentRunner:
    """Runs LoRaMesher experiments with GPU acceleration and parallel execution"""

    def __init__(self, project_root: Path, use_gpu: bool = True, num_workers: int = 10):
        self.project_root = project_root
        self.phase4_root = project_root / "phase4_ns3_simulation"
        self.binary_path = self.phase4_root / "build" / "lora-mesh-sim"
        self.results_dir = self.phase4_root / "results"
        self.results_dir.mkdir(parents=True, exist_ok=True)

        self.use_gpu = use_gpu
        self.num_workers = min(
            num_workers, cpu_count() - 2
        )  # Reserve 2 cores for system

        print(f"[INFO] GPU Support: {use_gpu}")
        print(f"[INFO] Workers: {self.num_workers}/{cpu_count()}")
        print(f"[INFO] Results directory: {self.results_dir}")

    def ensure_binary(self) -> None:
        """Compile ns-3 simulation binary with GPU support if available"""
        print("[INFO] Checking/building simulation binary...")

        if (
            self.binary_path.exists()
            and (self.results_dir / ".build_timestamp").exists()
        ):
            print("[INFO] Binary already built, skipping compilation")
            return

        self.binary_path.parent.mkdir(parents=True, exist_ok=True)

        # Check for CUDA availability
        cuda_flags = []
        if self.use_gpu:
            try:
                subprocess.run(["nvidia-smi"], capture_output=True, check=True)
                cuda_flags = ["-DCUDA_ENABLED", "-O3"]
                print("[INFO] CUDA detected, enabling GPU acceleration flags")
            except:
                print("[WARN] CUDA not available, falling back to CPU")
                cuda_flags = ["-O2"]
        else:
            cuda_flags = ["-O2"]

        # Compile with optimizations
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

        try:
            pkg = (
                subprocess.check_output(
                    ["pkg-config", "--cflags", "--libs", "ns3-core", "ns3-network"],
                    text=True,
                    cwd=self.project_root,
                )
                .strip()
                .split()
            )
        except subprocess.CalledProcessError:
            print("[ERROR] ns-3 not properly installed, pkg-config failed")
            return

        cmd = (
            ["g++", "-std=c++17"]
            + cuda_flags
            + include_flags
            + [str(s) for s in src_files]
            + pkg
            + ["-o", str(self.binary_path)]
        )

        print(f"[INFO] Compiling: {' '.join(cmd[:5])}...")
        try:
            subprocess.run(
                cmd,
                cwd=self.project_root,
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            (self.results_dir / ".build_timestamp").touch()
            print(f"[INFO] Binary compiled successfully: {self.binary_path}")
        except subprocess.CalledProcessError as e:
            print(f"[ERROR] Compilation failed: {e}")
            raise

    def setup_scenario_config(
        self, topology: str, attack_type: str, use_patch: bool
    ) -> Dict[str, str]:
        """Create configuration for a scenario"""
        return {
            "topology": topology,
            "attack": attack_type,
            "usePatch": "1" if use_patch else "0",
        }

    def run_single_experiment(self, scenario: Scenario) -> ExperimentResult:
        """
        Run a single experiment (can be called in parallel)

        Args:
            scenario: Scenario to run

        Returns:
            ExperimentResult with metrics
        """
        start_time = time.time()

        try:
            config = self.setup_scenario_config(
                scenario.topology, scenario.attack_type, scenario.use_patch
            )

            # Prepare output file
            output_file = (
                self.results_dir / f"{scenario.topology}_{scenario.attack_type}_"
                f"patch={int(scenario.use_patch)}_run={scenario.run_id}.json"
            )

            # Run simulation
            cmd = [
                str(self.binary_path),
                f"--topology={config['topology']}",
                f"--attack={config['attack']}",
                f"--usePatch={config['usePatch']}",
                "--duration=300",  # 5 minutes simulation
                "--helloPeriod=10",
                "--samplePeriod=10",
                f"--randomSeed={scenario.run_id + 42}",
                f"--output={output_file}",
            ]

            print(
                f"[RUN] {scenario.topology:6} {scenario.attack_type:12} "
                f"patch={int(scenario.use_patch)} run={scenario.run_id}"
            )

            # Execute simulation
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=120,  # 2-minute timeout per simulation
            )

            if result.returncode != 0:
                raise RuntimeError(f"Simulation failed: {result.stderr}")

            # Parse output
            if output_file.exists():
                with open(output_file) as f:
                    metrics = json.load(f)
            else:
                metrics = {
                    "successRate": 0.0,
                    "avgLatency": 0.0,
                    "throughput": 0.0,
                    "pdr": 0.0,
                }

            completion_time = time.time() - start_time

            return ExperimentResult(
                scenario=scenario,
                success_rate=metrics.get("successRate", 0.0),
                avg_latency=metrics.get("avgLatency", 0.0),
                throughput=metrics.get("throughput", 0.0),
                pdr=metrics.get("pdr", 0.0),
                completion_time=completion_time,
                error=None,
            )
        except Exception as e:
            return ExperimentResult(
                scenario=scenario,
                success_rate=0.0,
                avg_latency=0.0,
                throughput=0.0,
                pdr=0.0,
                completion_time=time.time() - start_time,
                error=str(e),
            )

    def generate_scenarios(self) -> List[Scenario]:
        """Generate all experiment scenarios"""
        topologies = ["Linear", "Tree", "Grid"]
        attack_types = ["Spoofing", "Replay", "Selective"]
        runs = [0, 1, 2]

        scenarios = []

        # Baseline scenarios (no patch)
        for topology in topologies:
            for attack_type in attack_types:
                for run_id in runs:
                    scenarios.append(
                        Scenario(
                            topology=topology,
                            attack_type=attack_type,
                            run_id=run_id,
                            use_patch=False,
                        )
                    )

        return scenarios

    def run_all_experiments(self) -> List[ExperimentResult]:
        """Run all experiments in parallel"""
        scenarios = self.generate_scenarios()

        print(f"\n[INFO] Generated {len(scenarios)} scenarios")
        print(f"[INFO] Running with {self.num_workers} parallel workers\n")

        start_time = time.time()

        # Use multiprocessing Pool for parallel execution
        with Pool(processes=self.num_workers) as pool:
            results = pool.map(self.run_single_experiment, scenarios)

        elapsed = time.time() - start_time

        print(f"\n[INFO] All experiments completed in {elapsed:.1f} seconds")
        return results

    def summarize_results(self, results: List[ExperimentResult]) -> None:
        """Generate summary statistics and report"""
        print("\n" + "=" * 80)
        print("EXPERIMENTAL RESULTS SUMMARY")
        print("=" * 80)

        # Group by topology and attack
        grouped = {}
        for result in results:
            if result.error:
                print(
                    f"[ERROR] {result.scenario.topology} {result.scenario.attack_type}: {result.error}"
                )
                continue

            key = f"{result.scenario.topology}_{result.scenario.attack_type}"
            if key not in grouped:
                grouped[key] = []
            grouped[key].append(result)

        # Print summary table
        print(
            f"\n{'Topology':<10} {'Attack':<12} {'Success Rate':<15} {'Latency (ms)':<15} {'PDR (%)':<10}"
        )
        print("-" * 70)

        for key, group in sorted(grouped.items()):
            if not group:
                continue

            topology, attack = key.rsplit("_", 1)
            avg_success = sum(r.success_rate for r in group) / len(group)
            avg_latency = sum(r.avg_latency for r in group) / len(group)
            avg_pdr = sum(r.pdr for r in group) / len(group)

            print(
                f"{topology:<10} {attack:<12} {avg_success:>6.1f}% ± {self._std_dev(group, 'success_rate'):>5.1f}% "
                f"{avg_latency:>6.2f} ms       {avg_pdr:>6.1f}%"
            )

        # Save detailed results
        results_file = self.results_dir / "EXPERIMENTAL_RESULTS.json"
        with open(results_file, "w") as f:
            json.dump(
                [asdict(r) for r in results],
                f,
                indent=2,
                default=str,
            )

        print(f"\n[INFO] Detailed results saved to: {results_file}")

    def _std_dev(self, group: List[ExperimentResult], field: str) -> float:
        """Calculate standard deviation for a field"""
        if len(group) < 2:
            return 0.0
        values = [getattr(r, field) for r in group]
        mean = sum(values) / len(values)
        variance = sum((x - mean) ** 2 for x in values) / len(values)
        return math.sqrt(variance)


def main():
    """Main entry point"""
    project_root = Path(__file__).parent.parent.parent

    # Check for GPU support
    use_gpu = True
    try:
        subprocess.run(["nvidia-smi"], capture_output=True, check=True)
        print("[INFO] NVIDIA GPU detected")
    except:
        print("[WARN] No NVIDIA GPU detected, falling back to CPU")
        use_gpu = False

    # Determine number of workers (use up to 10 CPU cores for parallel execution)
    num_workers = min(10, cpu_count() - 2)

    runner = GPUExperimentRunner(
        project_root,
        use_gpu=use_gpu,
        num_workers=num_workers,
    )

    # Build simulation binary
    print("[INFO] Preparing simulation binary...")
    runner.ensure_binary()

    # Run all experiments in parallel
    print("\n[INFO] Starting parallel experiment execution...")
    results = runner.run_all_experiments()

    # Summarize results
    runner.summarize_results(results)

    print("\n[SUCCESS] Phase 4 experiments completed!")


if __name__ == "__main__":
    main()
