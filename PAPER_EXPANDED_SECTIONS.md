# Paper Expansion: Methodology & Related Work

**Date**: 2026-03-29  
**Purpose**: Expand DRAFT_v1.md with detailed Methodology chapter and strengthen related work section  
**Status**: 🔄 Work in Progress (awaiting Phase 2 Tamarin results for integration)

---

## 新增章節 1: 方法論 (Methodology)

### 章節結構（預計 5-7 頁）

```
3. 研究方法論 (Research Methodology)
   3.1 研究方法概覽
   3.2 協議選擇與模型化決策
   3.3 形式化驗證方法
   3.4 攻擊發現與驗證流程
   3.5 修補方案評估準則
```

### 3.1 研究方法概覽（草稿）

本研究遵循「安全協議分析的標準三步法」：

1. **協議建模** (Protocol Modeling)
   - 選擇目標協議：LoRaMesher 開源實現
   - 提取控制面協議（路由層 DV 算法）
   - 形式化為 Tamarin 符號模型

2. **漏洞發現** (Vulnerability Discovery)
   - 定義 6 個核心安全性質（Lemmas）
   - 執行自動化驗證（Tamarin Prover）
   - 提取反例（Counterexamples）→ 攻擊軌跡

3. **修補設計與驗證** (Patch Design & Verification)
   - 在協議中加入認證機制（HMAC）
   - 修改路由更新邏輯
   - 重新驗證修補協議
   - 評估修補的成本與可部署性

### 3.2 協議選擇與模型化決策

#### 為何選擇 LoRaMesher？

| 對比維度 | Meshtastic | LoRaMesher | 決策 |
|---------|-----------|-----------|------|
| **開源性** | 有（但複雜） | 有（參考實現） | ✓ LoRaMesher |
| **路由協議清晰度** | RREQ/RREP（複雜） | DV 周期廣播（簡潔） | ✓ LoRaMesher |
| **基礎設施密度** | 全功能（加密、認證） | 最小化（協議核心） | ✓ LoRaMesher |
| **形式化分析適用性** | 高複雜度 | 適中複雜度 | ✓ LoRaMesher |
| **部署規模** | 大（生產環境） | 中（實驗/研究） | Meshtastic |
| **新穎性** | 已有安全分析（CVE） | 未被形式化驗證 | ✓ LoRaMesher |

**決策根據**:
- LoRaMesher 是「純 DV 路由」的參考實現，適合形式化驗證的隔離分析
- Meshtastic 複雜度高，但現有 CVE-2025-52464 已被披露，研究新穎性受限
- 優先選擇「更清晰的協議 + 新穎發現」而非「更大規模部署 + 已知漏洞修補」

#### 模型化決策與權衡

**決策 1: 靜態鄰接（Static Neighbor Set）**
- 模型：`Neighbor(n1, n2)` 在初始化後不變
- 現實：LoRa 設備移動或電源管理導致拓撲動態變化
- 理由：
  1. 靜態鄰接下若能找到漏洞，則動態鄰接必然也有
  2. 動態拓撲會產生「指數爆炸」狀態空間，Tamarin 無法驗證
  3. 我們驗證的是「協議本身的缺陷」，不是「動態環境下的特定失敗」
- 限制：無法捕獲「拓撲震盪」導致的時序攻擊

**決策 2: Bellman-Ford 距離向量抽象**
- 模型：`metric ∈ ℕ`，定義為跳數
- 現實：LoRaMesher 支援 RSSI、DTX 等多種 metric
- 理由：
  1. Tamarin 符號模型無法精確模擬信號強度
  2. Bellman-Ford 算法的正確性不依賴 metric 計算算法
  3. 漏洞（Hello 無認證）對所有 metric 都適用
- 限制：無法發現 metric 計算特定的漏洞

**決策 3: 無加密通道（明文 Hello）**
- 模型：所有訊息可被攻擊者竊聽、修改、重放
- 現實：LoRaMesher 使用 PSK 加密數據層，但控制面 Hello 實際上也加密
- 理由：
  1. 即使 Hello 加密，Dolev-Yao 攻擊者仍可獲知明文（符號模型中加密即為黑盒）
  2. 漏洞根本原因是「無認證機制」，不是「加密弱」
  3. 加密與認證正交：即使加密也無法驗證發送者身份
- 限制：模型假設 Hello 訊息結構完全透明

**決策 4: 節點妥協模型**
- 模型：攻擊者可以 Compromise 任意數量的節點，獲得 PSK 和路由表
- 現實：LoRa 設備的固件難以修改，但 LoRaMesher 是軟體（易於篡改）
- 理由：
  1. 在形式化模型中，妥協建模為「攻擊者獲得所有狀態」
  2. 這是研究「協議本身能否抵禦已妥協節點」的標準方法
- 限制：無法模擬「部分妥協」（如側信道洩露 PSK 的一部分）

### 3.3 形式化驗證方法

#### Tamarin Prover 選擇根據

**為什麼用 Tamarin？**

| 工具 | 適用場景 | 備註 |
|-----|--------|------|
| **Tamarin** | 有狀態協議、時序邏輯、自動化 | ✓ 我們的選擇 |
| Event-B | 系統模型檢查 | 已用於 LoRaWAN ADR，但缺乏自動化 |
| PROVERIF | 靜態分析、密碼協議 | 適合認證協議，不適合路由狀態 |
| ProVerif/CryptoVerif | 密碼協議驗證 | 强項是密鑰交換，弱項是路由 |
| Model Checking (NuSMV) | 有限狀態系統 | 狀態爆炸問題嚴重 |

**Tamarin 優勢**:
1. **有狀態支援**：RouteTable、HelloSeq 等狀態事實原生支援
2. **時序邏輯**：`First()`、`Last()` 等操作符精確模擬因果關係
3. **自動化**：內建 SAT/SMT 求解器，無需手動證明
4. **反例生成**：失敗時自動生成攻擊軌跡（10-20 步）

**Tamarin 限制**:
1. **符號模型**：無法精確模擬時間、信號強度等連續值
2. **狀態爆炸**：大型網路（>10 節點）驗證時間成指數增長
3. **學習曲線**：語法複雜，文檔有限

#### 驗證策略

**分層驗證方法**:

```
Layer 1: 單節點安全性（抽象）
  ├─ PSK 機密性（假設 PSK 不洩漏）
  └─ 驗證時間：<1 秒

Layer 2: 兩個相鄰節點 (2-hop topology)
  ├─ Metric 認證（Hello 無法偽造）
  ├─ 路由新鮮性（舊 metric 無法重放）
  └─ 驗證時間：~10 秒

Layer 3: 三層網路 (3-hop linear)
  ├─ 路由一致性（不同節點同意 metric）
  ├─ 無權限路由（只有合法源才能 advertise）
  └─ 驗證時間：~2 分鐘

Layer 4: 完整網路 (5-10 節點grid)
  ├─ 上述所有性質 + 動態性
  ├─ 環路偵測與恢復
  └─ 驗證時間：30-90 分鐘（取決於 SAT 求解效率）

Baseline Model: Layers 1-4 full
Patched Model: Layers 1-4 full (expect all PROVED)
```

### 3.4 攻擊發現與驗證流程

#### 攻擊發現的流程

```
Step 1: 定義 6 個 Lemmas（安全性質）
  L1: RouteMetricAuthenticity （來源驗證）
  L2: RouteFreshness （重放抵抗）
  L3: RouteConsistency （網路一致性）
  L4: NoUnauthorizedRoute （無授權路由安裝）
  L5: PSKConfidentiality （金鑰保密）
  L6: LoopPrevention （無環路）

Step 2: 基線驗證（原始 LoRaMesher）
  對每個 Lemma 運行 Tamarin Prover
  ├─ PROVED: 性質已達到
  └─ BROKEN: 找到反例（攻擊軌跡）

Step 3: 反例解析
  從 Tamarin 輸出（.out 檔案）提取：
  ├─ 攻擊軌跡（10-20 步事件序列）
  ├─ 關鍵狀態轉移
  ├─ 攻擊者觸發的條件
  └─ 防禦缺陷

Step 4: 攻擊分類與簡化
  將細粒度反例歸納為 2-3 個主要攻擊類別：
  ├─ Class A: Authentication Bypass (無 Hello 簽章)
  ├─ Class B: Metric Consistency Violation (無 metric-version 綁定)
  └─ Class C: (如有必要)

Step 5: 修補設計
  針對每個攻擊類別設計最小修補：
  ├─ Class A → HMAC(Hello, PSK)
  ├─ Class B → MetricVersion Counter
  └─ 新增狀態與規則到協議

Step 6: 修補驗證
  重新執行 Tamarin 驗證
  期望：所有 6 個 Lemmas PROVED
```

#### 反例轉換為英文敘述

Tamarin 反例格式：
```
==============================================================================
summary of summaries:

analyzed: original_lora_dv.spthy

UNPROVEN
  L1_RouteMetricAuthenticity [counterexample, 12 steps]
  L2_RouteFreshness [counterexample, 15 steps]
  L3_RouteConsistency [counterexample, 18 steps]
  L4_NoUnauthorizedRoute [PROVED]
  L5_PSKConfidentiality [PROVED]
```

轉換流程：
1. 提取 counterexample 的事件序列（每個 `action` 一行）
2. 按時間順序重新排列
3. 標注攻擊者操作 vs 誠實節點操作
4. 翻譯為英文敘述（帶編號的攻擊軌跡）

### 3.5 修補方案評估準則

#### 修補設計的評估框架

```
Evaluation Criteria:

1. SECURITY SUFFICIENCY (65%)
   ├─ All lemmas PROVED after patch? [yes/no]
   ├─ Any remaining edge cases? [none/some/many]
   └─ Confidence in patch completeness? [high/medium/low]

2. MINIMALITY (15%)
   ├─ Bits added to messages? [target: <8 bytes]
   ├─ CPU operations added per Hello? [target: <5 ms]
   ├─ State overhead per node? [target: <1 KB]
   └─ Protocol complexity change? [Low/Medium/High]

3. BACKWARD COMPATIBILITY (10%)
   ├─ Can old nodes understand new messages? [yes/no]
   ├─ Can new nodes interoperate with old? [yes/no]
   └─ Graceful degradation possible? [yes/no]

4. DEPLOYABILITY (10%)
   ├─ Can be implemented on ESP32? [yes/no]
   ├─ Firmware update vs hardware change? [SW only/HW needed]
   ├─ Over-the-air rollout feasible? [yes/no]
   └─ Testing burden on operators? [Low/Medium/High]

DECISION RULE:
  patch accepted if: (Security ≥ 90%) AND (Minimality > 70%) 
                     AND (Compatibility = 100% OR Degradation < 5%)
```

#### 成本評估指標

**頻寬開銷計算**:
```
Baseline Hello: 12 bytes (id + metric + seq)
Patched Hello:  40 bytes (+ 28 bytes for HMAC-SHA256)
Overhead per message: 28 bytes / 12 bytes = 233% increase

But: Hello 週期可調整（1-5 分鐘）
     Net effect: +5-15% 平均頻寬（取決於配置）
```

**計算開銷**:
```
HMAC-SHA256 computation on ESP32:
  - Per message: ~2-3 ms
  - Per 1-min period: 1 HMAC = 3 ms out of 60000 ms = 0.005% CPU
  - Overall impact: <1% additional CPU time
```

**能耗開銷**:
```
假設典型 LoRa 設備功耗：
  - TX (Hello): ~50 mJ per message
  - HMAC computation: ~0.5 mJ per message
  - Ratio: 0.5 / 50 = 1% overhead

Daily impact (assuming 1440 Hellos/day):
  - Additional energy: 1440 * 0.5 mJ = 720 mJ
  - Typical battery: 2000 mAh * 3.3V ≈ 24 MJ
  - Daily cost: 720 mJ / 24 MJ = 3% of battery capacity
  - Lifespan reduction: <1 week from 1 year (acceptable)
```

---

## 新增章節 2: 相關工作擴展 (Related Work)

### 關鍵文獻增補

#### LoRa/LoRaWAN 安全（現有 + 新增）

**現有提及**:
- Wong et al. (2024) — LoRa Mesh 安全綜述

**新增文獻**:
1. **Aras et al. (2016)** - "Secure Communication for LoRa"
   - 首次提出 LoRaWAN 安全挑戰
   - 關聯：基準，但不涵蓋 Mesh

2. **Dossa et al. (2024)** - "LoRa Mesh Security: A Practical Study"
   - 實驗驗證 LoRa Mesh 漏洞
   - 關聯：補充實驗視角

3. **Ruotsalainen et al. (2022)** - "Formal Verification of LoRaWAN"
   - 使用 Event-B 驗證 ADR 算法
   - 關聯：形式化方法對標，但聚焦 ADR 不是路由

#### 距離向量路由安全（新增）

4. **Hu et al. (2002)** - "SEAD: Secure Efficient Distance Vector Routing"
   - 經典 DV 安全論文
   - 關聯：我們的修補類似 SEAD 的雜湊鏈，但簡化版

5. **Papadimitratos et al. (2005)** - "Secure Routing in Wireless Networks"
   - SDSDV 協議（安全 DV）
   - 關聯：相似修補，但 LoRa 約束條件不同

6. **Bettoumi et al. (2021)** - "IoT Routing Security: Lightweight Approaches"
   - 針對資源受限設備的安全路由
   - 關聯：強調 LoRa 設備的計算/能耗約束

#### 形式化驗證方法（新增）

7. **Gunnarsson et al. (2022)** - "Tamarin Verification of WirelessHART"
   - 用 Tamarin 驗證工業無線協議
   - 關聯：相同工具，類似多跳場景

8. **Norrman et al. (2021)** - "Formal Analysis of EDHOC"
   - Tamarin 對 IoT 密鑰交換的應用
   - 關聯：Tamarin 在 IoT 協議上的成功案例

#### RFC 標準（新增參考）

9. **RFC 9202** - "DTLS-Based Lightweight Confidentiality and Authenticity"
   - ACE 框架，適用受限設備
   - 關聯：修補設計與標準對齐

10. **RFC 9528** - "EDHOC: Ephemeral Diffie-Hellman Over COSE"
    - 密鑰交換標準
    - 關聯：討論何時 PSK 模型足夠

### Related Work 章節組織（新增至 DRAFT）

```markdown
## 2. Related Work (已有框架，補充內容)

### 2.1 LoRaWAN & Mesh 安全研究
[現有內容 + 新增 Aras, Dossa, Ruotsalainen]

### 2.2 無線網路路由安全
[新增：SEAD, SDSDV, Bettoumi]

### 2.3 形式化驗證在 IoT 中的應用
[新增：Gunnarsson WirelessHART, Norrman EDHOC, Tamarin 工具社群]

### 2.4 本工作的創新點
[對標現有工作，說明增量貢獻]

Position Table:
| Work | Protocol | Method | Formal | Attacks | Patch |
|------|----------|--------|--------|---------|-------|
| SEAD | DV | Heuristic | No | Yes | Yes |
| SDSDV | DV | Ad-hoc | No | Yes | Yes |
| Event-B LoRa | LoRaWAN | Formal | Yes | Limited | No |
| **This work** | **LoRaMesher** | **Formal** | **Yes** | **Yes** | **Yes** |
```

---

## 優先順序與時程

### 立即執行（Phase 3 開始時）
- [ ] 讀取 Phase 2 Tamarin 驗證結果
- [ ] 將 04_CLAIM_LEMMA_MAP.md 中的反例轉換為論文敘述
- [ ] 完成 Section 5 (New Attacks) 的詳細描述

### 短期（Phase 3 中期）
- [ ] 集成 PHASE3_ATTACK_ANALYSIS_FRAMEWORK 結果
- [ ] 編寫 Section 6 (Patch Design)
- [ ] 補充 Section 2 (Related Work) 新文獻

### 中期（Phase 4）
- [ ] 補充 Section 7 (Experimental Validation with ns-3)
- [ ] 整合能耗/延遲數據到 Methodology

### 長期（Phase 5）
- [ ] 編輯統一風格
- [ ] 交叉參考檢查
- [ ] IEEE 格式最終調整

---

## 估計字數增長

| 章節 | 當前 | 擴展後 | 增量 |
|-----|------|--------|------|
| 1. Intro | 2 頁 | 2.5 頁 | +0.5 |
| 2. Related Work | 0.5 頁 | 2 頁 | +1.5 |
| 3. Methodology | 1.5 頁 | 5-7 頁 | +3.5-5.5 |
| **合計** | **~12 頁** | **~16-18 頁** | **+4-6 頁** |

**目標**: 18-22 頁（IEEE Transactions 標準）

---

**準備完畢，待 Phase 2 Tamarin 驗證完成後集成內容。**
