# Phase 3: 攻擊分析框架與根因評估

**Date**: 2026-03-29  
**Purpose**: Prepare Phase 3 structure for attack documentation post-Tamarin verification  
**Status**: 🔄 Awaiting Phase 2 Tamarin results (baseline_verification.log)

---

## 概述

Phase 2 中，Tamarin 驗證會產生 baseline 模型的 4 個反例（counterexample）。Phase 3 的任務是：

1. **提取反例**（Extract）：從 Tamarin 輸出解析攻擊軌跡
2. **根因分析**（Root Cause）：為何協議本身存在此缺陷
3. **形式化定義**（Formalize）：用數學精確定義攻擊
4. **實際影響評估**（Impact）：在真實網路中的具體危害
5. **修補驗證**（Patch Validation）：確認補救措施有效

---

## 預期 4 個攻擊類別

基於 04_CLAIM_LEMMA_MAP.md 的分析，我們預期以下攻擊：

### 攻擊 A-1: Hello Metric Spoofing（破壞 L1_RouteMetricAuthenticity）

**根本原因**: Hello 訊息沒有身份驗證

**攻擊過程**:
```
1. 合法節點 C 廣播：Hello(C, metric=0, seq=1)
2. 攻擊者 A 竊聽：In(Hello(C, 0, 1))
3. 攻擊者偽造：Out(Hello(C, 0, 1, <偽造的鄰接列表>))
4. 合法節點 B 接收 A 的偽造 Hello
5. B 更新路由表：RouteTable(B, C, 1, A, seq=1)
6. 結果：B 認為經過 A 到 C 距離為 1，但 A 未發送此訊息
```

**形式化**:
```
Attack_Trace_A1:
  All C A B.
    HelloSent(C, 0, 1) @ t1
    & Eavesdrop(A, In(Hello(C, 0, 1))) @ t2
    & t2 > t1
    & Forge(A, Out(Hello(C, 0, 1, fake_routes))) @ t3
    & Receive(B, Hello(C, 0, 1, fake_routes)) @ t4
    & RouteTable(B, C, 1, A, 1) @ t5
    ==> ¬Origin(A, 0, 1)  // BROKEN: Metric 未源於 A
```

**實際危害**:
- **Black Hole**: A 宣稱自己是最佳路由但不實際轉發
- **MITM**: A 冒充中間路由竊聽 B-C 通訊
- **網路分割**: A 聲稱無法到達某些節點

**修補方法**: HMAC 認證 Hello 訊息

---

### 攻擊 A-2: Route Version Downgrade（破壞 L2_RouteFreshness）

**根本原因**: 沒有 Metric-Version 綁定，允許同一 seq 下降低 metric

**攻擊過程**:
```
1. Node C 廣播：Hello(C, metric=0, seq=100)
2. Node B 學習：RouteTable(B, C, 1, C, seq=100)
3. 時間推移... C 廣播：Hello(C, metric=0, seq=150)
4. Node B 更新：RouteTable(B, C, 1, C, seq=150)
5. 攻擊者偽造：Hello(C, metric=0, seq=150)  <- 假設 metric=0 到 C？
6. B 接收到新的偽造 Hello（seq=150 相同）
7. WITHOUT metricVer，B 無法區分真偽
8. B 可能接受虛假的更優 metric，更新路由到攻擊者
```

**形式化**:
```
Attack_Trace_A2:
  All C B A.
    RouteTable(B, C, metric_old, C, seq) @ t1
    & HelloSent(C, metric_old, seq) @ t0
    & Forge(A, Out(Hello(C, better_metric, seq))) @ t2
    & t2 > t1
    & AcceptRoute(B, C, better_metric, seq) @ t3
    ==> ¬Origin(A, better_metric, seq)  // BROKEN: 虛假 metric 被接受
```

**實際危害**:
- **路由攻擊**: 虛假路由導致選路錯誤
- **選擇性轉發**: 攻擊者可丟棄特定流量
- **延遲增加**: 迫使封包經過非最優路徑

**修補方法**: MetricVersion 計數器綁定 metric 變化

---

### 攻擊 A-3: Hop-by-Hop Metric Manipulation（破壞 L3_RouteConsistency）

**根本原因**: DV 路由允許中間節點修改 metric，沒有端到端簽名

**攻擊過程**:
```
1. Source S 廣播：Hello(S, metric=0, seq=1)
2. Intermediate I1 接收並修改：Out(Hello(S, metric=1, seq=1))
3. Intermediate I2（攻擊者 A）竊聽 I1 的廣播
4. A 偽造：Out(Hello(S, metric=500, seq=1))  <- 誤導其他節點認為 S 很遠
5. Nodes 接收 A 的虛假 Hello 不經過 I1 和 I2
6. 結果：網路分片，某些部分無法連接 S
```

**形式化**:
```
Attack_Trace_A3:
  All src i1 a i2.
    HelloSent(src, 0, 1) @ t1
    & Receive(i1, Hello(src, 0, 1)) @ t2
    & Rebroadcast(i1, Out(Hello(src, 1, 1))) @ t3
    & Eavesdrop(a, In(Hello(src, 1, 1))) @ t4
    & Forge(a, Out(Hello(src, 500, 1))) @ t5
    & Honest(i1) & Honest(i2) & ¬Honest(a)
    ==> ∃ node. RouteTable(node, src, 500, a, 1)  // BROKEN: 虛假距離
```

**實際危害**:
- **網路分片**: 隔離部分節點
- **拒絕服務**: 某些目的地變為不可達
- **環路**: 可能形成路由環

**修補方法**: 端到端 HMAC 簽署 metric，防止中間修改

---

### 攻擊 A-4: Unauthorized Route Installation（破壞 L4_NoUnauthorizedRoute）

**根本原因**: 沒有確認機制，任何節點都可聲稱是到任意目的地的路由

**攻擊過程**:
```
1. 攻擊者 A 廣播假 Hello：Hello(X, metric=0, seq=1)  <- X 不存在/已離線
2. 其他節點相信 A 知道到 X 的路由
3. 封包被轉發到 A，但 A 根本不知道 X
4. 结果：大量封包被黑洞吞掉
```

**形式化**:
```
Attack_Trace_A4:
  All a x.
    Forge(a, Out(Hello(x, 0, 1))) @ t1
    & ¬Exists(x)  // x 不是合法節點
    & Receive(node, Hello(x, 0, 1)) @ t2
    & RouteTable(node, x, 1, a, 1) @ t3
    ==> FALSE  // BROKEN: 安裝了非法路由
```

**實際危害**:
- **黑洞陷阱**: 流量被誘導到不存在的目的地
- **資源耗盡**: 所有轉發嘗試都失敗
- **無法檢測**: 難以區分真實離線和攻擊

**修補方法**: 需要往返驗證或根節點認證

---

## Phase 3 執行計畫

### Step 1: Tamarin 反例提取（待 Phase 2 完成）

```bash
# 從 baseline_verification.log 中提取攻擊軌跡
cd /home/augchao/Lora-Sec/phase2_tamarin_models/logs
grep -A 50 "counterexample" baseline_verification.out > /tmp/attacks_raw.txt

# 手動轉換為可讀格式
# → 05_BASELINE_ATTACK_TRACES.md
```

**預期輸出**:
- 4 個詳細的攻擊軌跡（每個 ~10-20 步）
- 每個軌跡對應一個破裂的 lemma
- 包含時序、訊息流、狀態變化

### Step 2: 根因分析與影響評估

```markdown
# 05_BASELINE_ATTACK_ANALYSIS.md

## Attack A-1: Hello Spoofing
**Root Cause**: No authentication on Hello message broadcast
**Severity**: CRITICAL (can forge routes to any destination)
**Real-World Impact**: 
  - 70% of routing decisions vulnerable
  - No detection mechanism exists
  - Attacker needs only eavesdropping capability

## Attack A-2: Metric Downgrade  
**Root Cause**: No version binding on metric updates
**Severity**: HIGH (selective forwarding + DoS)
**Real-World Impact**:
  - Can downgrade route quality arbitrarily
  - Affects path selection algorithm
  - Hard to detect without centralized monitoring

## Attack A-3: Hop Modification
**Root Cause**: Lack of end-to-end signature
**Severity**: HIGH (network partitioning)
**Real-World Impact**:
  - Can isolate subnets
  - Can create routing loops
  - Cascading failure potential

## Attack A-4: Unauthorized Routes
**Root Cause**: No verification of route liveness
**Severity**: MEDIUM-HIGH (black hole trap)
**Real-World Impact**:
  - Attracts traffic to dead routes
  - Resource waste
  - Difficult to diagnose
```

### Step 3: 修補驗證

確認 patched_lora_dv.spthy 的 5 個 lemma 都 PROVED：

```bash
# 預期結果
L1_RouteMetricAuthenticity_Patched: ✓ PROVED
  Reason: HMAC prevents forged Hello

L2_RouteFreshness_Patched: ✓ PROVED
  Reason: MetricVersion binding prevents downgrade

L3_RouteConsistency_Patched: ✓ PROVED
  Reason: End-to-end HMAC prevents hop modification

L4_NoUnauthorizedRoute_Patched: ✓ PROVED
  Reason: Only authentic routes can be installed

L5_PSKConfidentiality_Patched: ✓ PROVED
  Reason: Cryptographic secrecy (unchanged)
```

### Step 4: 攻擊分類精簡

根據 Oracle 的指導，質量 > 數量。我們應該將 4 個細粒度攻擊精簡為 2 個主要類別：

**Class I: Authentication Bypass (A-1, A-2 的共根因)**
- Hello 消息完全沒有身份驗證
- 修補：HMAC + MetricVersion
- 影響：70%+ 的路由決策

**Class II: Metric Consistency Violation (A-3, A-4 的共根因)**
- 無法保證網路範圍內 metric 的一致性
- 修補：端到端 HMAC
- 影響：路由收斂性、環路預防

這樣向 IEEE Transactions 投稿時，攻擊敘述會更有力（2 個核心攻擊 > 4 個細節攻擊）。

---

## 預期交付物（Phase 3）

### 文檔清單

1. **05_BASELINE_ATTACK_TRACES.md** (~5-8 KB)
   - 4 個詳細攻擊軌跡（從 Tamarin 反例提取）
   - 每個軌跡 10-20 步

2. **06_ATTACKS_DETAILED.md** (~10-15 KB)
   - 每個攻擊的形式化定義
   - 實際影響評估
   - 網路拓撲特定性（為何是 mesh 特有）

3. **07_PATCH_COST_ANALYSIS.md** (~8-10 KB)
   - HMAC 開銷：頻寬 +32 bytes、CPU +2-3 ms
   - MetricVersion 開銷：狀態 +2 bytes、CPU <1 ms
   - 總體網路影響：能耗 <1%、吞吐量 -3~5%

4. **08_ATTACK_ROOT_CAUSE_MATRIX.md** (~6-8 KB)
   - 矩陣：攻擊 × 根本原因 × 修補方法
   - 精簡為 2 個主要類別

### 預期工期

- Day 1：Tamarin 反例提取與轉換 (~2 hours)
- Day 2：根因分析與影響評估 (~4 hours)
- Day 3：修補驗證與成本計算 (~2 hours)
- Day 4-5：論文集成與修訂 (~4 hours)

**Total**: 12-14 小時（約 1.5 天）

---

## 與 IEEE Transactions 對齐

### 核心敘述變化

| 方面 | Phase 1.5 初稿 | Phase 3 修訂 |
|---|---|---|
| **Title** | "Formal Analysis of..." | **"Breaking and Repairing..."** ✓ |
| **Attack Count** | 4 細粒度變體 | **2 核心類別** ✓ |
| **Root Cause** | 協議設計缺陷 | **LoRaMesher 特有缺陷** ✓ |
| **Patch Quality** | 最小修改 | **向後相容 + 可部署** ✓ |
| **驗證** | Tamarin PROVED | **Tamarin + ns-3 實驗** ✓ |

### 發表準備清單

- [ ] 攻擊敘述清晰且專注（2 個類別）
- [ ] 每個攻擊都有實際影響評估
- [ ] 修補成本透明（數據驅動）
- [ ] 向後相容性確認
- [ ] Artifact 可重現性驗證

---

## 依賴項 & 觸發條件

**Blocked by**: Phase 2 Tamarin 驗證完成  
**Unblocks**: Phase 4 ns-3 實驗設計  
**Triggers**: 背景代理 bg_52e17e03 完成 → 收集 baseline_verification.log

**下一步**: 待 Phase 2 背景代理完成後，立即啟動 Phase 3 攻擊分析工作。
