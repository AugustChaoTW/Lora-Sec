# Phase 4: ns-3 LoRaMesher 模擬與實驗驗證

**Date**: 2026-03-29  
**Purpose**: Detailed plan for ns-3 simulation of LoRaMesher attacks and patch effectiveness  
**Status**: 🔄 Planning (awaiting Phase 2-3 completion)

**Expected Duration**: Week 7-8 (2 weeks)

---

## 概述

Phase 2-3 已通過 Tamarin 形式化驗證證明了漏洞與修補。Phase 4 的目標是在**擬真的無線網路環境**中：

1. 重現 Baseline 模型的攻擊
2. 驗證修補的有效性
3. 量化修補成本（延遲、吞吐量、能耗）
4. 為論文提供實驗數據支持

---

## Experiment Design

### 實驗目標

| 目標 | 指標 | 基線預期 | 修補預期 |
|-----|------|--------|--------|
| **攻擊 A 可重現性** | Metric Spoofing 成功率 | 70-80% | 0% |
| **攻擊 B 可重現性** | Metric Replay 成功率 | 65-75% | 0% |
| **網路效能** | 吞吐量 | 100% | 95-98% |
| **延遲影響** | 平均轉發延遲 | T ms | T + 0.5-2 ms |
| **能耗開銷** | 平均功耗 | P mW | P + 0.5-1 mW |

### 網路拓撲設計

我們將測試 3 種拓撲以涵蓋不同場景：

#### 拓撲 1: 線性網路（Linear Chain）

```
[S] ── [R1] ── [R2] ── [R3] ── [R4] ── [D]

參數：
  - 節點數：6 個（1 個源 + 4 個中繼 + 1 個目的地）
  - 距離：每跳 250m（LoRa 典型範圍）
  - 拓撲：嚴格線性（每個節點只有 2 個相鄰）
  - 鏈路質量：完美（無丟包、無衝撞）

攻擊場景：
  - 攻擊者位置：R2（中間），控制 1 個節點
  - 目標：竊聽或黑洞 S-D 的流量
  - 預期結果：
    - Baseline: R1 發送到 R3 而非 R2 的概率 <10%（會選擇更優的虛假 metric）
    - Patched: 100% 正確轉發
```

#### 拓撲 2: 樹狀網路（Tree Topology）

```
          [Source]
            |
        [R1] [R2]
        |     |
    [R3][R4] [R5]
    |   |      |
   [D1][D2]  [D3]

參數：
  - 節點數：8 個（1 個源 + 4 個中繼 + 3 個目的地）
  - 拓撲：深度 3 的樹
  - 相鄰度：平均 2-3 個相鄰節點
  - 鏈路質量：完美

攻擊場景：
  - 攻擊者位置：R2（中間層，多條子樹的親節點）
  - 目標：黑洞子樹 {D2, D3} 的流量
  - 預期結果：
    - Baseline: 30-40% 流量被誤導到攻擊者
    - Patched: 100% 流量到正確目的地
```

#### 拓撲 3: 網格網路（Grid Topology）

```
[N00] ─ [N01] ─ [N02]
  |       |       |
[N10] ─ [N11] ─ [N12]
  |       |       |
[N20] ─ [N21] ─ [N22]

參數：
  - 節點數：9 個（3×3 網格）
  - 相鄰度：4-8 個相鄰節點（取決於位置）
  - 鏈路質量：完美
  - 源/目的地：隨機配對

攻擊場景：
  - 攻擊者位置：N11（中心，控制所有流量）
  - 目標：任意路由操縱
  - 預期結果：
    - Baseline: 多條路由可被攻擊者竊聽
    - Patched: 所有路由驗證正確
```

### 實驗參數設定

#### LoRa 射頻參數

```
Modulation:     LoRa (LoRa modulation, not LORA)
SF (Spreading Factor): 12 (maximum range, ~10-15 km)
Bandwidth:      125 kHz (standard)
Code Rate:      4/8 (CR 2)
Transmit Power: 20 dBm (100 mW, ESP32 typical)
Frequency:      868 MHz (EU ISM band)

Signal Propagation Model: Log-Distance Path Loss
  Path loss = PL_0 + 10*n*log10(d/d_0)
  Where:
    - PL_0 = 127.41 dB @ 1 m
    - n = 2.08 (outdoor)
    - d = distance

Receiver Sensitivity: -137 dBm (LoRa typical)
```

#### 網路協議參數

```
Hello Period:        1 minute (baseline) / 5 minutes (optimized)
Route Table Timeout: 10 minutes (entries become stale)
Maximum Hops:        15 (network diameter bound)
PSK Length:          256 bits (SHA256)
HMAC Algorithm:      SHA256 (patched only)

Data Traffic:
  - Packet size: 64 bytes (application payload)
  - Generation rate: 1 packet/sec per source
  - Duration: 300 seconds (5 minutes, covers ~5 Hello periods)
```

#### 攻擊參數

```
Attack A: Metric Spoofing
  - Attacker broadcasts: Hello(Victim, metric=0, seq=current)
  - Frequency: Every Hello period
  - Success metric: % of honest nodes that update route via attacker

Attack B: Metric Replay
  - Attacker replays: Old Hello(Victim, lower_metric, old_seq)
  - Frequency: Every other Hello period
  - Success metric: % of packets dropped / rerouted

Attack C: Route Manipulation (Combined)
  - Attacker position: Central node
  - Capabilities: Forge + replay + selective forwarding
  - Success metric: Network disconnection rate
```

---

## Simulation Implementation

### ns-3 環境設置

```bash
# Installation
git clone https://gitlab.com/nsnam/ns-3-allinone.git
cd ns-3-allinone
./build.py --enable-examples --enable-tests

# Version: ns-3.40+ recommended
# Python bindings: python3 required
```

### 模組開發

#### Module 1: LoRa PHY Layer (adapted from ns-3-lorawan)

```cpp
// ns-3/src/lora/model/lora-phy.h
class LoraPhy : public NetDevice::Phy {
  // Implements:
  // - Transmission with SF/BW parameters
  // - Reception sensitivity calculation
  // - Collision detection (two simultaneous TX = collision)
  // - Interference modeling (additive noise)
};
```

#### Module 2: LoRaMesher Routing (custom)

```cpp
// ns-3/src/routing/lora-mesh-routing.h
class LoraMeshRouting : public Ipv4RoutingProtocol {
public:
  // Baseline version (no authentication)
  virtual void BroadcastHello();
  virtual void UpdateRouteTable(const HelloMessage& hello);
  virtual int LookupRoute(Ipv4Address dst);
  
  // Patched version
  class PatchedLoraMeshRouting : public LoraMeshRouting {
    virtual void BroadcastHello();        // Now includes HMAC
    virtual bool VerifyHello(const HelloMessage& hello);  // NEW
    virtual void UpdateRouteTable(const HelloMessage& hello);  // With MetricVersion check
  };
};
```

#### Module 3: Attack Simulator (custom)

```cpp
// ns-3/src/attack-simulator/lora-attacker.h
class LoRaAttacker : public Ipv4RoutingProtocol {
public:
  // Attack A: Metric Spoofing
  void AttackMetricSpoofing(Ipv4Address victim);
  
  // Attack B: Metric Replay
  void AttackMetricReplay(Ipv4Address victim);
  
  // Attack C: Selective Forwarding
  void AttackSelectiveForwarding(uint16_t drop_rate_percent);
};
```

### 測試腳本 (Python)

```python
# scratch/lora-mesh-experiment.py

import ns3
from collections import defaultdict

class LoRaMeshExperiment:
    def __init__(self, topology, attack_type, use_patch=False):
        self.topology = topology          # "linear", "tree", "grid"
        self.attack_type = attack_type    # "spoofing", "replay", "selective"
        self.use_patch = use_patch        # True = patched, False = baseline
        self.results = defaultdict(list)
    
    def setup_network(self):
        """Create ns-3 network with specified topology"""
        self.container = ns3.NodeContainer()
        self.container.Create(self.get_node_count())
        
        # Position nodes according to topology
        self.place_nodes()
        
        # Install LoRa devices & routing
        self.install_devices()
        self.install_routing()
    
    def install_routing(self):
        """Install baseline or patched LoRaMesher routing"""
        if self.use_patch:
            # Use patched routing
            routing_helper = ns3.PatchedLoraMeshRoutingHelper()
        else:
            # Use baseline routing
            routing_helper = ns3.LoraMeshRoutingHelper()
        
        routing_helper.Install(self.container)
    
    def deploy_attack(self):
        """Deploy attacker on one node"""
        attacker_node = self.container.Get(self.get_attacker_index())
        
        if self.attack_type == "spoofing":
            attacker = ns3.LoRaAttacker()
            attacker.AttackMetricSpoofing(ns3.Ipv4Address("10.0.0.255"))
            attacker.SetNode(attacker_node)
        
        elif self.attack_type == "replay":
            attacker = ns3.LoRaAttacker()
            attacker.AttackMetricReplay(ns3.Ipv4Address("10.0.0.255"))
            attacker.SetNode(attacker_node)
        
        elif self.attack_type == "selective":
            attacker = ns3.LoRaAttacker()
            attacker.AttackSelectiveForwarding(50)  # Drop 50% of packets
            attacker.SetNode(attacker_node)
    
    def run_simulation(self, duration_seconds=300):
        """Run ns-3 simulation for specified duration"""
        ns3.Simulator.Stop(ns3.Seconds(duration_seconds))
        
        # Install packet tracing
        self.enable_tracing()
        
        # Run simulation
        ns3.Simulator.Run()
        ns3.Simulator.Destroy()
        
        # Collect & analyze results
        self.analyze_results()
    
    def enable_tracing(self):
        """Enable packet-level tracing for analysis"""
        pcap_helper = ns3.PcapHelper()
        for i in range(self.container.GetN()):
            node = self.container.Get(i)
            device = node.GetDevice(0)
            pcap_helper.Captureall(f"lora-mesh-{i}.pcap", device)
    
    def analyze_results(self):
        """Parse PCAP and compute metrics"""
        self.results['delivery_rate'] = self.compute_delivery_rate()
        self.results['latency'] = self.compute_latency()
        self.results['power_consumption'] = self.compute_power()
        self.results['attack_success_rate'] = self.compute_attack_success()

# Run experiments
experiments = [
    ("linear", "spoofing", False),      # Baseline with Attack A
    ("linear", "spoofing", True),       # Patched with Attack A
    ("tree", "replay", False),          # Baseline with Attack B
    ("tree", "replay", True),           # Patched with Attack B
    ("grid", "selective", False),       # Baseline with Attack C
    ("grid", "selective", True),        # Patched with Attack C
]

for topology, attack, patch in experiments:
    exp = LoRaMeshExperiment(topology, attack, patch)
    exp.setup_network()
    exp.deploy_attack()
    exp.run_simulation()
    exp.save_results(f"results_{topology}_{attack}_{patch}.json")
```

---

## Expected Results & Metrics

### 1. 攻擊成功率 (Attack Success Rate)

**定義**: 攻擊者成功將流量誤導到自己的路由比例

**測量方法**:
```
ASR = (packets_rerouted / total_packets) * 100%
```

**預期結果**:

| Topology | Attack | Baseline | Patched |
|----------|--------|----------|---------|
| Linear | Spoofing | 75% | 0% |
| Linear | Replay | 60% | 0% |
| Tree | Spoofing | 65% | 0% |
| Tree | Replay | 70% | 0% |
| Grid | Spoofing | 55% | 0% |
| Grid | Selective | 80% | 0% |

### 2. 網路性能 (Network Performance)

#### Throughput (吞吐量)

```
Definition: Successfully delivered packets / total generated packets
Expected: 
  - Baseline: ~95-100% (natural losses only)
  - Patched: ~95-100% (no degradation)
```

#### Latency (延遲)

```
Definition: Average time from source generation to destination reception
Expected:
  - Baseline: ~50-100 ms (depends on topology depth)
  - Patched: ~50-110 ms (+0.5-2 ms overhead from HMAC)
  - Overhead ratio: <3%
```

#### Jitter (抖動)

```
Definition: Variance in latency
Expected:
  - Baseline: ±10-20 ms
  - Patched: ±10-25 ms (slightly higher due to HMAC verification)
```

### 3. 能耗評估 (Energy Consumption)

**假設 ESP32 LoRa 設備**:
```
Baseline Topology, 300-sec simulation:
  TX (Hello broadcast every 60s): 6 × 50 mJ = 300 mJ
  RX (receive all Hellos): 6 × 10 mJ = 60 mJ
  Total: ~360 mJ

Patched (same traffic + HMAC):
  TX (Hello + HMAC): 6 × 50 mJ = 300 mJ (negligible overhead)
  HMAC computation: 6 × 3 mJ = 18 mJ
  RX verification: 6 × 1 mJ = 6 mJ
  Total: ~324 mJ + overhead

Overhead: ~24 mJ / 360 mJ ≈ 6.7% (acceptable)
```

### 4. 攻擊場景覆蓋矩陣 (Attack Scenarios)

| Scenario | Topology | Attacker Pos | Attack Type | Coverage | Notes |
|----------|----------|--------------|-------------|----------|-------|
| S1 | Linear | Middle (R2) | Spoofing | 75% | Black hole attack |
| S2 | Linear | Middle (R2) | Replay | 60% | Route downgrade |
| S3 | Tree | Subtree root | Spoofing | 65% | Subtree isolation |
| S4 | Tree | Subtree root | Replay | 70% | Metric manipulation |
| S5 | Grid | Center (N11) | Spoofing | 55% | Central control point |
| S6 | Grid | Center (N11) | Selective | 80% | Arbitrary forwarding |

---

## Implementation Timeline

### Week 7: ns-3 環境與基礎模組

- [ ] Day 1-2: Install ns-3 & verify setup
- [ ] Day 3: Implement LoraPhy (physical layer)
- [ ] Day 4-5: Implement LoraMeshRouting (baseline)
- [ ] Day 6-7: Implement PatchedLoraMeshRouting

**Deliverable**: Baseline & patched routing modules compiling successfully

### Week 8: 攻擊與實驗

- [ ] Day 1-2: Implement LoRaAttacker module
- [ ] Day 3-4: Create test scenarios (Linear, Tree, Grid)
- [ ] Day 5-6: Run simulations (6 × 3 scenarios = 18 runs)
- [ ] Day 7: Collect & analyze results

**Deliverable**: Complete simulation results in JSON + graphs

### Week 9: 結果分析與論文集成

- [ ] Tabulate results (Table 6-8 in paper)
- [ ] Create figures (graphs of ASR, latency, power)
- [ ] Write Section 7 (Experimental Evaluation)
- [ ] Cross-reference with Tamarin results (Section 4-5)

**Deliverable**: Complete experimental validation section for paper

---

## Validation Checklist

Before finalizing ns-3 results:

- [ ] **Baseline correctness**
  - [ ] Baseline model exhibits expected attacks
  - [ ] Attack success rates match predictions
  - [ ] No unintended patch behaviors

- [ ] **Patched correctness**
  - [ ] All attacks fail (0% success rate)
  - [ ] No false rejections of legitimate Hellos
  - [ ] Backward compatibility confirmed

- [ ] **Performance**
  - [ ] Latency overhead <5%
  - [ ] Throughput maintained at >95%
  - [ ] Energy overhead <10%

- [ ] **Reproducibility**
  - [ ] Simulation code in GitHub
  - [ ] Seed-based reproducibility (same seed → same results)
  - [ ] Docker setup for replication
  - [ ] Expected output screenshots

---

## Known Limitations & Mitigations

| Limitation | Impact | Mitigation |
|-----------|--------|-----------|
| Perfect channels (no loss) | Overestimates routing stability | Test with 5-10% loss too |
| Fixed topology | No mobility analysis | Out-of-scope for this work |
| Single attacker | Limited attack surface | Multiple attackers in extended work |
| Synchronous protocol | Unrealistic timing | Document as symbolic model assumption |

---

## Expected Publication Figures

Based on simulation, we expect:

1. **Figure 1**: Network topologies (Linear, Tree, Grid)
2. **Figure 2**: Attack success rate comparison (bar chart)
3. **Figure 3**: Latency overhead over time (line plot)
4. **Figure 4**: Throughput under attack (comparison)
5. **Figure 5**: Energy consumption breakdown (pie chart)
6. **Figure 6**: Route convergence time (line plot)
7. **Table 6**: Attack scenario matrix with results
8. **Table 7**: Performance comparison (Baseline vs Patched)

---

## Conclusion

Phase 4 ns-3 simulation will provide **empirical validation** of:
1. Tamarin-discovered attacks are real and reproducible
2. Proposed patches effectively block attacks
3. Patch overhead is acceptable for deployment

This bridges the gap between **formal verification** (Phase 2-3) and **practical deployment** (Phase 4-5), strengthening the paper's impact for IEEE Transactions publication.

---

**Status**: 🔄 Awaiting Phase 2-3 completion  
**Next Step**: Begin ns-3 module implementation once Phase 3 attacks are finalized
