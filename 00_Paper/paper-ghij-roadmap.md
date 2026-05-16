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

> **戰略原則：Dataset 驅動，非論文驅動**  
> 論文引用壽命 3–7 年；Dataset 引用壽命 10–20 年（cf. ImageNet 2009，至今每天被引）。  
> **LoRaWild Dataset** 是整個 G–K 計畫的引力中心：  
> B–F 從 LoRaWild 獲得 in-the-wild validation → 可視性提升；  
> G–J 的實驗數據來自 LoRaWild → 每篇都有真實世界背書；  
> K 是 LoRaWild 的 dataset paper，越早上線越好。  
> **K-infra（5 節點上線 + Discord 公告）現在就啟動，不等論文寫完。**

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

         ↓ 所有這些留下兩個空白 ↓

空白 1: mesh-aware · AI-assisted · reproducible · dirty-hardware-operable
        安全架構 ← Papers G / H / I / J 要填的洞

空白 2: 沒有任何長期、多地點、安全標注、AI-ready 的
        LoRa mesh 真實數據集
        ← Paper K (LoRaWild) 要填的洞
        ← 這個洞同時讓 B–F 從「論文結論」升格為「in-the-wild evidence」
```

---

## 二、三層 GenAI 架構 + LoRaWild 基礎設施（LeCun 第 64 輪）

```
┌─────────────────────────────────────────────────────────────┐
│  LoRaWild Dataset  (K-infra，現在啟動)                       │
│  volunteer nodes → telemetry → 匿名化 → research API        │
│  ← G/H/I/J 的訓練與評估數據來源                              │
│  ← B–F 的 in-the-wild validation 平台                       │
│  → Paper K (dataset paper)                                  │
└──────────────────────┬──────────────────────────────────────┘
                       │ 數據流
                       ▼
┌─────────────────────────────────────────────────────────────┐
│  Layer 1 — Cloud LLM                                        │
│  LLM-generated fuzz seeds · threat model versioning         │
│  訓練數據：LoRaWild security event log + G1-G4 實驗              │
│  → Paper G                                                  │
├─────────────────────────────────────────────────────────────┤
│  Layer 2 — Gateway (Raspberry Pi / mid-model INT8)          │
│  Quantized transformer · cross-layer anomaly detection      │
│  訓練數據：LoRaWild 多模態 trace (RF + routing + duty-cycle)     │
│  → Paper I                                                  │
├─────────────────────────────────────────────────────────────┤
│  Layer 3 — Node (ESP32-S3 / <80 KB model)                   │
│  Distilled fingerprint CNN · routing heuristic              │
│  訓練數據：LoRaWild RF fingerprint + multi-hop degradation trace │
│  → Paper H                                                  │
└─────────────────────────────────────────────────────────────┘
                       ↓ system integration
               Paper J — scaling + real deployment
               (90-day trace 回饋 LoRaWild → K v1.1)
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

### Paper K — LoRaWild: Long-Term Dataset & Observatory for AI-Assisted LoRa Mesh Security
**目標期刊：** NeurIPS Datasets & Benchmarks Track 或 ACM SIGCOMM (community paper) 或 IEEE TPDS  
**定位：** Dataset paper + Infrastructure paper（類比 ImageNet CVPR 2009）  
**Inherits:** B–J 全部（use cases）、Paper F（PCSS → 隱私框架）  
**Foundation rounds:** R51 LeCun、R52 Hollick、R53 Zhang、R54 Ranganathan  
**K-infra 啟動：** 現在（不等論文，5 節點 pilot + Discord 公告）  
**K-paper 啟動：** 有 6 個月 trace 後才開始寫

#### 為什麼是 Dataset paper 而非 Observatory 論文

```
Dataset paper 的貢獻 = 數據本身（長期被引）
                     + 收集方法（可重現）
                     + 隱私框架（可信任）
                     + Use cases（證明有用）

vs. "Observatory 論文" = 只描述系統，沒有數據 → 審稿人問「數據在哪？」→ reject

策略：先建 K-infra 收數據，有數據才寫 K-paper。
```

#### LoRaWild — 現有數據集對比（為何需要 LoRaWild）

| 現有資料集 | 問題 |
|-----------|------|
| Abrardo 2019（Siena） | 單地點、數月、無 security 標注、無 AI schema |
| LoRaSim / FLoRa traces | 純 simulation，無真實 RF chaos |
| ChirpOTLE captures | LoRaWAN 星型、短期攻擊測試、非 mesh |
| Meshtastic community logs | 分散、無統一 schema、無隱私保護 |
| **LoRaWild（目標）** | **多地點 + 長期季節性 + security 標注 + AI-ready schema** |

#### LoRaWild Dataset Schema（每筆記錄）

```yaml
# ── 封包層（per-packet）──────────────────────────────────────
packet:
  node_id: "sha256(real_id || daily_key)[:8]"   # rotating pseudonym
  timestamp_unix: 1748000000                      # 1s resolution
  rssi_dbm: -87
  snr_db: 3.5
  spreading_factor: 9
  bandwidth_khz: 125
  coding_rate: "4/5"
  hop_count: 2
  next_hop_id: "sha256(...)[:8]"                 # anonymized
  payload_bytes: 142
  tx_success: true
  duty_cycle_pct: 0.87
  battery_mv: 3720

# ── 節點層（5分鐘 aggregate）─────────────────────────────────
node_aggregate:
  node_id: "..."
  topology_snapshot_hash: "sha256(adjacency_matrix)"
  routing_table_version: 47
  active_ms_per_5min: 312
  event_count: 3                                 # PIR / motion triggers
  anomaly_score: 0.12                            # Paper I detector output

# ── RF 環境層（gateway，15分鐘）──────────────────────────────
rf_environment:
  gateway_id: "..."
  noise_floor_dbm: -115
  interference_events: 2
  channel_utilization_pct: 1.3
  terrain_tag: "urban_dense"                     # R40 Zhang mandatory
  antenna_type: "SMA 3dBi rubber duck"           # R40 dirty-hardware tag
  firmware_version: "meshtastic-2.5.1-abc1234"

# ── Security event log ────────────────────────────────────────
security_event:
  timestamp: 1748000000
  suspected_attack_type: "replay"               # from Paper G threat model YAML
  confidence: 0.87
  response: "alert_only"
  node_affected: "sha256(...)[:8]"
  attacker_capability: "RTL-SDR 50m"            # R41 Ranganathan disclosure
```

#### Dataset 發佈層級（隱私框架，R52 Hollick）

```
Tier 0  Public aggregate stats
        (channel utilization / PDR by region / attack frequency)
        → anyone, no registration, CC-BY

Tier 1  Anonymized topology + RF trace
        (routing events, RSSI, SNR；無 IQ samples)
        → registered researcher + citation agreement

Tier 2  Full anonymized packet trace + security event log
        (hop-by-hop, G threat labels, I anomaly scores)
        → IRB-approved + signed data use agreement (禁 surveillance)

Tier 3  Raw IQ samples
        → 永不公開
          (R53 Zhang: IQ → Shen 2022 model → device identity → user tracking)
```

**DP 保護：** routing statistics 加 Gaussian noise ε=1.0  
**Delayed release：** 30-day embargo（防 live topology tracking）  
**Location：** grid-quantized 500m（防 movement inference）  
**Pseudonym rotation：** 每日換 key（防跨天追蹤）

#### PCSS 連接（Paper F）

Observatory 隱私保護本身是 Φ(P,E) 的一個 running instance：
- 若某節點 DP 預算超支（duty cycle 過高）→ `privacy_claim_holds = False` → 停止上傳
- 這讓 Paper F 的抽象理論在 LoRaWild 中有具體 enforcement 機制

#### 研究假說

| # | 假說 | 驗證方法 |
|---|------|---------|
| K1 | Topology anonymization + DP(ε=1.0) 後，攻擊者無法重建 mesh 結構 | 攻擊者重建拓樸實驗（graph isomorphism attack） |
| K2 | Tier 3 IQ samples（若洩漏）能 deanonymize device → user identity | 用 Shen 2022 model 驗證風險存在 → 支持永不公開決策 |
| K3 | Aggregation threshold（≥10 nodes）+ 30-day delay 能防止 movement tracking | 時序重識別攻擊實驗 |
| K4 | LoRaWild 季節性 RF 數據能訓練 Paper J §7.5 的 long-term RF prediction model | LSTM/Transformer cross-region 訓練 + 驗證 |

#### Use Cases — B–F 的 In-the-Wild Validation（Paper K §7）

```
Paper B → UC1: dst=next_hop silent black hole in the wild
  B：ns-3 simulation 有此漏洞
  K：LoRaWild 觀測到 X% 真實節點觸發 silent drop event
  效果：B 從「理論漏洞」升格為「in-the-wild confirmed」

Paper C → UC2: 跨協議攻擊頻率分佈
  C：LoRaMesher vs Meshtastic 攻擊不對稱（lab 實驗）
  K：LoRaWild 含兩種協議節點，security event 分佈在真實數據中驗證 C 的分類學
  效果：C 的分類學有 empirical evidence

Paper E → UC3: 128-d embedding privacy 跨 dataset 泛化
  E：CIFAR-10 lab dataset，SSIM < 0.4
  K：LoRaWild 真實 telemetry 中，embedding 的 MIA 成功率分佈
  效果：E 的 privacy claim 在真實部署中成立

Paper F → UC4: PCSS Φ(P,E) 實時計算
  F：PCSS 抽象理論（ρ* 閉合式）
  K：每個 LoRaWild 節點實時計算 Φ(P,E)，記錄超出 ρ* 的降級事件
  效果：F 有 running system 實例，審稿人能看到理論在野外運作

Paper J → UC5: Seasonal RF prediction
  J §7.5：AI 能否預測「明天某路徑因霧氣衰退」？
  K：提供跨季節 RF trace 作為訓練數據
  效果：J 的長時間尺度 claim 有 12 個月真實數據支撐
```

#### K-infra 立即行動清單（不需寫論文）

```
Week 1 (現在)
  □ 5 節點 EoRa PI pilot 上線（現有硬體）
  □ 定義 LoRaWild schema v0.1（基於本文件）
  □ Meshtastic Discord 發公告：「LoRaWild — Open LoRa Mesh Dataset beta」
  □ GitHub repo: open-lora-observatory（含 schema + node operator kit）

Week 2–4
  □ node operator kit v0.1：firmware + data uploader + privacy guide
  □ 找 Meshtastic / MeshCore 社群 volunteer（目標 20 nodes, 3 cities）
  □ LoRaWild Tier 0 公開：https://lorawild.io（aggregate stats only）

Month 2–5
  □ 擴展至 50+ 節點（4 個地區）
  □ 每月 Discord progress update
  □ Paper G/H/I 論文標注「data from LoRaWild」

Month 6
  □ 有 6 個月 trace → K-paper 開始寫
  □ LoRaWild v1.0 Tier 1 開放（arxiv 同步）
```

#### 核心貢獻（Paper K 論文）
1. **LoRaWild 數據集**：多地點 × 長期 × 四層 schema × 安全標注（主角）
2. **Observatory 架構**：volunteer node → edge anonymizer → regional aggregator → research API
3. **Privacy framework**：DP(ε=1.0) + rotating pseudonym + 30-day delay + Tier 0–3 governance
4. **Five use cases**：B/C/E/F/J 在 LoRaWild 上的 in-the-wild validation
5. **Dual-use ethics**（R54 Ranganathan）：IRB 協議 + dataset license 禁 surveillance

---

## 四、繼承鏈（完整版 B → K）

```
Foundation (#1–29)
    │
    ▼
Paper B ✓ (AdHocNet)                ◄── LoRaWild UC1: B attack in-the-wild
    │  dst=next_hop→silent black hole; HMAC patch
    ├──► Paper C (TIFS)              ◄── LoRaWild UC2: cross-protocol event distribution
    │       cross-protocol taxonomy; #53 draft pending
    ├──► Paper D (IoTJ)
    │       selective encryption; #67 E21 open (P3)
    ├──► Paper E (IoTJ/FGCS)        ◄── LoRaWild UC3: embedding privacy in-the-wild
    │       split inference + MIA; #72 E20 INA219 open
    └──► Paper F ✓ (S&P/USENIX)    ◄── LoRaWild UC4: PCSS Φ(P,E) live enforcement
             PCSS theory
             │
    ┌────────────────────────────────────────────────────────────────┐
    │                                                                │
    │  K-infra ════════════════════════════════════════════════════  │
    │  (現在啟動，平行於 G–J)                                         │
    │  5 nodes → 20 nodes → 50+ nodes                               │
    │  Tier 0 public → Tier 1 researcher → Tier 2 IRB               │
    │                    │                                           │
    │                    ▼ 提供訓練與評估數據                         │
    │  ┌──────────────────────────────────────────────────────────┐ │
    │  │  GenAI Layer  (Papers G–J)                               │ │
    │  │                                                          │ │
    │  │  Paper G (ToSN/TDSC)          ← LoRaWild security event log │ │
    │  │  Cloud LLM: evolvable threat model + fuzzing             │ │
    │  │  Depends: B, C, F + [1,2,9]                             │ │
    │  │      │                                                   │ │
    │  │      ├──► Paper H (TIFS/JSAC) ← LoRaWild RF fingerprint     │ │
    │  │      │    Node: mesh-aware RFFI attestation              │ │
    │  │      │    Depends: B, E + [3,4,5]                       │ │
    │  │      │                                                   │ │
    │  │      └──► Paper I (TMC/IoTJ)  ← LoRaWild multi-modal trace  │ │
    │  │           Gateway: cross-layer quant-xformer             │ │
    │  │           Depends: B, C, G, H + [6,7]                   │ │
    │  │               │                                          │ │
    │  │               ▼                                          │ │
    │  │           Paper J (SenSys/INFOCOM) ← LoRaWild UC5 seasonal  │ │
    │  │           System: scaling + real deployment              │ │
    │  │           Depends: G+H+I + [8,10]                       │ │
    │  │           90-day trace → 回饋 LoRaWild v1.1                  │ │
    │  └──────────────────────────────────────────────────────────┘ │
    │                    │                                           │
    └────────────────────┼───────────────────────────────────────────┘
                         ▼
    Paper K (NeurIPS D&B / SIGCOMM / TPDS)
    Dataset paper: LoRaWild + Observatory + Privacy + Ethics
    主角是數據，論文描述如何收集、保護、使用
    Use cases UC1–UC5 讓 B–F 獲得 in-the-wild visibility
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
    §1.3  Two missing pieces: AI security stack (G–J) + long-term dataset (K)
    §1.4  Contribution map (G/H/I/J/K + LoRaWild)

§2  Background & Related Work (3p)
    §2.1  LoRaWAN threat taxonomy [1][2] — what mesh changes
    §2.2  Physical-layer fingerprinting [3][4][5] — multi-hop gap
    §2.3  Cross-layer attacks [6][7] — from LoRaWAN to mesh
    §2.4  Scaling reality [8][10] — engineering constraints
    §2.5  Existing tools & datasets [9] — reproducibility gap + dataset gap

§3  System Architecture (2p)
    §3.1  LoRaWild as the data foundation (Fig: K-infra flow)
    §3.2  Three-layer AI stack (Fig: node → gateway → cloud)
    §3.3  Packet format evolution (B 142B → H 206B attestation)
    §3.4  PCSS grounding (Paper F): Φ(P,E) as privacy enforcement in LoRaWild

§4  Layer 1: Cloud — Evolvable Threat Model (Paper G) (3p)
    §4.1  LLM fuzz pipeline design + supply-chain security (R39)
    §4.2  Versioned threat model YAML schema
    §4.3  Evaluation: seed coverage vs manual, ChirpOTLE pass rate
    §4.4  LoRaWild data use: G threat labels annotate LoRaWild security_event

§5  Layer 3: Node — Mesh-Aware RFFI Attestation (Paper H) (3p)
    §5.1  Multi-hop fingerprint degradation measurement (LoRaWild RF trace)
    §5.2  Distillation pipeline: Shen 2022 → <80 KB student
    §5.3  Attestation packet protocol
    §5.4  Dirty-hardware evaluation (R43): cheap antenna / power-unstable

§6  Layer 2: Gateway — Cross-Layer Detection (Paper I) (3p)
    §6.1  Multi-modal feature fusion (LoRaWild multi-layer trace as training set)
    §6.2  Quantized transformer architecture (INT8, ~2 MB)
    §6.3  Co-evolutionary red-team validation (R48, 3 rounds)
    §6.4  Evaluation: false-negative rate vs single-layer baseline

§7  System Integration: Scaling & Real Deployment (Paper J) (4p)
    §7.1  Mesh scaling curve (vs Bor 2016 star topology)
    §7.2  AI security overhead at cliff boundary
    §7.3  90-day outdoor deployment (trace → LoRaWild v1.1)
    §7.4  Three-timescale evaluation: short / medium / long (R47)
    §7.5  Long-timescale RF prediction: can AI anticipate tomorrow's path degradation?

§8  LoRaWild Dataset & Observatory (Paper K summary) (2p)
    §8.1  Why long-term dataset matters: research / industry value
    §8.2  Schema: 4-layer (packet / node-agg / RF-env / security-event)
    §8.3  Privacy framework: DP(ε=1.0) + pseudonym + 30-day delay + Tier 0–3
    §8.4  Use cases UC1–UC5: how LoRaWild validates B–F in the wild
    §8.5  Governance: operator agreement / IRB / license (no surveillance)

§9  Discussion (1.5p)
    §9.1  AI autonomy boundary: what AI must NOT decide (R38 LeCun)
    §9.2  Dual-use risks: RF fingerprint → surveillance (R54 Ranganathan)
    §9.3  Standardization path: Meshtastic RFC → Academic → IETF (R36 Hollick)
    §9.4  PCSS degradation window: when does LoRaWild stop publishing?

§10 Conclusion (0.5p)
```

---

## 七、Roadmap Gantt（K-infra 現在啟動）

```
────────────────────────────────────────────────────────────────────────
 TRACK    May  Jun  Jul  Aug  Sep  Oct  Nov  Dec  Jan  Feb  Mar  Apr
          2026 2026 2026 2026 2026 2026 2026 2026 2027 2027 2027 2027
────────────────────────────────────────────────────────────────────────
 K-infra  ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████
  5 nodes  ^
  schema  v0.1
  Discord  ^
  20 nodes      ^
  Tier 0        ^─────────────────────────────────────────────────────
  50+ nodes          ^
  Tier 1             ^─────────────────────────────────────────────────
────────────────────────────────────────────────────────────────────────
 Paper G        ████ ████ ████
  ChirpOTLE     ^
  LLM pipe      ─────^
  YAML tool          ^
  draft              ────^
  submit                  ^
────────────────────────────────────────────────────────────────────────
 Paper H        ████ ████ ████
  3-hop test    ^
  distill CNN   ─────^
  attestation        ^
  draft              ────^
  submit                  ^
────────────────────────────────────────────────────────────────────────
 Paper I             ████ ████ ████
  feature vec        ^
  quant-xformer      ─────^
  co-evol test            ^
  draft                   ────^
  submit                       ^
────────────────────────────────────────────────────────────────────────
 Paper J                  ████ ████ ████ ████ ████
  ns-3 scaling            ^
  deployment              ^─────────────────^  (90 days)
  seasonal RF                               ────^
  draft                                         ────^
  submit                                              ^
────────────────────────────────────────────────────────────────────────
 Paper K                                      ████ ████ ████ ████
  (6mo trace needed before writing)
  K-paper draft                               ^
  privacy analysis                            ─────^
  UC1–UC5 in paper                                 ────^
  submit                                                ─────^
────────────────────────────────────────────────────────────────────────
 Standards  ────────────────────────────────────────────────^────^
  Meshtastic RFC                                            ^
  IETF contact                                                  ^
────────────────────────────────────────────────────────────────────────

KEY MILESTONES
 2026-05  K-infra launch: 5 nodes + LoRaWild schema v0.1 + Discord post #1
 2026-06  Architecture locked; LoRaWild Tier 0 public
 2026-07  LoRaWild 20+ nodes (3 cities); G+H experiments done
 2026-08  G+H submission-ready; v0.1 code release (ChirpOTLE+attestation+YAML)
 2026-09  LoRaWild 50+ nodes; Tier 1 open; Paper I experiments done
 2026-11  J 90-day deployment starts
 2026-12  I+J submission ready; LoRaWild v1.0 with 6-month trace
 2027-01  K-paper writing starts (6-month trace in hand)
 2027-02  Meshtastic RFC draft
 2027-03  K-paper submission
 2027-04  IETF LPWAN working group contact; LoRaWild public beta
```

---

## 八、仍需補強

### 立即行動（blocking K-infra 啟動）

| 項目 | 說明 | 期限 |
|------|------|------|
| LoRaWild 命名確認 | "LoRaWild" 不夠易記，需要類 ImageNet/COCO 的名字 | 本週 |
| LoRaWild schema v0.1 | 基於本文件 YAML spec，建 GitHub repo | 本週 |
| 5 節點 pilot | 現有 EoRa PI 硬體，flash node operator firmware | 本週 |
| Discord 公告 | Meshtastic + MeshCore 社群，宣布 Observatory beta | 本週 |

### 短期（非 blocking，1–3 個月）

| 項目 | 說明 | 影響 |
|------|------|------|
| LoRaWild 命名 domain | lorawild.io 或 [name].io 域名註冊 | K-infra 品牌 |
| INA219 實測 (Paper E #72) | 目前 80mA 估算 | Paper J J4 精確值 |
| Shen 2022 model weights | 聯絡張老師確認授權 | Paper H distillation teacher |
| ChirpOTLE maintainer | Hessel 提供聯絡 | Paper G fork 合法性 |
| IRB 申請 | LoRaWild Tier 2 開放前必須通過 | K-paper 合規性 |

### 長期（6–12 個月）

| 項目 | 說明 | 影響 |
|------|------|------|
| 90-day 部署場地 | 戶外有遮蔽環境（類 Abrardo aqueduct） | Paper J J3 + LoRaWild 季節性數據 |
| Paper C #53 draft | 唯一缺 draft 的現有論文 | 不影響 G–K 但應優先關閉 |
| Industry partner | Semtech / RAK Wireless / TTN 接洽 | LoRaWild 節點擴展 + K paper impact |
| Meshtastic RFC draft | 2027 Q1 | 標準化路線 Phase 1 |
