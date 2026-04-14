# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Security research project formally verifying vulnerabilities in the LoRaMesher distance-vector routing protocol. Follows a "Breaking and Repairing" paradigm: discover vulnerabilities → formally prove them with Tamarin Prover → propose minimal patches → validate with ns-3 simulation. Target venue: IEEE Transactions on Dependable and Secure Computing (IEEE TDSC).

## Key Commands

### Tamarin Formal Verification
```bash
# Verify a specific model
tamarin-prover phase2_tamarin_models/patched/patched_lora_dv_2node.spthy --prove

# Use the automated script (recommended)
cd supplementary && ./scripts/verify.sh baseline
cd supplementary && ./scripts/verify.sh patched_2node
cd supplementary && ./scripts/verify.sh patched_metrics_v2
```

### NS-3 Simulation
```bash
# Build the simulator
g++ -std=c++17 -O2 \
  phase4_ns3_simulation/scratch/lora-mesh-sim.cc \
  phase4_ns3_simulation/src/routing/loramesh-routing.cc \
  phase4_ns3_simulation/src/attack/attacker-node.cc \
  -o phase4_ns3_simulation/build/lora-mesh-sim \
  $(pkg-config --cflags --libs ns3-core ns3-network)

# Run a single scenario
./phase4_ns3_simulation/build/lora-mesh-sim \
  --topology=linear --attack=spoofing --usePatch=1 \
  --duration=300 --output=result.json

# Run full 27-scenario experiment matrix (135 runs total, 2-4 hours)
cd phase4_ns3_simulation && python3 run_experiment_matrix.py
```

### Analysis
```bash
# Parse Tamarin verification logs
python3 supplementary/scripts/parse_results.py supplementary/verification/patched_2node_verification.log

# Analyze PDR metrics from ns-3 results
cd phase4_ns3_simulation && python3 analyze_pdr.py results/
```

### Paper
```bash
cd papers/IEEE && make   # Compiles main.tex → main.pdf
```

## Architecture

### Phase 2: Tamarin Formal Models (`phase2_tamarin_models/`)

Three `.spthy` models encode the protocol in the applied pi-calculus with multiset rewriting:

- **`baseline/baseline_lora_dv.spthy`** — Unpatched protocol; 4/5 lemmas falsified, confirming vulnerabilities (Metric Spoofing, Metric Replay, Route Inconsistency, Unauthorized Routes).
- **`patched/patched_lora_dv_2node.spthy`** — **Primary patched model.** Adds HMAC-SHA256 authentication; all 8 lemmas verified in ~4 seconds. Use this for analysis.
- **`patched/patched_lora_dv_with_metrics_v2.spthy`** — Extended model with per-destination MetricVersion counter; 7/8 lemmas verified.

Verification logs and JSON result summaries live in `supplementary/verification/`.

### Phase 4: NS-3 Simulation (`phase4_ns3_simulation/`)

```
src/common/sim-types.h       ← Core enums and structs (TopologyType, AttackType, SecurityMode,
                                HelloMessage, RouteEntry, DataPacket)
src/routing/loramesh-routing.{h,cc}  ← DV routing implementation for baseline/patched/metricversion
src/attack/attacker-node.{h,cc}      ← Dolev-Yao attacker (SPOOFING, REPLAY, SELECTIVE drop)
scratch/lora-mesh-sim.cc             ← Main simulator: topology setup, metrics collection, JSON output
run_experiment_matrix.py             ← Orchestrates 3 topologies × 3 security modes × 3 attack types
```

Security mode is controlled at runtime via `--usePatch` and `--useMetricVersion` flags; all three modes share the same binary.

### Supplementary Artifacts (`supplementary/`)

Publication-ready artifacts: copies of Tamarin models, full verification logs, raw simulation data, Docker image for reproducibility, and a 12KB `README.md` artifact guide for reviewers.

## Security Model

The attacker is a **Dolev-Yao network adversary** who can intercept, replay, and inject LoRa control-plane messages (Hello/RouteUpdate). The two patches are:
1. **HMAC-SHA256** on all control messages (prevents spoofing/injection)
2. **MetricVersion counter** per destination (prevents replay of stale metrics)

Patches are designed to be minimal — no changes to the DV routing algorithm itself.

## Tamarin Lemma Reference

| Lemma | Baseline | Patched (2-node) |
|-------|----------|-----------------|
| L1 MetricAuthenticity | FALSIFIED | ✅ verified |
| L2 RouteFreshness | FALSIFIED | ✅ verified |
| L3 RouteConsistency | FALSIFIED | ✅ verified |
| L4 AuthorizedRoutes | FALSIFIED | ✅ verified |
| L5 Executability | ✅ verified | ✅ verified |
