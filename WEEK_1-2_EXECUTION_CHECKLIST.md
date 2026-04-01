# ✅ Week 1-2 执行检查清单
## LoRa Mesh 优化计划 — Day 1-14 精确路线图

**启动日期**: 2026-03-29 (Day 1 上午)  
**前置完成**: ✅ ns-3修复 + Tamarin Triage  
**当前里程碑**: Week 1 Day 1 完成  

---

## 📋 Week 1 执行日程（Day 1-5）

### ✅ Day 1 (2026-03-29) — COMPLETE

- [x] **A轨道**: ns-3修复数据启动时间 t=1→t=120  
  - Commit: `206e9af` ✅
  - 修改: `lora-mesh-sim.cc` L283-344
  - 影响: 解除PDR=0%阻塞

- [x] **B轨道**: Tamarin问题分类
  - 文档: `TAMARIN_TRIAGE.md` ✅
  - 分析: 3个问题 → severity/effort/solution
  - 建议路线: 问题1(B方案) + 问题2(动态) + 问题3(A路线)

- [x] **C轨道**: Literature库完成
  - 文档: `LITERATURE_DISCOVERY_REPORT.md` ✅
  - 数量: 29篇论文 (目标20+)
  - 向量覆盖: 4/4完整

- [x] **提交**:
  - Commit 1: `206e9af` (ns-3 fix + Tamarin triage + literature库)
  - Status: ✅ 8 files committed

---

### ⏳ Day 2-3 (2026-03-30 to 2026-03-31) — ns-3验证

#### Day 2: 快速验证 (9场景子集)

**Task 2.1: 编译ns-3**
```bash
cd /home/augchao/Lora-Sec/phase4_ns3_simulation
# 验证修改后的代码可编译
# 预期: 无编译错误

时间预估: 15分钟
通过标准: 编译成功，无warning
```

**Task 2.2: 单场景测试**
```bash
# 最小可行测试: 一个拓扑、无攻击、1 seed
./docker-run-gpu.sh --scenarios "Linear" --attacks "None" --seeds 1 --duration 420

期望结果:
  - PDR > 80% (相比之前0%)
  - 路由表有非零entries
  - 数据包delivered > 0

时间预估: 10分钟运行 + 2分钟分析
通过标准: JSON结果中 pdr > 0.8
```

**Task 2.3: 三场景快速集合**
```bash
# 验证修复对所有拓扑有效
# Linear, Tree, Grid 各1场景，无攻击

期望结果:
  Linear: PDR > 85%
  Tree:   PDR > 75% (拓扑复杂)
  Grid:   PDR > 80%

时间预估: 30分钟
通过标准: 全3场景都有非零PDR
```

#### Day 3: 完整验证 (27场景重跑)

**Task 3.1: 启动全量重跑**
```bash
python3 phase4_ns3_simulation/scratch/lora-mesh-experiment-gpu-parallel.py \
  --topologies Linear Tree Grid \
  --attacks Spoofing Replay Selective \
  --patches 0 1 \
  --workers 10 \
  --duration 420

时间预估: 60-90分钟（10-worker并行）
输出: 27个JSON文件在 results/ 目录
```

**Task 3.2: 统计验证**
```bash
# 验证PDR数据的合理性
python3 -c "
import json, os
import statistics

results_dir = 'phase4_ns3_simulation/results'
pdr_values = []
for f in os.listdir(results_dir):
    if f.endswith('.json'):
        with open(os.path.join(results_dir, f)) as fp:
            data = json.load(fp)
            pdr_values.append(data.get('pdr', 0))

print(f'Total scenarios: {len(pdr_values)}')
print(f'PDR mean: {statistics.mean(pdr_values):.2%}')
print(f'PDR std: {statistics.stdev(pdr_values):.2%}')
print(f'PDR > 0: {sum(1 for p in pdr_values if p > 0)} scenarios')

# 验证标准
assert len(pdr_values) == 27, 'Missing scenarios'
assert sum(1 for p in pdr_values if p > 0) > 20, 'Most PDR must be > 0'
"

时间预估: 5分钟
通过标准: >20个场景PDR > 0
```

**Task 3.3: 数据备份与对比**
```bash
# 保存Day 3结果与原始27场景对比
mkdir -p results/day3_backup
cp results/*.json results/day3_backup/

# 生成对比报告
# 原始27场景: PDR全0%
# Day 3 27场景: PDR大部分 > 50%
# 证明修复有效

时间预估: 5分钟
```

#### Day 2-3 关键指标

| 检查点 | 成功标准 | 失败则 |
|--------|--------|--------|
| 单场景PDR | > 80% | 回到Day 1 debug时序 |
| 三拓扑集 | 全部PDR > 0% | 逐个诊断 |
| 27场景平均 | PDR > 50% | 验证修复的泛用性 |

---

### ⏳ Day 4-5 (2026-04-01 to 2026-04-02) — 里程碑决策

#### Day 4: 数据分析

**Task 4.1: 攻击效果验证**
```
对比:
  - Baseline无attack: PDR ≈ 85%
  - 各Attack: PDR < baseline (明显退化)
  - 预期退化: 20-50% (攻击有效)
```

**Task 4.2: Patch效果验证**
```
对比:
  - Baseline无patch: 易受attack
  - Patch=1: 攻击成功率接近0
  - 预期: patch有效阻止攻击
```

**Task 4.3: 生成分析报告**
```
文档: NS3_VERIFICATION_REPORT.md
内容:
  - 27场景的PDR分布（直方图）
  - 攻击效果对比表
  - Patch防护效果验证
  - 升级到360场景的可行性
```

#### Day 5: 里程碑决策 (15分钟)

**Go/No-Go 检查**:

| 检查项 | 成功 | 失败处理 |
|--------|------|--------|
| ns-3 PDR验证 | ✅ | 回Day 1-3 debug |
| Tamarin路线确认 | ✅ | Day 5 下午确认 |
| Literature库可用 | ✅ | （已完） |
| 工期评估 | ✅ | 调整Week 2-6计划 |

**决策会议内容** (15 min):

```
1. ns-3修复成功吗?
   ✅ YES → 继续Week 2-6 360场景计划
   
2. Tamarin工期？
   选择: 
   - A) 问题1(B方案) + 问题2(动态) + 问题3(A路线) = 6-8周 ✅推荐
   - B) 完整修1+2+3 = 10-12周
   - 决策影响: 整体投稿时间
   
3. 文献库足够？
   ✅ YES (29篇 > 20篇目标)
   
4. Week 2启动条件？
   - [ ] ns-3验证通过
   - [ ] Tamarin路线确定
   - [ ] Sisyphus-Junior代理分配确认
```

**输出**: `WEEK_1_DECISION.md` 文档 (签署Day 5)

---

## 📋 Week 2 执行计划（Day 8-14）

### 三向并行启动 (Day 8 上午)

#### 轨道 A: Tamarin 可执行性引理 (Sisyphus-Junior [deep])

**任务**: 添加executability lemmas验证模型非vacuous

```
Rule: Node_Init, Hello_Broadcast, Route_Update, Data_Forward
都应该能在模型中被触发

Lemmas:
  - L_Ex1: ∃路径使得Node初始化
  - L_Ex2: ∃路径使得Hello广播被接收
  - L_Ex3: ∃路径使得Route更新发生
  - L_Ex4: ∃路径使得Data转发成功
  
验证标准:
  - 每个lemma < 60s验证时间
  - 都应该VERIFIED (not FALSIFIED)
```

**工期**: W2 Day 1-2 (3天)  
**输出**: baseline + patched各加4个executability lemmas  
**验证**: Tamarin --prove <3 min

#### 轨道 B: ns-3 360场景因子矩阵设计 (Explore agent)

**任务**: 扩展实验矩阵从27→360

```
原矩阵: 3拓扑 × 3攻击 × 3 seeds = 27
新矩阵: 3拓扑 × 3攻击 × 2补丁 × 20 seeds = 360

设计要素:
  - 因子完整度验证
  - 种子随机性确保
  - GPU并行分批策略 (3批×120)
  - 结果收集脚本
```

**工期**: W2 Day 1-2 (2天)  
**输出**: 
  - `EXPERIMENT_MATRIX_DESIGN.md`
  - Updated Python runner配置
  - 验证: dry-run列出360场景

#### 轨道 C: Related Work写作骨架 (Sisyphus-Junior [artistry])

**任务**: 准备Week 3的Related Work章节

```
基于29篇论文，组织为:

Section 1: LoRaWAN安全背景 (向量1的论文)
  - 协议现状
  - 已知漏洞
  - CVE-2025-52464案例

Section 2: Mesh路由安全 (向量2的论文)
  - DV routing已知攻击
  - Sinkhole/blackhole/replay
  - 动态拓扑挑战

Section 3: 形式化验证方法论 (向量3的论文)
  - Tamarin适用性
  - 5G-AKA/AODV案例
  - 优化技术

Section 4: Breaking & Repairing范式 (向量4的论文)
  - 攻击发现工作流
  - 补丁验证方法
  - 论文定位对标
```

**工期**: W2 Day 3-4 (2天)  
**输出**: 4节骨架+每节的核心论文引用  
**验证**: 自评是否覆盖4个向量

### Week 2 里程碑检查（Day 14）

| 检查 | 成功标准 |
|-----|---------|
| Tamarin executability | 4个lemmas都verified |
| 360场景矩阵 | dry-run无报错 |
| Related Work骨架 | 4节完整 + 论文映射 |

---

## 🚀 关键日期与依赖

### 阻塞链

```
Day 1: ns-3修复 + Tamarin分类
  ↓
Day 2-3: ns-3验证 (阻塞360场景启动)
  ↓
Day 5: 里程碑决策 (Tamarin工期确认)
  ↓
Day 8: Week 2 三向并行启动
  ↓
Day 14: Week 2里程碑 (executability+矩阵+骨架)
  ↓
Day 15+: Week 3 Tamarin深化开始
```

### 并行机会

```
Day 1: 
  - A: ns-3修复 ∥ B: Tamarin分类 ∥ C: Literature (已完)
  
Day 2-3:
  - A: ns-3验证 ∥ B: Literature整理 ∥ C: Paper思考
  
Day 8-14:
  - A: Tamarin lemmas ∥ B: 360矩阵设计 ∥ C: Related Work骨架
```

---

## 📊 执行检查表

### Week 1 必须完成的

- [x] Day 1: ns-3修复 + Tamarin分类
- [ ] Day 2-3: ns-3验证 (9 + 27场景)
- [ ] Day 4: 数据分析报告
- [ ] Day 5: 里程碑决策 + Week 2启动准备

### Week 2 必须完成的

- [ ] Day 8-9: Tamarin executability lemmas (A轨道)
- [ ] Day 8-9: 360场景矩阵设计 (B轨道)
- [ ] Day 10-11: Related Work骨架 (C轨道)
- [ ] Day 12-14: 各轨道验证 + Week 3准备
- [ ] Day 14: Week 2里程碑检查

---

## 📞 后续指令

**Day 2-3等待验证结果时**:
```bash
# 如果ns-3仍有问题
git log --oneline -1  # 确认修改已commit
cd phase4_ns3_simulation
grep -n "dataStartSec" scratch/lora-mesh-sim.cc  # 验证修改存在
```

**Day 5决策前**:
```bash
# 确认Week 2可以启动
# 检查: WEEK_1_DECISION.md 是否签署
# 检查: Tamarin路线是否确定
```

**Day 8启动Week 2时**:
```bash
# 启动三个并行任务 (见下面的代理分工)
```

---

## 👥 代理分工（Week 2启动）

### 当Day 5里程碑通过后，立即启动:

```bash
# Sisyphus-Junior [deep] — Tamarin executability
task(category="deep", load_skills=[], ...)

# Explore agent — 360场景矩阵设计
task(subagent_type="explore", load_skills=[], ...)

# Sisyphus-Junior [artistry] — Related Work骨架
task(category="artistry", load_skills=[], ...)
```

详见 `OPTIMIZATION_MASTERPLAN.md` 的"代理分工"章节。

---

**状态**: Week 1 Day 1 完成，Day 2-5待执行  
**下一行动**: Day 2上午快速验证ns-3修复
