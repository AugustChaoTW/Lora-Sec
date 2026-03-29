# Phase 4: ns-3 Experimental Validation

**Status**: 🔵 Planning (Phase 2-3 prerequisites not yet complete)  
**Expected Duration**: Week 7-8  
**Purpose**: Reproduce attacks in simulation and measure patch cost

---

## Overview

Phase 4 reproduces Tamarin findings in ns-3 simulator:
1. **Attack Reproduction**: Measure success rates of discovered attacks
2. **Cost Measurement**: Quantify patch overhead (bandwidth, latency, energy)
3. **Validation**: Confirm Tamarin insights with network simulation

## Deliverables

### Attack Reproduction Experiments

**Experiment 1: Hello Spoofing Attack (Metric Injection)**
- Baseline: Unpatched LoRaMesher
- Attack: Attacker node injects false Hello with metric=0
- Metric: Success rate (% of traffic diverted to attacker)
- Expected: ~70% success in 50-node random topology

**Experiment 2: Metric Replay / Downgrade**
- Baseline: Unpatched LoRaMesher
- Attack: Attacker replays old metric at new sequence
- Metric: Selective forwarding rate (% of packets dropped)
- Expected: ~65% success with sustained eavesdrop

**Experiment 3: Patched Protocol Verification**
- Baseline: Patched LoRaMesher (HMAC + MetricVersion)
- Attack: Same as Exp 1 & 2
- Metric: Attack success rate (0% expected)
- Verdict: Patch effectiveness validation

### Cost Measurement

**Metric 1: Bandwidth Overhead**
- Hello message size (baseline vs patched)
- Network traffic per day (50-node mesh)
- Convergence time impact

**Metric 2: Latency Impact**
- HMAC verification delay per node
- Route convergence latency (Bellman-Ford)
- Data forwarding latency

**Metric 3: Energy Consumption**
- CPU usage (HMAC verification)
- Radio transmission energy (+36 bytes)
- Battery life projection

## Timeline

### Immediate Prep (End of Phase 3)
- [ ] Install ns-3 framework
- [ ] Create LoRa PHY layer abstraction
- [ ] Implement LoRaMesher routing module

### Phase 4 Week 1
- [ ] Deploy baseline LoRaMesher in ns-3
- [ ] Run baseline experiments (normal operation)
- [ ] Verify convergence, throughput, latency

### Phase 4 Week 2
- [ ] Implement attack scenarios
- [ ] Run Experiments 1-2 (attack reproduction)
- [ ] Measure attack success rates

### Phase 4 Week 3
- [ ] Deploy patched protocol
- [ ] Run Experiment 3 (patch verification)
- [ ] Run cost measurement experiments

### Phase 4 Week 4
- [ ] Data analysis and visualization
- [ ] Create EXPERIMENTAL_RESULTS.md
- [ ] Prepare Phase 5 inputs

## ns-3 Model Architecture

```
ns-3/
├── src/
│   └── lora-mesh/
│       ├── model/
│       │   ├── loramesh-routing.h/cc
│       │   ├── loramesh-net-device.h/cc
│       │   └── loramesh-packet.h/cc
│       ├── helper/
│       │   └── loramesh-helper.h/cc
│       └── examples/
│           ├── lora-mesh-baseline.cc
│           ├── lora-mesh-attack.cc
│           └── lora-mesh-patched.cc
```

## Expected Results

### Attack Reproduction
| Attack | Baseline Success | Patched Success | Improvement |
|--------|---|---|---|
| Hello Spoofing | 70% | 0% | 100% |
| Metric Replay | 65% | 0% | 100% |
| Combined | 60% | 0% | 100% |

### Cost Analysis
| Metric | Baseline | Patched | Overhead |
|--------|----------|---------|----------|
| Hello size | 12 bytes | 44 bytes | +32 bytes |
| Daily traffic | 360 KB | 2.95 MB | +720% |
| Energy/day | 73 J | 72.6 J | -0.5% |
| Battery life | 1650 days | 1656 days | +0.4% |

---

**Note**: This directory will be populated during Phase 4. Phase 2-3 completion is prerequisite.

