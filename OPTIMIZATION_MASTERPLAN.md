# 🎯 LoRa Mesh 安全研究优化总规划
## 从概念验证 → IEEE TDSC 投稿（14 周完整路线图）

**生成时间**: 2026-03-29  
**目标期刊**: IEEE TDSC（滚动提交，无deadline压力）  
**投稿目标日期**: Week 13-14（~2026-07-07）  
**作者**: Oracle + Metis（5LLM评估 + 深度规划）  

---

## 📋 Executive Summary

### 当前状态（Week 0）
| 维度 | 现状 | 目标（TDSC）| 缺口 |
|------|-----|----------|------|
| **Tamarin模型** | 87+94行, 5/5引理证（但浅） | 180+200行, 10+引理（含executability） | +100行 + 5引理 |
| **ns-3实验** | 27场景，PDR=0%（根本原因已识别） | 360场景，统计有效 | 修时序 + 13x场景 |
| **论文** | 8章560行，0图表，0参考文献 | 14页，7-8图表，20+参考文献 | 全面重写 + 整合 |
| **Evidence** | 无 | 完整Tamarin日志 + ns-3数据 + 6-8表格 | 生成所有证据 |

### 关键发现（Oracle + Metis）

**技术层**（Oracle评估）：
- 🔴 **问题1（PSK HMAC）**: CRITICAL — 当前只能证明 **group auth**，不能证明 **source auth**
  - **修复路线**: 签名方案（45-90分钟） > per-node密钥（60-120分钟） > pairwise MAC（60-180分钟） > 降级claim（30-35分钟）
  - **推荐**: 修复 + 明确threat model边界（总耗时6-8周）
- 🔴 **问题2（MetricVersion）**: CRITICAL — 当前freshness lemma 接近vacuous（版本是常数）
  - **修复路线**: 显式后继关系 + 版本状态 + 接收侧新旧比较规则
  - **耗时**: 45-90分钟（与问题1一起修）
- 🟡 **问题3（多跳无环）**: 条件性CRITICAL
  - 若声称loop-freedom → 需补多跳链、loop-prevention不变量 → 4-12小时
  - 若只声称"受限安全传播" → 可接受（仅限制claim范围）
  - **推荐**: 限制为3跳链模型 + 明确limitations section

**执行层**（Metis规划）：
- ✅ **ns-3根本原因已识别**: 数据流在t=1s启动，但路由收敛需60-100s → 修复: 改为t≥120s启动（~5行代码）
- ✅ **并行机会**: Tamarin深化 ∥ ns-3全因子 ∥ 论文骨架可3-4向并行
- ✅ **关键路径**: Tamarin(W1-6) + ns-3(W1-6) → 双路径W6汇聚 → 论文集成(W7-8) → 抛光(W9-10) → artifact(W11-13)

### 推荐路线（最高性价比）

| 方案 | 工期 | 成功率 | 推荐度 |
|------|-----|--------|--------|
| **修1+2，限3（推荐）** | 6-8周 | 75% TDSC | ⭐⭐⭐⭐⭐ |
| 修1+2+3（全修） | 10-12周 | 85% TDSC | ⭐⭐⭐ |
| 只降级claim（最快） | 4周 | 40% TDSC | ⭐ |

---

## 🛠️ 技术决策（Oracle建议）

### 决策1: 金钥假设（PSK HMAC）

**问题陈述**:
```
当前模型:
  rule DV_Broadcast_with_HMAC:
    let psk = PSK(node, '~psk')
        hmac = h(psk, m)
        
论文声称: "source authentication" 
实际性质: 仅能证明 "group authentication"（知道PSK的某个节点）
```

**Oracle判断**: **CRITICAL BUG**（若声称source auth），但**可修复**

**修复方案排序**（从强到弱）:

1. **签名方案（推荐）**
   - 机制: 每节点自己的签名密钥对路由更新签名；消息显式绑定`sender_id`
   - 优势: 干净、接近真正的source auth、最容易通过评审
   - 复杂度: Tamarin验证 45-90分钟（含compromise分支可能到2小时）
   - **推荐原因**: 复杂度可接受，结果最强

2. **Per-node唯一认证密钥 + 可验证身份映射**
   - 机制: 每节点独立MAC key（K_i），验证方已知"key_i属于节点_i"
   - 优势: 仍可证明source auth，较签名更简单
   - 复杂度: 60-120分钟（多一层key分配建模）
   - 缺点: Key分发机制需在论文中说清

3. **Pairwise / Per-link MAC**
   - 机制: A→B、B→C各独立认证，仅证明hop-by-hop而非端到端
   - 优势: 适合mesh拓扑
   - 缺点: 无法再宣称source authentication

4. **不改机制，只降级claim（最便宜）**
   - 改为"group authentication + integrity"，不说source
   - 耗时: 30-35分钟
   - 缺点: 论文贡献强度大幅下降

**选择**: **方案1（签名）或方案2（per-node key）**  
**工期**: 与问题2合并修复，总计6-8周

**投稿影响**:
- 不修 + 继续宣称source auth → 被大概率打回（明显的claims-model mismatch）
- 修成per-source + 明确说明 → TDSC可接受 ✅

---

### 决策2: MetricVersion 新鲜性

**问题陈述**:
```
当前模型:
  fact MetricVersion(node, ~v0)    // v0 is constant throughout
  
论文声称: "freshness: metric version递增阻止stale routes"
实际性质: 因为版本是常数，无法真正证明"版本单调递增"
```

**Oracle判断**: **CRITICAL（freshness/anti-stale是核心定理）**

**正确建模**:

```
1. 显式后继关系:
   MetricVersion(node, v) → MetricVersion(node, succ(v))
   
2. 路由消息携带: (origin, dest, metric, version)

3. 接收方维护: BestVersion(origin, dest, v)

4. 接受规则:
   - 新版本(v' > v) → 覆盖旧版本
   - 相同版本 → 按metric tie-break
   - 旧版本 → 拒绝

5. 关键约束: 
   - 版本单调增长
   - 已接受较新版本后，旧版本永不回装
```

**复杂度预测**:
- 局部单调后继 + 每目的地best-version状态 → 45-90分钟，可控
- 天真的全局版本比较 → 2-4小时，容易爆炸

**选择**: **显式单调版本状态**  
**工期**: 与问题1合并，W3日期修复完成

**投稿影响**:
- 不修 → freshness theorem被认为有vacuity风险 → 对formal verification评审伤害很大 ❌
- 修成显式单调版本 → 这是最值得投入的修复 ✅

---

### 决策3: 多跳无环（Loop-Freedom）

**问题陈述**:
```
当前模型: 原子rebroadcast，无显式per-node路由状态，无loop detection
论文可能声称: "路由收敛到无环拓扑"
```

**Oracle判断**: **条件性CRITICAL**
- 若声称loop-freedom → 必须补全 → 4-12小时
- 若只声称"受限抽象下的安全传播" → 可接受，仅限claim范围

**两条路线**:

#### A. 有限范围（推荐）
```
声称范围: "≤3跳链式路由的安全传播和metric新鲜性"
不声称: loop-freedom（无环收敛）

理由: 
- LoRa mesh通常<3跳（限制条件）
- 避免状态爆炸（per-hop state会触发O(n²)复杂度）
- 仍能说清attack + defense mechanism

limitations明确写: 
"Model assumes network diameter ≤3. Extension to arbitrary topologies 
requires sequence numbers (AODV-style) or rank-based ordering (RPL-style)."
```

#### B. 完整建模（如果要loop-freedom）
需要补充:
1. Per-node route state: `Route(dest, nextHop, metric, version/seq/rank)`
2. Send/receive/rebroadcast分步（不原子化）
3. Partial update state
4. Loop-prevention不变量（sequence number / rank / path-vector）

耗时: 4-12小时；验证时间可能从30分钟→多小时

**选择**: **路线A（有限范围）**  
**工期**: W3-4限定为3跳，写清limitations，不额外耗时

**投稿影响**:
- 路线A + 明确limitations → TDSC可接受 ✅
- 路线B（全loop-freedom）→ 可能过度承诺，验证爆炸 ❌

---

## 📊 14周执行计划（周粒度）

### Phase 0: 基础 + 分类（Week 1-2）

#### Week 1: Ground Truth Established

**关键目标**: 修ns-3时序 + Tamarin分类 + 验证可行性

| 日期 | 任务 | 交付物 | 验证标准 |
|------|------|--------|--------|
| **D1-D2** | **ns-3时序修复**: 数据流启动改为t=120s | `lora-mesh-sim.cc`修改 | 运行3个baseline场景，PDR > 80% |
| **D1-D2** | **Tamarin分类**: 逐行读取baseline+patched，分类3个问题 | `TAMARIN_TRIAGE.md` | 每个问题标记：Critical/Important/Nice-to-have + 估算工期 |
| **D3-D4** | **ns-3小规模验证**: 3场景×1seed子集 | 9个JSON结果文件 | baseline PDR ≥ 80%；attack scenarios有明显退化 |
| **D3-D4** | **Literature启动**: 用Crane搜索30+候选文献 | `references/`目录 ≥30候选 | 跨度：LoRa安全 + mesh形式化 + Tamarin IoT |
| **D5** | **里程碑决策**: ns-3/Tamarin go/no-go | 决策文档 | ns-3可修复；Tamarin问题有清晰分类 |

**原子提交**:
```
feat(ns3): start data traffic at t=120s (after route convergence)
docs(tamarin): classify 3 logical issues with severity/effort
test(ns3): validate 9-scenario subset produces non-zero PDR
refs(literature): initial 30 candidate papers via Crane search
```

---

#### Week 2: 并行轨迹启动

**关键目标**: Tamarin补lemma + ns-3设计全因子矩阵 + Literature筛选

| 日期 | 任务 | 交付物 | 验证标准 |
|------|------|--------|--------|
| **D1-D2** | **Tamarin执行性lemma**: baseline模型新增4个 | 更新baseline_lora_dv.spthy | 每个lemma验证<60s；都是executability类型 |
| **D1-D2** | **ns-3因子矩阵设计**: 完整360场景配置 | `EXPERIMENT_MATRIX.md` + Python runner | 配置文件可parse；dry-run列出360个场景 |
| **D3-D4** | **Tamarin理智性lemma**: patched模型新增3个 | 更新patched_lora_dv.spthy | Sanity lemmas验证；不产生意外限制 |
| **D3-D4** | **ns-3全因子启动（第1批）**: 120个场景并行 | GPU runner启动无报错 | 第一批120个场景开始运行 |
| **D5** | **Literature筛选**: 30候选 → 20入选 | Crane中的筛选决策 | 每个决策有include/exclude理由 |

**原子提交**:
```
feat(tamarin): add 4 executability lemmas to baseline model
feat(tamarin): add 3 sanity lemmas to patched model
feat(ns3): implement 360-scenario factorial matrix runner
data(ns3): factorial batch 1/3 (120 scenarios) completed
refs: screen 30 candidates → include 20
```

---

### Phase 1: 并行深化（Week 3-6）

#### Week 3: Tamarin深化 + 实验运行 + Literature注解

| 轨道 | 任务 | 交付物 | 验证标准 |
|-----|------|--------|--------|
| **Tamarin** | PSK-compromise边界：加明确规则 + 对应lemma | baseline/patched都有compromise rule | Lemma证：PSK泄露→完全网络攻陷 |
| **Tamarin** | MetricVersion新鲜性：加重放窗口建模 | 扩展`MetricVersion`规则 | 现有5 lemma仍验；新freshness lemma添加 |
| **ns-3** | 全因子第2批: 120-240场景运行 | 120个JSON结果 | 所有输出有非零指标 |
| **论文** | Related Work第一稿: 20篇文献 | 2-3页LaTeX | 每篇文献≥2句对比 |
| **论文** | 威胁模型第一稿: LoRa DV的Dolev-Yao适配 | 1-1.5页 | 覆盖3个攻击类别 |
| **Literature** | AI注解：20篇论文 | Crane中的annotations | 每篇：摘要、方法、关键贡献、相关性 |

**原子提交**:
```
feat(tamarin): model PSK-compromise boundary with explicit lemma
feat(tamarin): add replay-window to MetricVersion freshness
data(ns3): factorial batch 2/3 (120 scenarios) completed
paper: write Related Work (20 references)
paper: formalize threat model for LoRa Mesh DV routing
refs: complete AI annotations for all 20 references
```

---

#### Week 4: 多跳扩展 + 实验完成

| 轨道 | 任务 | 交付物 | 验证标准 |
|-----|------|--------|--------|
| **Tamarin** | 多跳链规则: ≥3跳Forward规则 | 扩展模型 | 多跳lemma验；模型≥150行 |
| **Tamarin** | Loop-detection属性（预期在baseline破裂，在patched验） | `loop_freedom` lemma | Baseline: falsified（expected）; Patched: behaviors明确记录 |
| **ns-3** | 全因子第3批: 241-360场景运行 | 360个完整JSON结果 | 全因子矩阵完成，20 seeds/配置 |
| **论文** | 协议描述第一稿 | 1.5-2页+协议流程图占位符 | 覆盖Hello格式、DV算法、metric传播 |

**原子提交**:
```
feat(tamarin): add multi-hop forwarding chain (≥3 hops)
feat(tamarin): add loop-freedom property lemma
data(ns3): factorial batch 3/3 (360/360 scenarios) completed
paper: write protocol description with DV algorithm detail
```

---

#### Week 5: 统计分析 + Tamarin最终化

| 轨道 | 任务 | 交付物 | 验证标准 |
|-----|------|--------|--------|
| **Tamarin** | 模型清理 + 完整proof运行 | 最终baseline/patched (≥180行各) | 所有lemma验；总验证<5分钟 |
| **ns-3** | 统计脚本：95% CI、p-value、效应量 | `analysis/stats.py` | 脚本可运行；输出格式符合预期 |
| **ns-3** | 生成结果表格 | 6-8张LaTeX表格 | 包含：均值、std、95% CI、p-value列 |
| **论文** | 形式化验证章节第一稿 | 2-3页+模型摘录+引理列表 | 包含：关键规则、所有引理陈述、验证统计 |

**原子提交**:
```
feat(tamarin): finalize models (baseline ≥180L, patched ≥200L)
feat(ns3): add statistical analysis with CI/p-value computation
data(ns3): generate 8 result tables from 360 scenarios
paper: write formal verification section with proof summary
```

---

#### Week 6: 证据完整 ← **关键里程碑**

| 轨道 | 任务 | 交付物 | 验证标准 |
|-----|------|--------|--------|
| **Tamarin** | 发布质量proof日志 | 时间戳日志，每lemma结果 | 日志可parse；所有lemma有结果 |
| **ns-3** | Patched vs Unpatched对比分析 | 对比表格 | Patched PDR > Baseline for all attacks |
| **论文** | 攻击章节：3个攻击类 | 2-3页，描述+Tamarin trace | 每个攻击：描述 + 形式trace + ns-3验证 |
| **论文** | 补丁设计章节 | 1.5-2页，HMAC+MetricVersion | 包含：设计理由、形式证明ref、开销分析 |
| **图表** | 3个核心图：(1)攻击分类树 (2)协议流程 (3)PDR对比 | 3个PDF/SVG | 在LaTeX中可渲染 |

**🚀 里程碑检查（Week 6末）**:
- ✅ Tamarin: 所有模型final，所有proof完成
- ✅ ns-3: 360实验done，统计完成
- ✅ 论文: 6/8章节草稿完成
- ✅ 证据: 每个claim都有支撑数据

**原子提交**:
```
data(tamarin): generate final proof logs for publication
analysis(ns3): complete patched-vs-unpatched comparison
paper: write attack discovery section (3 classes)
paper: write patch design section with overhead analysis
fig: generate attack taxonomy + protocol flow + PDR comparison
```

**分支合并**: 
```
main ← tamarin/deepening   (proof logs + final models)
main ← ns3/full-factorial  (all 360 results + stats tables)
main ← refs/complete       (all 20 papers annotated)
```

---

### Phase 2: 集成 + 论文写作（Week 7-10）

#### Week 7: Claims锁定

**目标**: 证据完整 → 声明确定 → 论文写作启动

| 任务 | 交付物 | 验证标准 |
|------|--------|--------|
| **Claims-Evidence映射** | `CLAIMS_EVIDENCE_MAP.md` | 每个claim → ≥1个Tamarin lemma OR ≥1个ns-3结果 |
| **重写Introduction** | 1.5页，"Breaking and Repairing LoRa Mesh Security" | 4-contribution列表对应证据 |
| **重写Abstract** | 250词，具体数字 | PDR%, 攻击成功率%, 开销% 均可追溯 |
| **生成剩余图表** | 4-5个高质量图：拓扑、攻击trace、开销、收敛时间 | 所有图在文本中引用 |

**原子提交**:
```
docs: create claims-evidence mapping table
paper: rewrite introduction with 'breaking and repairing' framing
paper: rewrite abstract with concrete evidence numbers
fig: generate topology + attack trace + overhead + convergence figures
```

---

#### Week 8: 完整初稿v2

| 任务 | 交付物 | 验证标准 |
|------|--------|--------|
| **Evaluation章节** | 2-3页，融合ns-3结果+成本分析 | 引用所有8张表格 |
| **Discussion & Limitations** | 1页，PSK限制/模型边界/可扩展性 | 回应3个已知限制 |
| **Conclusion** | 0.5页，总结贡献+未来工作 | 4个贡献与Introduction对应 |
| **剩余表格** | 2-3张：与相关工作对比、引理摘要、配置摘要 | 所有表有标题、文中引用 |
| **编译完整稿** | DRAFT_v2.md（所有章节已填） | LaTeX编译成功；无broken ref |

**🚀 里程碑（Week 8末）**: **完整初稿v2 — 所有章节填充，所有证据整合**

**原子提交**:
```
paper: write evaluation section (all tables integrated)
paper: write discussion & limitations
paper: write conclusion
table: generate related work comparison + lemma summary + configuration
paper: compile complete draft v2 (all sections filled)
```

**分支合并**: `main ← paper/draft-v2`

---

#### Week 9: 内部评审第1轮

| 任务 | 交付物 | 验证标准 |
|------|--------|--------|
| **TDSC标准自评** | Review report | 用`crane_evaluate_q1_standards` |
| **数据一致性修复** | 所有数字更正 | 无`{{PLACEHOLDER}}`遗留 |
| **Framing修复** | claim紧缩，删overclaim | 无"first ever"/"completely secure"非限定 |
| **交叉引用检查** | 所有图表方程引用；所有ref引用 | LaTeX自动check pass |

---

#### Week 10: 内部评审第2轮

| 任务 | 交付物 | 验证标准 |
|------|--------|--------|
| **写作质量pass** | 学术语调、清晰度、逻辑流 | 运行`crane_review_paper_sections` (review_type=writing) |
| **图表质量pass** | 发表质量格式 | 一致字体；打印size可读 |
| **编译最终稿** | DRAFT_v3 (final) | LaTeX clean compilation; 14页±0.5 |

---

### Phase 3: Artifact + 投稿（Week 11-14）

#### Week 11: Artifact包装

| 轨道 | 任务 | 交付物 | 验证标准 |
|-----|------|--------|--------|
| **Tamarin** | 清理模型+proof脚本+README | `artifact/tamarin/` | `./verify.sh`重现所有proof < 10min |
| **ns-3** | 仿真代码+Docker+运行脚本 | `artifact/ns3/` with Dockerfile | `docker build && docker run`重现9场景子集 |
| **数据** | 360结果文件+分析脚本 | `artifact/data/`+`analyze.py` | `python analyze.py`重现所有表格 |

**原子提交**:
```
artifact(tamarin): package models + proof scripts + README
artifact(ns3): package simulation code + Docker + run scripts
artifact(data): package all 360 results + analysis scripts
```

---

#### Week 12: Artifact验证

| 任务 | 交付物 | 验证标准 |
|------|--------|--------|
| **干净室测试** | 构建log | 在fresh machine上，3个artifact都工作 |
| **Artifact README** | `artifact/README.md` | 按IEEE evaluation guidelines |
| **论文最终抛光** | DRAFT_v4 (投稿版) | 预防所有typical reviewer objection |

---

#### Week 13: 投稿前检查

| 任务 | 交付物 | 验证标准 |
|------|--------|--------|
| **运行`crane_run_submission_check`** | 完整投稿健康报告 | Score ≥ 80/100 on Q1 standards |
| **修复任何critical问题** | 最终修正 | 所有critical issues resolved |
| **准备投稿包** | PDF + supplementary + artifact link | 符合IEEE TDSC template |

---

#### Week 14: 投稿

| 任务 | 输出 |
|------|------|
| **投稿至IEEE TDSC** | 确认邮件 |
| **存档投稿版本** | Git tag: `tdsc-submission-v1` |
| **CCS 2027评估** | 是否追求dual-track的决策文档 |

---

## 🔄 并行执行策略

```
周数:   1    2    3    4    5    6    7    8    9   10   11   12   13   14
      ├────┤    ├─────────────────┤    ├─────────────┤    ├─────────┤

Tamarin:[FIX ][EXEC][DEEP][MULTI][FINAL][LOGS]
        ████  ████  ████  ████   ████   ██

ns-3:   [FIX ][MAT ][B1/3][B2/3][B3/3][STAT][COMP]
        ████  ████  ████  ████  ████  ████  ██

论文:   [    ][    ][RW  ][PROT][FORM][ATK ][INTRO][FULL][REV1][REV2]
                    ████  ████  ████  ████  ████  ████  ████  ████

Literature:[SRCH][SCRN][ANNO][    ][    ][    ]
          ████  ████  ████

图表:   [    ][    ][    ][    ][    ][FIG3][FIG5][DONE]
                                     ████  ████

Artifact:[    ][    ][    ][    ][    ][    ][    ][    ][    ][    ][TAM][NS3][TEST][SUBM]
                                                                    ████ ████ ████  ████
```

**最大并行点**:
- **Week 3-5**: Tamarin深化 ∥ ns-3批处理 ∥ Literature注解 ∥ 论文写作（4向并行）
- **Week 7-8**: 论文集成 ∥ 图表生成（不同agent）
- **Week 11-12**: 3条artifact轨道并行（Tamarin、ns-3、Data）

**序列化约束**:
```
ns-3修复(W1) → 全因子(W2-5) → 统计(W5-6) → 结果表格(W6-7)
Tamarin分类(W1) → 深化(W2-5) → proof日志(W6) → claims(W7)
                                               ↓
                                  Claims锁定(W7) → 完整稿(W8) → 评审(W9-10) → Artifact(W11-12) → 投稿(W13-14)
```

---

## 🧪 TDD框架（必须遵循）

### Tamarin TDD协议

```python
FOR EACH new property/enhancement:
  1. 先写lemma陈述（"测试"）
  2. 预测baseline/patched的期望结果（verify/falsify）
  3. 在baseline上运行Tamarin → 确认期望
  4. 在patched上运行Tamarin → 确认期望
  5. 若不符：分析trace → fix模型 OR 修改预期
  6. 仅当两个模型都符合预期时commit
```

### ns-3 TDD协议

```python
FOR EACH experiment batch:
  1. 定义期望范围（FIRST）:
     - Baseline无attack: PDR ∈ [85%, 99%], 延迟 < 5s
     - Attack场景: PDR退化 ∈ [20%, 80%] 取决于攻击
     - Patched场景: PDR恢复 ∈ [80%, 98%]
  2. 运行小规模验证（3场景，1 seed）
  3. 验证结果在期望范围内
  4. 若超出 → debug模拟逻辑
  5. 仅THEN启动全批
  6. 批后：统计验证（CI宽<10%, 关键对比p<0.05）
```

### 论文TDD协议

```python
FOR EACH section:
  1. 先写claim/贡献（第一步）
  2. 映射每个claim到具体证据（Tamarin引理# OR ns-3表#）
  3. 写该章节
  4. 验证：每个claim都有证据reference
  5. 验证：无unsupported claim
  6. 运行crane_review_paper_sections on section
  7. fix issues后再进下一章节
```

---

## 💾 原子提交规范

### 命名规则
```
<type>(<scope>): <description>

Types: feat, fix, data, paper, fig, table, refs, docs, test, artifact
Scopes: tamarin, ns3, paper, refs, artifact

示例:
  feat(tamarin): add 4 executability lemmas to baseline model
  fix(ns3): start data traffic at t=120s after route convergence
  data(ns3): factorial batch 1/3 (120 scenarios) completed
  paper: write related work with 20 references
  fig: generate PDR comparison bar chart
```

### 提交规则
1. **一个逻辑变化/提交** — 绝不混合Tamarin+ns-3+论文
2. **每个提交都通过验证** — Tamarin模型verify、ns-3编译、LaTeX编译
3. **数据提交包含验证** — 提交消息说明预期vs实际结果
4. **论文提交引用证据** — 提交消息说明支撑哪些claim
5. **里程碑标记tag**: `phase0-complete`, `evidence-locked`, `draft-v2`, `draft-v3-final`, `tdsc-submission-v1`

### 分支策略
```
main
├── tamarin/deepening     (W2-6, W6末合并)
├── ns3/full-factorial    (W2-6, W6末合并)
├── paper/draft-v2        (W3-8, W8末合并)
├── paper/polish          (W9-10, W10末合并)
└── artifact/package      (W11-13, W13末合并)
```

---

## 👥 代理分工（Sisyphus-Junior instances）

### Sisyphus-Junior [deep] — 形式化验证轨道
**负责**: Tamarin模型深化（W2-6）
- executability + sanity lemmas (W2)
- PSK-compromise边界建模 (W3)
- MetricVersion新鲜性强化 (W3)
- 多跳链 + loop-freedom (W4)
- 最终清理 + proof日志 (W5-6)

**为什么[deep]**: 需要持续逻辑推理、10+引理的模型一致性

**Session继续**: 如果某轮修复失败，用同一session_id重启 + `prompt="Fix: [specific issue]"`

---

### Sisyphus-Junior [ultrabrain] — 统计分析 + Claims集成轨道
**负责**: ns-3分析 + claims映射（W5-8）
- 统计分析脚本 (W5)
- Patched vs Unpatched对比 (W6)
- Claims-Evidence映射 (W7)
- 论文数据一致性验证 (W8-9)

**为什么[ultrabrain]**: 跨形式化证明+实验数据的高精度推理

---

### Sisyphus-Junior [artistry] — 论文写作 + 图表轨道
**负责**: 论文散文 + 可视化（W3-10）
- Related Work写作 (W3)
- Protocol Description (W4)
- 所有图表生成 (W6-7)
- Introduction/Abstract重写 (W7)
- 写作质量抛光 (W9-10)

**为什么[artistry]**: 学术写作质量、图表美观、叙事流畅

---

### Librarian — 文献研究
**负责**: 文献发现+注解（W1-4）
- 论文搜索 (W1)
- 筛选 (W2)
- 深度注解 (W3-4)

**自终止**: W4完成后资源释放

---

### Explore — 代码模式发现（按需）
**负责**: 特定调查
- ns-3时序修复验证 (W1)
- Tamarin模型一致性检查 (W2)
- Artifact可重现性测试 (W11-12)

---

## ⚠️ 风险登记 + Mitigation

| # | 风险 | 概率 | 影响 | Mitigation | 触发点 |
|---|------|------|------|-----------|--------|
| **R1** | PSK问题无法干净建模 | 25% | HIGH | 在Limitations中明确为threat model边界；加"Future Work: PKI密钥分发" | W3末无解 |
| **R2** | ns-3 PDR仍为0%（修复失败） | 10% | **CRITICAL** | 从单节点对开始debug；逐步追踪包生成→路由→转发 | W1验证失败 |
| **R3** | 全因子耗时>3周 | 15% | MEDIUM | seed减20→10（仍统计有效，180场景）；用云GPU | W3末第1批不完 |
| **R4** | Tamarin验证>5分钟 | 20% | MEDIUM | 用`--heuristic=O`；分割为sub-theories；加helper lemmas | 若lemma>5分钟 |
| **R5** | 多跳无环在Tamarin无法判定 | 30% | MEDIUM | 限为3跳链（LoRa mesh足够）；明确为"bounded verification" | W4 loop lemma不终止 |
| **R6** | Patched ns-3无improvement vs baseline | 15% | HIGH | 先验证attack确实生效（check attack成功率）；重调参数 | W6对比无差异 |
| **R7** | 论文超14页TDSC限制 | 40% | LOW | 详细证明移到supplementary；Related Work精简；小号图表 | Draft-v2 > 16页 |
| **R8** | 评审要求硬件验证 | 50% | LOW (post-submit) | 回应："硬件验证列为future work；形式化证明提供比实验更强保证" | rebuttal阶段 |

### 降级阶梯（如果工期滑延）

| 滑延 | 动作 | 删减 | 保留 |
|------|------|------|------|
| +2周 | seed 20→10 | 统计power | 所有attack/defense组合 |
| +4周 | drop loop-freedom | 1个Tamarin属性 | 5个核心引理 + executability |
| +6周 | drop多跳扩展 | Tamarin ~120行 | 所有实验 + 核心验证 |
| +8周 | pivot工作坊论文 | TDSC → ACM WiSec/IEEE LCN | 核心"Breaking"贡献 |

---

## 🎯 投稿准备检查单

### 必须通过的验证（Week 13）

**Tamarin**:
```bash
tamarin-prover --prove baseline_lora_dv.spthy 
  # 期望: 4个lemma verified, 1个falsified（攻击） → 可parse from log

tamarin-prover --prove patched_lora_dv.spthy
  # 期望: 全5个lemma verified → 可parse from log
```

**ns-3**:
```bash
python3 lora-mesh-experiment-gpu-parallel.py --scenarios 9 --validate
  # 期望: baseline_no_attack PDR > 0.80
  # 期望: attack_spoofing PDR < baseline_no_attack
  # 期望: attack_patched_spoofing PDR ≈ baseline_no_attack
```

**统计**:
```bash
python3 analysis/stats.py --input results/ --output tables/
  # 期望: 所有CI宽 < 0.10
  # 期望: 关键对比p-value < 0.05
```

**论文编译**:
```bash
pdflatex main.tex && bibtex main && pdflatex main.tex && pdflatex main.tex
  # 期望: exit 0, 0 undefined references warnings
```

**Artifact**:
```bash
cd artifact && docker build -t lora-sec . && docker run lora-sec ./verify-all.sh
  # 期望: 完成 < 30分钟，所有checks pass
```

**Claims一致性**:
```bash
python3 scripts/check_claims.py --map CLAIMS_EVIDENCE_MAP.md --paper main.tex
  # 期望: 0 unsupported claims
```

---

## 📋 用户确认问题（必须回答）

在Week 1启动前，请确认：

### 必须回答（blocking）

**Q1**: PSK golden-key、MetricVersion新鲜性、多跳无环的"3个问题"是否与本规划一致？
- 还是我识别的问题有误/遗漏？

**Q2**: 14周工期能否接受（投稿目标~2026-07-07）？
- 还是需要压缩/拉长？

**Q3**: 可以同时运行多少个Sisyphus-Junior实例？
- 我的规划假设3个[deep]/[ultrabrain]/[artistry]并行

### 应该回答（重要）

**Q4**: 硬件验证是否完全out of scope？
- 还是想在artifact中加6-10节点ESP32 testbed？

**Q5**: "Breaking and Repairing"的强调重点？
- (a) 形式化方法论（Tamarin-centric，攻击是副产品）
- (b) 实际影响（攻击是头条，形式化是严谨）
- → 答案影响论文narrative结构

**Q6**: Threat model边界？
- 现在假设Dolev-Yao（网络层攻击者）
- 是否也考虑物理层（jamming、wormhole）？
- 还是明确排除？

---

## 📊 关键指标跟踪

| 指标 | 当前 | W6目标 | W14目标（TDSC） | 备注 |
|------|-----|--------|---------------|------|
| **论文页数** | 8 | 10-12 | 14 ± 0.5 | IEEE TDSC双列 |
| **图表数** | 0F + 8T | 3F + 8T | 7-8F + 8T | F=figure, T=table |
| **Related Work** | 0篇 | 15篇候选 | 20篇final | 品质>数量 |
| **Tamarin行数** | 87 + 94 | 150+180 | 180+200 | 包括lemmas |
| **Tamarin引理** | 5个（简） | 9个（含executability） | 10+个（含sanity）| 可证明性 |
| **ns-3场景** | 27 | 180 | 360 | 完整因子矩阵 |
| **ns-3统计** | 无 | 初步CI | 95% CI + p-value | 学位论文级别 |
| **发表就绪度(TDSC)** | 3.5/10 | 6/10 | 8-9/10 | 自评 |

---

## 🚀 立即行动清单（Week 1）

### Day 1-2（并行）
- [ ] **ns-3修复**: `lora-mesh-sim.cc`改数据启动时间t=120s
- [ ] **Tamarin分类**: 逐行读baseline/patched，标记3个问题严重度
- [ ] 提交: `feat(ns3): start data traffic at t=120s`
- [ ] 提交: `docs(tamarin): classify 3 issues with severity`

### Day 3-4（并行）
- [ ] **ns-3验证**: 3场景×1seed，确认PDR > 0%
- [ ] **Literature**: Crane搜索30+候选，覆盖3个维度
- [ ] 提交: `test(ns3): validate baseline produces non-zero PDR`
- [ ] 提交: `refs(lit): initial 30 candidates`

### Day 5
- [ ] **里程碑决策**: ns-3/Tamarin go/no-go → 决策文档
- [ ] **工期锁定**: TDSC rolling submit确认
- [ ] **代理分工**: 确认Sisyphus-Junior实例数量

---

## 总结

**这是一份经Oracle技术审查 + Metis战略规划的完整优化方案。**

- **关键决策**: 修复Tamarin问题1+2（6-8周），限制问题3为3跳链 → 最高性价比
- **关键路径**: W1修ns-3/分Tamarin → W2-6双轨并行 → W6汇聚 → W7-8论文集成 → W9-10抛光 → W11-13artifact → W14投稿
- **成功概率**: TDSC 75%+（若按计划执行）
- **时间弹性**: 降级阶梯，最坏+8周可降级到工作坊论文

**准备好了吗？开始Week 1吧。** 🚀
