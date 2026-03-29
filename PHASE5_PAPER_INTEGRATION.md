# Phase 5: 論文最終集成與投稿準備

**Date**: 2026-03-29  
**Purpose**: Integrate all Phase 2-4 results into DRAFT_v1.md and prepare for IEEE Transactions submission  
**Status**: 🔄 Planning (awaiting Phase 4 ns-3 results)  
**Target Timeline**: Week 9-20

---

## 概述 (Overview)

Phase 5 是最終階段，將 Phase 2-4 的形式化驗證、攻擊分析、成本評估和實驗結果整合成完整的期刊論文。

### 工作分解 (Work Breakdown)

| Phase | Deliverable | Status | Dependency |
|-------|-------------|--------|------------|
| 1.5 | Tamarin preparation | ✅ DONE | - |
| 2 | Baseline + Patched models | ✅ DONE | - |
| 3 | Attack analysis + Cost | ✅ DONE | Phase 2 |
| 4 | ns-3 experiments | 🔄 RUNNING | Phase 3 |
| **5** | **Final paper + submission** | ⏳ PENDING | Phase 4 |

---

## Phase 5 詳細計畫

### 5.1 論文結構最終化 (Paper Structure)

**目標頁數**: 18-22 頁 (IEEE Transactions 標準)

```
1. Abstract (0.5 page)
   - Problem statement
   - Methods (Tamarin + ns-3)
   - Key findings (attacks + patches)
   - Impact

2. Introduction (1.5-2 pages)
   - Motivation: LoRa Mesh security gap
   - Related work positioning
   - Contributions (5 bullet points)
   - Paper organization

3. Threat Model & Security Goals (1-1.5 pages)
   - Dolev-Yao attacker capabilities (6 points)
   - System boundary (in/out scope)
   - Security properties (5 lemmas)

4. Protocol & Formal Model (2-2.5 pages)
   - LoRaMesher protocol phases A-D
   - Tamarin model overview
   - Fact/rule/lemma definitions
   - Model limitations & assumptions

5. Formal Verification Results (1.5-2 pages)
   - Baseline results table (4 BROKEN + 1 PROVED)
   - Counterexample summary
   - Patched results table (5 PROVED)
   - Verification statistics

6. New Attacks Discovered (2-3 pages)
   - Attack Class I: Authentication Bypass
     * Hello Spoofing variant 1-3
     * Success rate: 70-80%
     * Impact: Black hole, MITM
   - Attack Class II: Metric Consistency Violation
     * Metric Replay + Selective Forwarding
     * Success rate: 60-75%
     * Impact: Partitioning, loops
   - Mesh-specific characteristics
   - Comparison with prior work

7. Patch Design & Verification (1.5-2 pages)
   - HMAC authentication mechanism
   - MetricVersion binding logic
   - Patched protocol specification
   - Formal verification proof (all 5 lemmas PROVED)
   - Patch minimality argument

8. Experimental Validation (2-3 pages)
   - ns-3 simulation methodology
   - Topology design (3 types)
   - Attack implementation details
   - Baseline vs Patched results table
   - Performance metrics (latency, throughput, PDR)
   - Patch overhead analysis

9. Cost Analysis & Deployability (1-1.5 pages)
   - Message size overhead (+32 bytes)
   - Computational overhead (2-3 ms)
   - Energy overhead (<1%)
   - Deployment considerations
   - Backward compatibility strategy

10. Related Work (1.5-2 pages)
    - LoRaWAN/Mesh security (4-5 papers)
    - Wireless routing security (4-5 papers)
    - Formal verification (3-4 papers)
    - Differentiation table

11. Discussion & Limitations (1 page)
    - Model assumptions vs reality
    - Generalizability to other protocols
    - Future work directions (5 items)

12. Conclusion (0.5 page)
    - Summary of contributions
    - Significance for IoT security
    - Call for action (patch adoption)

Appendices (4-8 pages optional):
- A: Complete Tamarin code (baseline + patched)
- B: ns-3 simulation code
- C: Extended proofs
- D: Additional experimental data
```

### 5.2 章節修訂流程 (Section Revision Process)

#### Section 1-3: 導論與威脅模型
- **Status**: ✅ READY (已在 DRAFT_v1.md + PAPER_EXPANDED_SECTIONS.md)
- **Action**: 集成 PAPER_EXPANDED_SECTIONS.md 的 Methodology 章節
- **Timeline**: 2 hours

#### Section 4: 形式化驗證結果
- **Status**: ✅ READY (已在 VERIFICATION_SUMMARY.md + BASELINE_VERIFICATION_RESULTS.md)
- **Action**: 整合驗證結果表、統計數據、置信度評估
- **Timeline**: 1 hour

#### Section 5: 新發現的攻擊
- **Status**: ✅ READY (已在 ATTACKS_DETAILED.md)
- **Action**: 轉換為期刊格式，補充成功率數據、實際場景
- **Timeline**: 1.5 hours

#### Section 6: 修補設計
- **Status**: ✅ READY (已在 DRAFT_v1.md + PATCH_COST_ANALYSIS.md)
- **Action**: 完善修補驗證論證，補充向後相容性討論
- **Timeline**: 1 hour

#### Section 7: 實驗驗證
- **Status**: 🔄 PENDING Phase 4
- **Action**: 集成 ns-3 結果、性能數據、對比分析
- **Timeline**: 2-3 hours (awaiting Phase 4 output)

#### Section 8-9: 成本與部署
- **Status**: ✅ READY (已在 PATCH_COST_ANALYSIS.md)
- **Action**: 整合能耗、延遲、吞吐量數據；補充部署指南
- **Timeline**: 1 hour

#### Section 10: 相關工作
- **Status**: ✅ READY (已在 PAPER_EXPANDED_SECTIONS.md)
- **Action**: 補充 10-15 篇新文獻，完成區分表
- **Timeline**: 1.5 hours

#### Section 11-12: 討論與結論
- **Status**: ⏳ PENDING
- **Action**: 撰寫新章節（未在任何文檔中）
- **Timeline**: 2 hours

### 5.3 圖表與數據視覺化

**必須圖表** (8-10 個):

1. **Figure 1**: Protocol phases (4 phases A-D, sequence diagram)
   - Source: 01_PROTOCOL_ABSTRACTION.md §2-4
   - Time: 30 min

2. **Figure 2**: Attack Class I - Hello Spoofing
   - Attacker position diagram + attack flow
   - Time: 45 min

3. **Figure 3**: Attack Class II - Metric Replay
   - Timeline + metric evolution
   - Time: 45 min

4. **Figure 4**: Tamarin model overview (rule graph)
   - Rules: Node_Init → Hello_Broadcast → Route_Update → Data_Forward
   - Time: 1 hour

5. **Figure 5**: Attack success rate comparison (bar chart)
   - X: Topology × Attack Type
   - Y: Success rate (%)
   - Data source: EXPERIMENTAL_RESULTS_BASELINE.md
   - Time: 30 min

6. **Figure 6**: Patch overhead analysis
   - Subplots: Message size, CPU time, energy consumption
   - Time: 1 hour

7. **Figure 7**: Baseline vs Patched verification results
   - Lemma status table + confidence levels
   - Time: 30 min

8. **Figure 8**: Lemma verification timeline
   - X: Verification time (seconds)
   - Y: Lemmas PROVED/BROKEN
   - Time: 30 min

9. **Figure 9**: ns-3 simulation topologies
   - Linear, Tree, Grid visualizations
   - Time: 45 min

10. **Figure 10**: Performance metrics (latency, throughput, PDR)
    - Box plots: Baseline vs Patched across topologies
    - Time: 1 hour

**必須表格** (6-8 個):

1. **Table 1**: Security goals vs Lemmas
   - 5 security goals × 5 lemmas = mapping

2. **Table 2**: Baseline verification results
   - Lemma × Result × Confidence × Counterexample steps

3. **Table 3**: Patched verification results
   - Lemma × Result × Confidence × Protection mechanism

4. **Table 4**: Attack characteristics
   - Attack type × Root cause × Success rate × Detectability

5. **Table 5**: Patch cost analysis
   - Metric × Baseline × Patched × Overhead

6. **Table 6**: ns-3 simulation scenarios
   - Topology × Attack × Runs → Results

7. **Table 7**: Performance comparison
   - Metric × Baseline μ ± σ × Patched μ ± σ × Improvement

8. **Table 8**: Related work comparison
   - Work × Protocol × Method × Formal × Attacks × Patch

**Production Time**: 6-8 hours (all figures + tables)

### 5.4 文本編輯與品質檢查

#### 第一輪：內容檢查 (1 day)
- [ ] 所有章節邏輯完整性檢查
- [ ] 引用完整性驗證 (40-50 篇文獻)
- [ ] 數據準確性檢查 (數字、統計)
- [ ] 假設清晰性 (列表、驗證)

#### 第二輪：文本編輯 (1 day)
- [ ] 句子長度優化 (<25 字)
- [ ] 被動語態轉主動
- [ ] 冗餘詞彙刪除
- [ ] 段落過渡優化

#### 第三輪：格式與合規 (0.5 day)
- [ ] IEEE 模板合規檢查
- [ ] 引用格式統一 (Author-Year)
- [ ] 頁碼與交叉引用
- [ ] 圖表編號與標題

#### 第四輪：同行評審 (2-3 days)
- [ ] 2-3 人外部評審
- [ ] 意見收集與修訂
- [ ] 迭代改進

### 5.5 最終投稿準備

#### 投稿包準備 (1 day)
- [ ] 主稿 (manuscript_lora_mesh_security.pdf)
- [ ] 補充材料 (appendix_tamarin_code.pdf)
- [ ] 補充材料 (appendix_ns3_code.pdf)
- [ ] 可重現性 artifact (code + Docker)
- [ ] 投稿信 (cover_letter.pdf)
- [ ] 建議評審人清單

#### 期刊選擇與投稿 (1 day)
- [ ] IEEE TDSC (首選，IF 3.8+)
- [ ] IEEE IoT Journal (備選，IF 3.5+)
- [ ] 聯繫編輯確認方向
- [ ] 提交至期刊系統

---

## Phase 5 工作時程

### 週次分解 (Week-by-Week Breakdown)

**Week 9** (Phase 4 進行中):
- [ ] 準備所有圖表框架
- [ ] 補充相關工作文獻
- [ ] 準備 Section 11-12 草稿

**Week 10**:
- [ ] Phase 4 完成 → 接收 ns-3 結果
- [ ] Section 7 集成 (實驗結果)
- [ ] 完成所有圖表與表格

**Week 11-12**:
- [ ] 第一輪內容檢查 (邏輯完整性)
- [ ] 所有章節整合到單一文檔
- [ ] 初稿完成 (18-22 頁)

**Week 13**:
- [ ] 第二輪文本編輯 (語言質量)
- [ ] 格式與合規檢查
- [ ] v1.0 「準備投稿版」完成

**Week 14**:
- [ ] 同行評審 (2-3 人)
- [ ] 收集反饋與建議
- [ ] 開始修訂

**Week 15** (原定投稿時間):
- [ ] 完成修訂 (Major/Minor)
- [ ] 準備投稿包 (稿件 + appendix + artifact)
- [ ] 投稿信撰寫
- [ ] **提交至 IEEE TDSC**

**Week 16-20**:
- [ ] 等待編輯初審 (2-4 週)
- [ ] 編輯分配評審人 (1-2 週)
- [ ] 首輪評審意見反饋

---

## 成功標準 (Success Criteria)

### 論文品質指標

| Criterion | Target | Status |
|-----------|--------|--------|
| **字數** | 18-22 頁 | 待測 |
| **圖表** | ≥10 個 | 待準備 |
| **引用** | 40-50 篇 | ✅ 已達 50+ |
| **新穎性** | 8-10 分 | 預期達到 |
| **深度** | 8-10 分 | 預期達到 |
| **影響力** | 8-10 分 | 預期達到 |

### 投稿準備指標

| Item | Status |
|------|--------|
| 稿件完成 | ⏳ Week 13 |
| 同行評審完成 | ⏳ Week 14 |
| 修訂完成 | ⏳ Week 15 |
| 投稿包準備 | ⏳ Week 15 |
| **投稿提交** | ⏳ **Week 15** |

---

## 投稿決策樹 (Decision Tree)

### 如果 Phase 4 成功（預期）

```
✅ ns-3 結果支持形式化預測
  └─ Baseline: 60-80% 攻擊成功率
  └─ Patched: 0% 攻擊成功率
  └─ Overhead: <1% 能耗

→ 立即進入 Week 9-15 完整計畫
→ IEEE TDSC 優先投稿
→ 預期 accept 概率: 70-80%
```

### 如果 Phase 4 遇到問題

```
⚠️ ns-3 結果不完整或有偏差
  └─ 攻擊成功率低於預期
  └─ 修補效果不完美

→ 診斷與修正 (1-2 週)
→ 補充說明 (論文加註說明)
→ 可能轉向 IEEE IoT Journal (接受率更高)
→ 預期 accept 概率: 50-70%
```

### 如果 Phase 4 失敗

```
❌ ns-3 模擬環境問題
  └─ 無法實現理想的 LoRa 物理層
  └─ 模塊化困難或時間超期

→ 基於形式化驗證提交 (Tamarin 結果已完整)
→ 減弱實驗驗證部分，強調形式化貢獻
→ 論文篇幅: 16-18 頁 (略短但可接受)
→ 預期 accept 概率: 40-60%
```

---

## 備忘錄 (Notes)

### 關鍵決策

1. **投稿期刊排序**
   1. IEEE TDSC (首選，IF 3.8, 接受率 15-20%)
   2. IEEE IoT Journal (備選，IF 3.5+, 接受率 25-35%)
   3. IEEE Transactions on Network and Service Management (第三選，IF 3.2, 接受率 20-25%)

2. **論文定位**
   - **Title**: "Breaking and Repairing LoRa Mesh Control Plane: Formal Analysis, Attacks, and Verified Mitigation"
   - **Narrative**: 質量優先 (2 個核心攻擊 > 4 個細節變體)
   - **Novelty**: 首個 LoRaMesher 控制面形式化驗證 + 新攻擊發現

3. **投稿時機**
   - 目標: 2026 年 7 月底 (Week 15)
   - 審查週期: 4-6 個月 (IEEE Transactions 標準)
   - 預期接受: 2026 年 11-12 月

### 風險項

| Risk | Probability | Mitigation |
|------|-------------|-----------|
| Phase 4 超期 | Medium | 前置準備圖表、草稿論文框架 |
| 同行評審延遲 | Low | 提前 2 週完成準備投稿版 |
| Minor revision 要求 | High | 預留 1 週修訂時間 |
| 第一次被拒 | Low (5%) | 備選期刊清單已準備 |

---

## 下一步 (Next Steps)

1. **立即** (本週):
   - 監控 Phase 4 進度
   - 準備圖表框架
   - 補充相關工作文獻

2. **Phase 4 完成後** (下週):
   - 接收 ns-3 結果
   - 開始 Section 7 整合
   - 完成所有圖表

3. **Week 10-13**:
   - 完整論文整合
   - 第一版投稿前準備

4. **Week 14-15**:
   - 同行評審
   - 修訂與投稿

---

**Status**: 🟡 **READY FOR PHASE 4 COMPLETION**  
**Next Milestone**: Phase 4 ns-3 實驗完成 (預計 24-48 小時)
