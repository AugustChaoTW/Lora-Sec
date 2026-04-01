# 🚀 Week 2 并行启动摘要
## LoRa Mesh 优化计划 — Day 8-14 三向并行执行

**启动时间**: 2026-03-29 (Week 1 Day 1) → Week 2 Day 8启动  
**前置条件**: Day 2-5 里程碑检查通过  
**执行模式**: 三向完全并行  
**预期完成**: Week 2 Day 14 (2026-04-02)

---

## 📊 三向并行任务总览

| 轨道 | Agent | 任务 | 工期 | 会议 |
|------|-------|------|------|------|
| **A: Tamarin** | [deep] | executability + sanity lemmas | 6-8h | bg_57f87859 |
| **B: ns-3** | explore | 360场景因子矩阵设计 | 5.5h | bg_55f98ae8 |
| **C: Paper** | [artistry] | Related Work 骨架 | 7h | bg_28430263 |

**总工作量**: 18.5小时 ÷ 3轨道 = **6.17小时/轨道** (可在Day 8-9完成)

---

## 🎯 Week 1 Day 2-5: 阻塞链（并行自动化）

### Day 2-3: ns-3 PDR验证 (自动化)

**使用脚本**: `phase4_ns3_simulation/verify_fix.sh`

```bash
# 启动快速验证
cd /home/augchao/Lora-Sec/phase4_ns3_simulation
./verify_fix.sh

# 预期流程:
# Phase 1: 单场景 (Linear/NoAttack)  → 期望 PDR > 80%
# Phase 2: 三拓扑集 (Linear/Tree/Grid) → 期望 全部 PDR > 0%
# 时间: 约30分钟
```

**成功标准**:
```
✅ Phase 1: PDR > 80% for baseline
✅ Phase 2: PDR > 0% for all 3 topologies
→ Proceed to Day 3: Full 27-scenario rerun
```

**失败回滚**:
```
❌ Phase 1: PDR = 0% 
→ 检查 lora-mesh-sim.cc L283-344 的修改
→ 确认 dataStartSec = 120 已应用
```

### Day 3: 27场景完整重跑

```bash
# 启动完整验证 (约90分钟，GPU并行)
python3 phase4_ns3_simulation/scratch/lora-mesh-experiment-gpu-parallel.py \
  --topologies Linear Tree Grid \
  --attacks Spoofing Replay Selective \
  --patches 0 1 \
  --seeds 1 2 3 \
  --workers 10 \
  --duration 420

# 完成后分析
cd phase4_ns3_simulation
python3 analyze_pdr.py

# 预期输出:
# Total scenarios: 27
# PDR > 0%: 24-27
# PDR Mean: 50-70%
# ✅ SUCCESS: Majority PDR > 50%
```

### Day 4: 数据分析报告

**自动生成**: `NS3_VERIFICATION_REPORT.md`
```markdown
# ns-3 PDR验证报告 (Week 1 Day 4)

## 总体统计
- 总场景: 27
- PDR > 0%: X个
- PDR平均: Y%
- PDR std: Z%

## 按攻击类型
- Spoofing (patch=0): mean PDR = X%
- Spoofing (patch=1): mean PDR ≈ 0% (patch有效)
- ...

## 结论
✅ 修复有效: PDR从0% → >50%
✅ 可进入Week 2: 三向并行启动
✅ 360场景升级: 基础设施验证通过
```

### Day 5: 里程碑决策

**检查表**:
```
1. ns-3修复有效? 
   ✅ YES (Day 3验证通过)

2. Tamarin工期路线?
   ✅ 决定: 路线A (6-8周, 推荐)
      - 问题1: PSK per-node key (方案B)
      - 问题2: MetricVersion动态递增 (必须)
      - 问题3: ≤3跳限定范围 (路线A)

3. Week 2三向启动?
   ✅ YES → 立即启动
      - Tamarin executability lemmas (bg_57f87859)
      - ns-3 360场景矩阵设计 (bg_55f98ae8)
      - Related Work骨架 (bg_28430263)
```

**签署**: `WEEK_1_DECISION.md`

---

## 📋 Week 2 Day 8-14: 三向并行执行

### 轨道 A: Tamarin Executability + Sanity Lemmas

**Agent**: Sisyphus-Junior [deep]  
**Session**: ses_2c6da9ff4ffebkJC9FzBDHR3aF  
**Task**: bg_57f87859

**Day 8-9 输出**:
1. `baseline_lora_dv.spthy` with 4 executability lemmas
   - L_Ex1_Node_Can_Init
   - L_Ex2_Hello_Can_Broadcast
   - L_Ex3_Route_Can_Update
   - L_Ex4_Data_Can_Forward
   - 验证: All VERIFIED < 60s each

2. `patched_lora_dv.spthy` with 3 sanity lemmas
   - L_Sanity1_Honest_Route_Exists
   - L_Sanity2_HMAC_Verified
   - L_Sanity3_MetricVersion_Increment_Happens
   - 验证: All VERIFIED < 90s

3. `WEEK_2_TAMARIN_VERIFICATION.md` 完整报告

**Day 10 验证**:
```bash
cd phase2_tamarin_models
tamarin-prover baseline/baseline_lora_dv.spthy --prove > baseline/logs/verify.log
tamarin-prover patched/patched_lora_dv.spthy --prove > patched/logs/verify.log

# 检查输出
grep "VERIFIED\|FALSIFIED" baseline/logs/verify.log
grep "VERIFIED\|FALSIFIED" patched/logs/verify.log
```

**里程碑**: Week 2 Day 10 ✅

---

### 轨道 B: ns-3 360场景因子矩阵设计

**Agent**: explore  
**Session**: ses_2c6da6384ffecaenKpRMVRhLEv  
**Task**: bg_55f98ae8

**Day 8-9 输出**:
1. `EXPERIMENT_MATRIX_DESIGN.md` (2000+ 字)
   - 因子定义与矩阵布局
   - 360场景的完整列表
   - 分批运行计划
   - 统计功率分析

2. Updated `lora-mesh-experiment-gpu-parallel.py`
   ```python
   SEEDS = 20  # 原来是 3
   # 总场景: 3 × 3 × 2 × 20 = 360
   ```

3. **Dry-run 验证脚本**
   ```bash
   python3 scratch/lora-mesh-experiment-gpu-parallel.py --dry-run
   # 输出: 360 scenarios listed without error
   ```

4. 分批运行计划
   - Batch 1: Scenario 1-120
   - Batch 2: Scenario 121-240
   - Batch 3: Scenario 241-360
   - 每批可独立运行，跨Day 10-12

**Day 10 验证**:
```bash
# Dry-run verification
python3 phase4_ns3_simulation/scratch/lora-mesh-experiment-gpu-parallel.py --dry-run > /tmp/scenarios.txt
wc -l /tmp/scenarios.txt  # 应该是 360

# 确认无重复
sort /tmp/scenarios.txt | uniq | wc -l  # 也应该是 360
```

**里程碑**: Week 2 Day 10 ✅  
**后续**: Week 2 Day 10 开始启动 Batch 1 (跨 Day 10-12)

---

### 轨道 C: Related Work 骨架

**Agent**: Sisyphus-Junior [artistry]  
**Session**: ses_2c6da175affemttCBa03B28a21  
**Task**: bg_28430263

**Day 10-11 输出**:
1. `RELATED_WORK_PART1_LORAWAN.md` (~1500字)
2. `RELATED_WORK_PART2_MESH.md` (~1500字)
3. `RELATED_WORK_PART3_FORMAL.md` (~1200字)
4. `RELATED_WORK_PART4_BREAKING.md` (~1200字)
5. `RELATED_WORK_MAPPING.md` (论文映射清单)

**每个Part的结构**:
```
Part 1: LoRaWAN 安全
  1.1 LoRaWAN 协议现状 [论文A, B, C]
  1.2 已知攻击与漏洞 [论文D, E] + CVE-2025-52464
  1.3 形式化验证工作 [论文F, G]

Part 2: Mesh 路由安全
  2.1 距离向量路由基础 [论文H, I]
  2.2 Mesh环境中的特殊挑战 [论文J]
  2.3 既有防御机制与LoRaMesher缺陷 [论文K, L]

Part 3: 形式化验证方法论
  3.1 Tamarin Prover基础 [论文M, N]
  3.2 已知应用案例 [论文O, P, Q]
  3.3 局限与扩展 [论文R]

Part 4: "Breaking and Repairing"范式
  4.1 协议安全分析标准流程 [论文S, T]
  4.2 实践案例 [论文U]
  4.3 本工作的定位 [综合]
```

**Day 11 验证**:
```bash
# 统计字数
wc -w RELATED_WORK_PART*.md
# 总计: 5500+ 字 = 约2.5页（期望）

# 检查论文映射
grep -c "\[" RELATED_WORK_MAPPING.md
# 应该映射 20+篇论文的引用
```

**里程碑**: Week 2 Day 11 ✅

---

## 📊 Week 2 完成条件（Day 14）

| 轨道 | 里程碑 | 状态 | 交付 |
|------|--------|------|------|
| **Tamarin** | executability+sanity lemmas | ✅ 目标 | 4+3 lemmas |
| **ns-3** | 360场景矩阵设计完成 | ✅ 目标 | DESIGN.md + runner |
| **Paper** | Related Work 4部分完成 | ✅ 目标 | 4×PART.md |

**Day 14 最终检查**:
```bash
# Tamarin
[ -f phase2_tamarin_models/baseline/logs/verify.log ] && echo "✅ Baseline verify done"
[ -f phase2_tamarin_models/patched/logs/verify.log ] && echo "✅ Patched verify done"

# ns-3
[ -f phase4_ns3_simulation/EXPERIMENT_MATRIX_DESIGN.md ] && echo "✅ Matrix design done"

# Paper
ls RELATED_WORK_PART*.md | wc -l  # Should be 4
```

---

## 🚀 Week 3 启动条件（Day 15）

**前置条件** (Week 2全部完成):
```
✅ Tamarin executability verified
✅ ns-3 360场景Batch 1启动
✅ Related Work 4部分骨架完成
```

**Week 3 工作** (W3 Day 1-7):
```
轨道A (Tamarin): 
  - PSK source auth修复 (方案B: per-node key)
  - MetricVersion真实递增实现
  - 重新验证 (预期时间: 30-60分钟)

轨道B (ns-3):
  - Batch 1 继续运行 (Day 12-14启动的)
  - Batch 2 启动 (Day 15+)
  
轨道C (Paper):
  - 补充Related Work详细分析
  - 开始protocol section写作
```

---

## 📞 并行控制与同步

### 依赖关系
```
Week 1 (Day 1-5):
  ├─ Day 1: ✅ Triage完成
  ├─ Day 2-3: ns-3验证 ⏳
  ├─ Day 4: 分析报告 ⏳
  └─ Day 5: 决策通过 ⏳ 
            ↓
Week 2 (Day 8-14):
  ├─ Tamarin (Day 8-10) ∥ ns-3 (Day 8-9) ∥ Paper (Day 10-11)
  ├─ Day 10: 3个轨道验证 & Batch 1启动
  ├─ Day 12-14: Batch 2-3启动 ∥ Related Work最终磨合
  └─ Day 14: Week 2完成检查
            ↓
Week 3 (Day 15+):
  └─ Tamarin深化 + ns-3续跑 + Paper写作继续
```

### 风险点 & Mitigation
```
Risk 1: ns-3验证失败 (Day 2-3)
  → Mitigation: Day 1修改已验证正确，风险低
  
Risk 2: Tamarin lemmas无法证明 (Day 8-9)
  → Mitigation: executability/sanity是基础，通常能pass
  
Risk 3: 360场景因子计算错误
  → Mitigation: Dry-run验证可捕获
  
Risk 4: Related Work写作超期
  → Mitigation: Skeleton优先，Week 3补detail
```

---

## ✨ 成功信号

**Week 2 Day 14 完成标志**:
```
✅ Tamarin: 
   - baseline/logs/*.log 显示所有lemma VERIFIED
   - patched/logs/*.log 显示所有lemma VERIFIED
   
✅ ns-3: 
   - EXPERIMENT_MATRIX_DESIGN.md 完成
   - Dry-run通过，无报错
   - Batch 1已启动或计划清晰
   
✅ Paper:
   - 4个RELATED_WORK_PART*.md 存在
   - 总字数 > 5000字
   - 29篇论文映射完成
```

---

**Status**: Week 2 Launch Ready ✅  
**Readiness**: Day 8启动 (pending Week 1 Day 5 decision)  
**Confidence**: HIGH (all dependencies clear, 3 agents ready)

---

**下一更新**: Day 5 里程碑通过后 → Day 8自动启动Week 2三向任务
