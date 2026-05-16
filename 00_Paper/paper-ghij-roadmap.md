# Paper G–K Roadmap — GenAI × LoRa Mesh Security
**作者定位：** Aug（PhD Y3，嵌入式 ML × 無線系統）  
**基礎圓桌：** 第 36–65 輪（Hollick / Zhang / Ranganathan / Hester / Hessel / Kirby / LeCun）  
**Last updated:** 2026-05-16

---

> **核心 framing（R37 Hester / R38 LeCun）**  
> 稱呼方式：**AI-assisted protocol engineering**（非 AI-controlled mesh）  
> AI 只提議 heuristics，由 human-designed constraints 決定是否採用。  
> 違反此原則的 paper，Meshtastic maintainer 不會接受、LeCun 不會背書。

---

## 零、六項新增共識（LeCun 第 55 輪，補入所有論文）

| # | 共識 | 對應論文 |
|---|------|---------|
| C1 | AI 必須是 assistive，非 autonomous | 全部 — 措辭強制要求 |
| C2 | Reproducible AI pipeline 是安全必要條件 | G（pipeline）、H（蒸餾）、I（transformer） |
| C3 | Benchmark 必須含真實骯髒低品質硬體 | H（fingerprint）、J（scaling） |
| C4 | 長時間尺度 adaptation 是 LoRa AI 核心研究價值 | J（三尺度）、K（Observatory） |
| C5 | Continuous adversarial evaluation > static benchmark | I（co-evolutionary red-team） |
| C6 | Privacy 與倫理必須內建，不能事後補救 | K（Observatory + 倫理） |

---

## 一、研究敘事（Aug 第 65 輪整合版）

```
LoRa mesh 的攻擊面已被定義 [Yang 2018]
    但 mitigation 必須能演化 [Hessel 2022]
物理層提供裝置身分 [Robyns 2017 / Shen 2021 / Shen 2022]
    但 mesh 多跳破壞指紋傳遞
Selective jamming [Aras 2017] + 跨層攻擊 [Sathaye 2019]
    讓單層防禦失效
工程上 scaling cliff [Bor 2016] + 真實部署鴻溝 [Abrardo 2019]
    證明理論與現實差距
ChirpOTLE [Hessel 2020] 提供可重現測試基礎

         ↓ 所有這些留下一個空白 ↓

mesh-aware · AI-assisted · reproducible · dirty-hardware-operable
安全架構 ← Papers G / H / I / J 要填的洞
```

---

## 二、三層 GenAI 架構（LeCun 第 64 輪）

```
┌─────────────────────────────────────────────────────────┐
│  Layer 1 — Cloud LLM                                    │
│  LLM-generated fuzz seeds · threat model versioning     │
│  → Paper G                                              │
├─────────────────────────────────────────────────────────┤
│  Layer 2 — Gateway (Raspberry Pi / mid-model INT8)      │
│  Quantized transformer · cross-layer anomaly detection  │
│  → Paper I                                              │
├─────────────────────────────────────────────────────────┤
│  Layer 3 — Node (ESP32-S3 / <80 KB model)               │
│  Distilled fingerprint CNN · routing heuristic          │
│  → Paper H                                              │
└─────────────────────────────────────────────────────────┘
                 ↓ system integration
            Paper J — scaling + real deployment
```

---

## 三、論文規劃

### Paper G — AI-Assisted Evolvable Threat Model for LoRa Mesh
**目標期刊：** ACM ToSN 或 IEEE TDSC  
**層：** Cloud LLM  
**Inherits:** B（攻擊分類）、C（跨協議）、F（PCSS 框架）  
**Foundation papers:** [1] Yang 2018、[2] Hessel 2022、[9] ChirpOTLE  

#### 研究假說

| # | 假說 | 驗證方法 |
|---|------|---------|
| G1 | LLM pipeline 能從協議規格自動生成對應 Yang 五大攻擊的 fuzz seeds | seed quality 評估 + ChirpOTLE 執行通過率 |
| G2 | Threat model YAML artifact 能 diff / merge / replay，等價 git 版控 | tool demo + 版本間 delta 分析 |
| G3 | AI-generated seeds 發現攻擊變體數 ≥ 手動分析 2× | 對照實驗：human analyst vs LLM |
| G4 | 框架適用於 LoRaWAN（星型）與 LoRaMesher（mesh）兩種拓樸 | 兩拓樸各跑完整 pipeline |

#### 核心貢獻
1. **ThreatGPT-LoRa pipeline**：spec → LLM → fuzz seeds → ChirpOTLE 執行 → 結果回饋 → 威脅模型更新
2. **版本化 threat model**：YAML 格式，支援 `threat-diff v1 v2`、`threat-replay v2`
3. **ChirpOTLE fork** with mesh routing module（Meshtastic Router 抽象層）
4. **Paper F PCSS mapping**：每個 fuzz seed 標記對應的 PCSS 降級條件 Φ(P,E)

#### 補強（R39 Hessel）：AI Pipeline Supply-Chain 攻擊面
Paper G 本身是新攻擊面：若 LLM-generated heuristic 被污染，整個防禦棧失效。  
必須加入：
- Dataset hashing（每個 training set SHA-256 manifest）
- Model provenance（訓練參數 + commit hash 鎖定）
- Deterministic compilation（reproducible build for embedded targets）
- Signed heuristic manifest（gateway 拒絕未簽名更新）

#### 補強（R40 Zhang）：Reproducibility Metadata Spec
Paper G 每個實驗記錄必須包含：
```yaml
firmware_version: meshtastic-2.5.1-abc1234
antenna_type: "SMA 3dBi rubber duck"
spreading_factor: 9
bandwidth_khz: 125
coding_rate: "4/5"
gateway_hw: "Raspberry Pi 4B 4GB"
terrain: "urban dense, building height 15-25m"
```

#### 補強（R36 Hollick）：標準化路線
Paper G 附錄聲明路線圖：
```
Phase 1: Meshtastic / MeshCore RFC（社群驗證）
Phase 2: Academic reference architecture（本論文）
Phase 3: IETF MANET / LPWAN 工作組（待 Phase 2 成熟）
```

#### 切入點（Hollick）
Yang/Hessel 都在 LoRaWAN 星型。Mesh 多跳放大每個攻擊破壞半徑。這就是 delta。

---

### Paper H — Mesh-Aware RFFI: Physical Fingerprint Attestation Through Multi-Hop LoRa
**目標期刊：** IEEE TIFS 或 IEEE JSAC  
**層：** Node (<80 KB)  
**Inherits:** B（HMAC auth）、E（128-d INT8 embedding pipeline）  
**Foundation papers:** [3] Robyns 2017、[4] Shen 2021、[5] Shen 2022  

#### 研究假說

| # | 假說 | 驗證方法 |
|---|------|---------|
| H1 | 多跳中繼後 fingerprint SSIM 下降 >30%，無法直接用 Shen 2022 識別 | 3-hop 實測 EoRa PI |
| H2 | Fingerprint embedding 可壓縮至 <32 bytes 且維持 >90% 辨識率 | 蒸餾實驗 |
| H3 | 簽名 attestation（HMAC over fingerprint + route）不被偵測為 Sybil | 模擬 Sybil 攻擊對比 |
| H4 | 端到端：ESP32-S3 fingerprint 蒸餾 CNN <80 KB，推論 <500 ms | 硬體量測 |

#### 核心貢獻
1. **Fingerprint attestation protocol**：`[node_id:2 | fingerprint:32 | route_path:N | HMAC:8]` 跟著封包走
2. **蒸餾 CNN**：teacher=Shen 2022 TIFS model → student <80 KB INT8，可在 ESP32-S3 執行
3. **Multi-hop fingerprint chain**：每個中繼節點附加自己的 fingerprint slice，gateway 驗鏈
4. **Mesh-aware 攻擊者模型**：攻擊者控制中繼時 fingerprint chain 如何被污染

#### 補強（R41 Ranganathan）：Attacker Disclosure
Paper H 必須揭露：
- Attacker SDR class（RTL-SDR vs USRP N210）
- Jammer distance（m）
- Synchronization assumption（preamble detected vs symbol-level）
- Spoofing capability（transmit power, antenna gain）

#### 補強（R43 Hester / R42 Kirby）：Dirty Hardware 指標
Paper H 實驗必須含「ugly hardware」場景：
- 淘寶天線 vs calibrated antenna（RSSI delta 量化）
- 焊接品質不良 SMA connector（insertion loss 量測）
- 電源波動（3.0V–3.6V sweep）對 fingerprint stability 影響

#### 切入點（Zhang）
Robyns/Shen 三篇都假設單跳單接收端。多跳破壞指紋。填這個洞 → TIFS 空白。

---

### Paper I — Cross-Layer Attack Detection in LoRa Mesh via Gateway Quantized Transformer
**目標期刊：** IEEE TMC 或 IEEE IoTJ  
**層：** Gateway (Raspberry Pi)  
**Inherits:** B（攻擊分類）、C（跨協議）、G（threat model）、H（RFFI）  
**Foundation papers:** [6] Aras 2017、[7] Sathaye 2019  

#### 研究假說

| # | 假說 | 驗證方法 |
|---|------|---------|
| I1 | 單層偵測（RFFI only 或 routing only）對跨層攻擊 false-negative >40% | 消融實驗 |
| I2 | 多模態融合（RFFI + routing anomaly + duty-cycle）降 false-negative ≥60% | 同上對比 |
| I3 | 量化 transformer INT8 在 Raspberry Pi 推論 <100 ms/packet | 硬體量測 |
| I4 | LLM 生成的紅隊腳本（來自 Paper G）覆蓋率超過手動紅隊 | 攻擊覆蓋率分析 |

#### 核心貢獻
1. **Multi-modal feature vector**：[RFFI_embedding:32 | routing_delta:16 | duty_cycle_ratio:4 | tx_count:4] = 56 bytes/packet
2. **Quantized transformer**：INT8，~2 MB，Raspberry Pi 4 執行（閘道）
3. **Red-team script library**：Paper G pipeline 自動生成，標記對應 [6][7] 的攻擊類型
4. **Online 學習**：正常流量分佈 EWMA 更新，不需停機 retraining

#### 補強（R48 Ranganathan）：Co-Evolutionary Adversary
Paper I 評估框架必須含 adaptive attacker：
- Round 1：部署 detector → attacker 調整攻擊模式
- Round 2：detector 更新 → attacker 再調整
- 至少三輪 co-evolution 記錄（非 static benchmark）
- 這對應 LeCun R55 C5：continuous adversarial evaluation

#### 補強（R49 Kirby）：Canonical Simulation Harness
Paper I 隨論文發佈：
```
/simulation/
  harness.py        # unified entry point
  scenarios/        # YAML attack scenario descriptors
  baselines/        # single-layer detector configs
  requirements.txt  # pinned ns-3 / FLoRa / LoRaSim versions
```
目的：任何人重現結果只需 `python harness.py --scenario selective_jam`。

#### 切入點（Ranganathan）
Aras 2017 示範 selective jamming attacker model 的精確度。Sathaye 2019 示範跨層嚴謹度。兩者結合 → multi-layer threat。沒人在 mesh 上這樣做。

---

### Paper J — Scaling Cliff of AI-Augmented LoRa Mesh Security: Real Deployment Analysis
**目標期刊：** ACM SenSys 或 IEEE INFOCOM  
**層：** System integration  
**Inherits:** G + H + I（完整 AI 安全棧）、B（6× EoRa PI 硬體）  
**Foundation papers:** [8] Bor 2016、[10] Abrardo 2019  

#### 研究假說

| # | 假說 | 驗證方法 |
|---|------|---------|
| J1 | Mesh 拓樸下 scaling cliff 比 Bor 2016（星型）更早出現 | ns-3 sweep + 實測 |
| J2 | AI 安全棧（G+H+I）在 cliff 以下 PDR 損失 <5% | PDR 對照實驗 |
| J3 | AI 安全棧在 cliff 附近使 cliff 向高密度方向移動（因異常包提前過濾） | density vs PDR 曲線 |
| J4 | 90 天戶外部署：電池壽命 >150 天（5 ev/hr），security layer 不改變量級 | E20 energy model + INA219 實測 |

#### 核心貢獻
1. **Mesh scaling curve**：重現 Bor 2016，但拓樸為 mesh（不是星型）—文獻空白
2. **AI security overhead characterization**：PDR / latency / battery 三維量測
3. **90 天 Abrardo-style 戶外部署**：before（純 HMAC）vs after（G+H+I 全棧）
4. **Dirty-hardware robustness**：deliberate node failure / RF interference / firmware glitch

#### 補強（R47 Hessel）：三尺度評估
Paper J 評估框架三層：
```
Short-term  (0–24h):  attack resilience (replay / jam / spoofing)
Medium-term (1–4wk):  topology drift (node failure / RF change / routing table evolution)
Long-term   (1–3mo):  environmental adaptation (seasonal foliage / humidity / rain attenuation)
```
三層必須各有獨立指標，不能只用 aggregate PDR。

#### 補強（R50 Hester）：Real Deployment Trace 強制要求
Paper J 必須包含 ≥30 天真實部署 trace（不只 ns-3 simulation）。  
Trace 必須含 topology event log（node join/leave/RF degradation）。

#### 補強（R45 Hollick / R46 Zhang）：長時間尺度 AI 價值主張
Paper J 加 §7.5：AI heuristic 能否預測「明天早上某條路徑會因霧氣而衰退」？  
這是傳統 heuristic 做不到的，也是 LoRa AI 最有 novelty 的研究方向。

#### 切入點（Hester + Kirby）
Bor 2016 之後幾乎沒人在 mesh 下重做 scaling。Abrardo 2019 真實部署沒有 AI/security。你的 before/after 是最容易被引用的結構。

---

### Paper K — Open LoRa Observatory: Privacy-Preserving Global Mesh Telemetry
**目標期刊：** IEEE TPDS 或 ACM SIGCOMM (community paper track)  
**層：** Infrastructure / Ethics  
**Inherits:** G+H+I+J（全棧）、Paper F（PCSS）  
**Foundation rounds:** R51 LeCun（Observatory idea）、R52 Hollick（privacy-by-design）、R53 Zhang（fingerprint deanon risk）、R54 Ranganathan（dual-use / ethics）  

#### 核心概念（R51 LeCun）
全球 volunteer LoRa mesh nodes → continuous telemetry → 研究社群使用  
類比 ImageNet 對 vision，但真實世界版。

#### 研究假說

| # | 假說 | 驗證方法 |
|---|------|---------|
| K1 | Topology anonymization + DP(ε=1.0) 後，mesh 結構不能被 deanonymize | 攻擊者重建拓樸實驗 |
| K2 | RF fingerprint IQ samples 能 deanonymize device → user identity | 用 Shen 2022 model 攻擊 Observatory dataset |
| K3 | Aggregation threshold（節點數 ≥ 10）+ delayed release（24h）能防止 movement tracking | 時序分析 |
| K4 | 全球節點 RF propagation 模型（季節性）能輔助 Paper J J4 長時間尺度預測 | cross-region telemetry vs Paper J model |

#### 核心貢獻
1. **Observatory 架構**：volunteer node → edge anonymizer → regional aggregator → research API
2. **Privacy spec**：拓樸匿名化 / 24h delayed release / DP noise injection / aggregation threshold
3. **Dataset governance**：IQ sample 禁止公開、fingerprint 只公開 aggregate statistics
4. **Dual-use 倫理 section**（R54 Ranganathan）：
   - RF fingerprinting 可用於追蹤異見者、戰場監控、抗議監視
   - 論文明確聲明使用限制與 IRB 協議
   - dataset license 禁止 surveillance 用途

#### 為什麼是獨立論文（不是 J 的附錄）
Observatory 本身是 infrastructure 貢獻。Privacy-by-design + 倫理框架的設計決策值得獨立審查。 
ComST survey（P05/Paper K-prev 定位）合併：K 同時是 survey anchor + infrastructure paper。

---

## 四、繼承鏈（完整版 B → K）

```
Foundation (#1–29)
    │
    ▼
Paper B ✓ (AdHocNet)
    │  dst=next_hop→silent black hole; HMAC patch
    ├──► Paper C (TIFS) — cross-protocol taxonomy
    │       #53 draft pending
    ├──► Paper D (IoTJ) — selective encryption
    │       #67 E21 open (P3)
    ├──► Paper E (IoTJ/FGCS) — split inference + MIA
    │       #72 E20 INA219 open
    └──► Paper F ✓ (S&P/USENIX) — PCSS theory
             │
             ▼
    ┌────────────────────────────────────────────────┐
    │  GenAI Layer  (Papers G–J)                     │
    │                                                │
    │  Paper G (ToSN/TDSC)                           │
    │  Cloud LLM: evolvable threat model + fuzzing   │
    │  Depends: B, C, F + [1,2,9]                   │
    │      │                                         │
    │      ├──► Paper H (TIFS/JSAC)                 │
    │      │    Node: mesh-aware RFFI attestation    │
    │      │    Depends: B, E + [3,4,5]             │
    │      │                                         │
    │      └──► Paper I (TMC/IoTJ)                  │
    │           Gateway: cross-layer quant-xformer  │
    │           Depends: B, C, G, H + [6,7]         │
    │               │                               │
    │               ▼                               │
    │           Paper J (SenSys/INFOCOM)             │
    │           System: scaling + real deployment    │
    │           Depends: G+H+I + [8,10]             │
    └────────────────────────────────────────────────┘
             │
             ▼
    Paper K (TPDS/SIGCOMM) — Observatory + Privacy + Ethics
    Rolls up B–J + 10 foundation papers + 全球 telemetry infrastructure
    (Originally P05/ComST proposal; scope expanded by R51–54)
```

---

## 五、10 篇必讀文獻 — 2段式閱讀筆記

### [1] Yang et al. 2018 — Security Vulnerabilities in LoRaWAN (IEEE IoTDI)
**是什麼：** 五大攻擊（replay、plaintext recovery、message modification、ACK falsification、battery exhaustion）的首次系統性示範，含實機驗證。成為此後所有 LoRaWAN 安全論文的標準 baseline。  
**限制 / 切入：** 星型拓樸，gateway 是攻擊唯一入口。Mesh 多跳讓每個攻擊有 N 個切入點，破壞半徑 O(hop) 增長。Paper G 把這五個攻擊做成 fuzz seed template，每個對應 PCSS 降級條件。

### [2] Hessel, Almon, Hollick 2022 — LoRaWAN Security: An Evolvable Survey (ACM ToSN)
**是什麼：** 提出「evolvable methodology」——安全分析不是一次性 snapshot，而是必須能版本化 diff replay 的活文件。這是 Paper G 威脅模型版控設計的直接理論基礎。  
**限制 / 切入：** 沒有自動化工具，沒有 AI，手動維護。Paper G 把 evolvable 概念實作為 YAML + LLM pipeline，使更新代價從「人天」降到「分鐘」。

### [3] Robyns et al. 2017 — Physical-layer fingerprinting of LoRa (ACM WiSec)
**是什麼：** LoRa RFFI 開山論文。利用 preamble CFO 與 amplitude distortion 作為指紋，supervised + zero-shot learning。確立問題 framing：IQ sample → embedding → identity。  
**限制 / 切入：** 單接收端、受控環境，識別率隨 SNR 急劇下降。Paper H 的切入：把 fingerprint 從接收端拉到傳送端（node-side 蒸餾），指紋作為 attestation 隨封包走。

### [4] Shen, Zhang et al. 2021 — RFFI for LoRa Using Deep Learning (IEEE JSAC)
**是什麼：** 目前 LoRa RFFI 最高引用基線。處理 CFO drift（跨天穩定識別）；CNN 架構，辨識率 >95%。成為實驗對照的標準 teacher model。  
**限制 / 切入：** 模型 ~5 MB，無法在 ESP32-S3 執行。Paper H 從此 teacher 蒸餾到 <80 KB student；且沒有處理多跳——Paper H 的核心問題。

### [5] Shen, Zhang et al. 2022 — Scalable & Channel-Robust RFFI for LoRa (IEEE TIFS)
**是什麼：** 解決 channel variation 問題（不同位置、不同 SF 仍能識別），引入 contrastive learning。這是最接近 reference implementation 要求的 RFFI 論文。  
**限制 / 切入：** 仍是單跳假設；在 relay 中繼後指紋失真未處理。Paper H 實測這個失真量化之後建 multi-hop attestation chain。

### [6] Aras et al. 2017 — Selective Jamming of LoRaWAN (MobiQuitous)
**是什麼：** 用便宜 SDR 做 selective jamming（只 jam 特定 node_id 的封包），示範 attacker model 的設定方式：明確硬體（RTL-SDR）、明確距離（50 m）、明確功率。  
**限制 / 切入：** 沒有 AI，沒有 mesh，沒有 RFFI。Paper I 把 selective jamming 與 fingerprinting 結合：攻擊者能 jam 時，能否同時偽造 fingerprint？跨層 co-attack 偵測。

### [7] Sathaye, Ranganathan et al. 2019 — Wireless attacks on aircraft landing systems (ACM WiSec)
**是什麼：** 非 LoRa 論文，但方法論精髓：physical-layer attack 如何系統化建模、重現、量化。攻擊者能力用「距離 × 功率 × 時間」精確描述。  
**限制 / 切入：** 場景是 ILS（precision approach），與 LoRa 完全不同。Paper I 借其方法論嚴謹度：每個 cross-layer attack scenario 用同等精度描述 attacker 能力，Paper G 生成對應的 fuzz script。

### [8] Bor et al. 2016 — Do LoRa LPWANs Scale? (ACM MSWiM)
**是什麼：** 用真實實驗與 simulation 雙重驗證 LoRa 密度 scaling cliff。超過節點密度上限，PDR 急跌。任何 AI 提案若忽略此，社群會打回票。  
**限制 / 切入：** 星型 LoRaWAN 拓樸；mesh 下 cliff 特性未知（多跳可能提前或延後 cliff）。Paper J 填此空白，並量測 AI 安全棧對 cliff 的影響。

### [9] Hessel, Almon, Álvarez 2020 — ChirpOTLE (ACM WiSec)
**是什麼：** 目前唯一開源、可重現、實機的 LoRaWAN 攻擊評估框架。Python + SDR + 實際 LoRa gateway。作為 Paper G 的測試平台 fork 基礎。  
**限制 / 切入：** 沒有 AI 元件、沒有 mesh routing module。Paper G fork ChirpOTLE，加 mesh module（Meshtastic Router 最小抽象）+ LLM fuzz driver。這讓論文第一天就站在可重現基礎上。

### [10] Abrardo, Pozzebon 2019 — Multi-Hop LoRa in Siena Aqueducts (Sensors)
**是什麼：** 少數真實部署數月以上的多跳 LoRa 案例，地下環境，完整 PDR/能耗/topology drift 數據。示範 simulation 與 reality 的鴻溝（PDR 比 sim 低 20–30%）。  
**限制 / 切入：** 無 AI、無 security。Paper J 用同等場景做 before/after：純 HMAC 基線 vs G+H+I 全棧，PDR/battery/resilience 三維對比。這種結構最容易被引用。

---

## 六、White Paper Outline（章節級）

**Title:** *AI-Assisted Security for LoRa Mesh: From Evolvable Threat Models to Physically-Aware Multi-Layer Defense*

```
§1  Introduction (2p)
    §1.1  LoRa mesh problem statement (attack surface, scaling constraint)
    §1.2  Why single-layer security fails (cross-layer gap)
    §1.3  Three-layer AI contribution summary (G/H/I/J map)

§2  Background & Related Work (3p)
    §2.1  LoRaWAN threat taxonomy [1][2] — and what mesh changes
    §2.2  Physical-layer fingerprinting [3][4][5] — multi-hop gap
    §2.3  Cross-layer attacks [6][7] — from LoRaWAN to mesh
    §2.4  Scaling reality [8][10] — engineering constraints
    §2.5  Existing tools [9] — reproducibility baseline

§3  System Architecture (2p)
    §3.1  Three-layer AI stack (Fig: node → gateway → cloud)
    §3.2  Packet format evolution (Paper B 142B → Paper H 206B attestation)
    §3.3  PCSS grounding (Paper F) — why physical feasibility is non-negotiable

§4  Layer 1: Cloud — Evolvable Threat Model (Paper G) (3p)
    §4.1  LLM fuzz pipeline design
    §4.2  Versioned threat model YAML schema
    §4.3  Evaluation: seed quality, coverage, ChirpOTLE pass rate

§5  Layer 3: Node — Mesh-Aware RFFI Attestation (Paper H) (3p)
    §5.1  Multi-hop fingerprint degradation measurement
    §5.2  Distillation pipeline: Shen 2022 → <80 KB student
    §5.3  Attestation packet protocol
    §5.4  Evaluation: 3-hop EoRa PI testbed

§6  Layer 2: Gateway — Cross-Layer Detection (Paper I) (3p)
    §6.1  Multi-modal feature fusion design
    §6.2  Quantized transformer architecture (INT8, ~2 MB)
    §6.3  Red-team validation (Paper G seeds)
    §6.4  Evaluation: false-negative rate vs single-layer baseline

§7  System Integration: Scaling & Real Deployment (Paper J) (4p)
    §7.1  Mesh scaling curve (vs Bor 2016 star topology)
    §7.2  AI security overhead at cliff boundary
    §7.3  90-day outdoor deployment design
    §7.4  Before/after: PDR / battery / attack resilience

§8  Discussion (2p)
    §8.1  Dirty-hardware robustness (R43 Hester)
    §8.2  PCSS degradation window analysis (Paper F)
    §8.3  AI autonomy boundary: what AI must NOT decide (R38 LeCun)
    §8.4  Dual-use risks: RF fingerprinting → surveillance (R54 Ranganathan)
    §8.5  Standardization path: Meshtastic RFC → Academic → IETF (R36 Hollick)
    §8.6  Open problems → Paper K Observatory scope

§9  Conclusion (0.5p)
```

---

## 七、3-Month Roadmap Gantt

```
Month 1 (June 2026)
 Wk1  ├── [G] ChirpOTLE fork + mesh module skeleton
 Wk1  ├── [H] Multi-hop fingerprint degradation measurement (3-hop EoRa PI)
 Wk2  ├── [G] LLM fuzz pipeline v0.1 (GPT-4o → Yang 5 attacks → seeds)
 Wk2  ├── [H] Shen 2022 teacher model download + distillation experiment start
 Wk3  ├── [G] Threat model YAML schema + diff tool
 Wk3  ├── [I] Multi-modal feature vector design + data collection start
 Wk4  └── [All] White paper §1–§3 draft (architecture locked)

Month 2 (July 2026)
 Wk5  ├── [G] LLM pipeline evaluation (seed coverage vs manual baseline)
 Wk5  ├── [H] Student CNN distillation complete + ESP32-S3 INT8 deployment
 Wk6  ├── [H] Attestation packet protocol implementation + 3-hop test
 Wk6  ├── [I] Quantized transformer training + Raspberry Pi benchmark
 Wk7  ├── [G] Paper G §4 draft
 Wk7  ├── [H] Paper H §5 draft
 Wk8  └── [I] Paper I §6 draft + red-team validation (Paper G seeds)

Month 3 (August 2026)
 Wk9  ├── [J] ns-3 scaling sweep (mesh topology, density 1–50 nodes)
 Wk9  ├── [J] AI security overhead measurement (PDR / latency)
 Wk10 ├── [J] 90-day outdoor deployment kickoff (n=6 EoRa PI)
 Wk10 ├── [All] White paper §4–§7 complete
 Wk11 ├── [G] Paper G submission-ready draft
 Wk11 ├── [H] Paper H submission-ready draft
 Wk12 └── [All] v0.1 release: ChirpOTLE fork + attestation stack + threat YAML
          └── Meshtastic/MeshCore Discord: progress update post

Milestones
 ─── Jun 30: Architecture locked + v0.1 paper plan + Meshtastic Discord post #1
 ─── Jul 31: G + H experiments complete, drafts started
 ─── Aug 31: G + H submission ready; I draft; J deployment started; v0.1 code release
 ──────────── Monthly Discord update posts (Aug–Nov)
 ─── Nov 30: J 90-day data complete; K architecture design doc
 ─── Dec 31: I + J submission ready; K prototype Observatory (5 volunteer nodes)
 ─── 2027 Q1: Meshtastic RFC draft; IETF LPWAN working group contact
 ─── 2027 Q2: K submission; full Observatory public beta
```

---

## 八、仍需補強（非 blocking）

| 項目 | 說明 | 影響 |
|------|------|------|
| INA219 實測 (Paper E #72) | 目前 80mA 估算 | Paper J J4 假說需精確值 |
| Shen 2022 model weights | 需聯絡張老師確認授權 | Paper H distillation teacher |
| ChirpOTLE maintainer | Hessel 可提供聯絡方式 | Paper G fork 合法性 |
| 90-day 部署場地 | 戶外有遮蔽環境（類 Abrardo aqueduct） | Paper J J3 |
| Paper C #53 draft | 唯一缺 draft 的現有論文 | 不影響 G–J 但應優先關閉 |
