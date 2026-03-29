# Tamarin模型结构与保真度分析报告

**分析日期**: 2026-03-29  
**分析对象**: LoRa Mesh安全形式化验证 - Week 1 Triage决策支持  
**模型版本**: Baseline (87行) vs Patched (94行)  

---

## 执行摘要

**当前状态**: 两个Tamarin模型已完成基础实现，但距离深化目标（180-200行/模型）仍有显著差距。

**关键发现**:
- ✅ 核心DV路由规则已建模（5-6个规则）
- ✅ 基础安全属性已定义（5个引理）
- ⚠️ **缺少executability/sanity lemmas**（Week 2关键任务）
- ⚠️ **PSK建模过于简化**（Week 3边界修正需求）
- ⚠️ **MetricVersion为常量**（Week 3新鲜性问题）
- ❌ **无多跳链规则**（Week 4核心缺失）
- ❌ **无loop-freedom属性**（Week 4关键属性）

**保真度评分**: **6.5/10** （基础功能完整，但抽象级别过高）

**Triage建议**: Week 2-3为**Critical Path**，Week 4需要**重大架构扩展**

---

## 任务1: 规则覆盖度分析

### 1.1 规则清单（按类别分组）

#### Baseline模型 (baseline_lora_dv.spthy)
```
Initialization:
  - Node_Init (L10-18): 节点初始化 + PSK分发

Send/Broadcast:
  - Hello_Broadcast (L21-24): 无认证Hello广播

Receive/Update:
  - Route_Update (L27-30): 无验证路由更新

Forward:
  - Data_Forward (L33-36): 简单数据转发

Compromise:
  - Compromise_Node (L39-42): 节点妥协（泄露PSK+路由表）
```

#### Patched模型 (patched_lora_dv.spthy)
```
Initialization:
  - Node_Init (L12-21): 节点初始化 + PSK分发 + MetricVersion初始化

Send/Broadcast:
  - Hello_Broadcast_Auth (L23-26): HMAC认证Hello广播

Receive/Update:
  - Route_Update_Verify (L28-37): HMAC验证路由更新

MetricVersion管理:
  - MetricVersion_Increment (L39-42): 版本递增（但实际为常量）

Forward:
  - Data_Forward (L44-47): 简单数据转发

Compromise:
  - Compromise_Node (L49-52): 节点妥协
```

### 1.2 覆盖度评估

| 类别 | Baseline | Patched | 深化需求 | 状态 |
|------|----------|---------|----------|------|
| **Initialization** | ✓ 基础 | ✓ 基础 | +Topology建模 | 需扩展 |
| **Send** | ✓ 原子化 | ✓ 原子化 | +Per-hop转发 | **原子化问题** |
| **Receive** | ✓ 原子化 | ✓ 原子化 | +Multi-step验证 | **原子化问题** |
| **Rebroadcast** | ❌ 缺失 | ❌ 缺失 | +Hop-by-hop规则 | **Critical缺失** |
| **Cleanup** | ❌ 缺失 | ❌ 缺失 | +Route aging | 需添加 |

**关键问题**: 当前Send/Receive是**原子化建模**，缺少per-hop转发的分步建模。这将影响多跳链规则和loop-freedom属性的建模。

---

## 任务2: 引理充分性分析

### 2.1 引理清单与分类

#### Baseline模型引理
```
L1_RouteMetricAuthenticity_Baseline (L46-51):
  类别: Authentication
  陈述: 路由接受 → 源节点曾发送Hello
  状态: BROKEN (预期)

L2_RouteFreshness_Baseline (L53-57):
  类别: Freshness  
  陈述: 路由接受 → 源节点曾发送对应序列号
  状态: BROKEN (预期)

L3_RouteConsistency_Baseline (L59-66):
  类别: Consistency
  陈述: 同序列号 → 相同metric值
  状态: BROKEN (预期)

L4_NoUnauthorizedRoute_Baseline (L68-73):
  类别: Authorization
  陈述: 路由中继节点必须是诚实节点
  状态: BROKEN (预期)

L5_PSKConfidentiality_Baseline (L75-79):
  类别: Secrecy
  陈述: 未妥协节点的PSK保持机密
  状态: PROVED

Compromise_Reveals_PSK (L81-85):
  类别: Sanity
  陈述: 妥协节点的PSK被泄露
  状态: FALSIFIED (预期)
```

#### Patched模型引理
```
L1_RouteMetricAuthenticity_Patched (L54-60):
  类别: Authentication
  陈述: 路由接受 → 源节点曾发送Hello (增加未妥协条件)
  状态: PROVED (目标)

L2_RouteFreshness_Patched (L62-68):
  类别: Freshness
  陈述: 同节点同序列号 → 相同MetricVersion
  状态: PROVED (目标)

L3_RouteConsistency_Patched (L70-78):
  类别: Consistency  
  陈述: 同序列号同版本 → 相同metric值
  状态: PROVED (目标)

L4_NoUnauthorizedRoute_Patched (L80-86):
  类别: Authorization
  陈述: 路由中继节点必须是诚实节点
  状态: PROVED (目标)

L5_PSKConfidentiality_Patched (L88-92):
  类别: Secrecy
  陈述: 未妥协节点的PSK保持机密
  状态: PROVED (目标)
```

### 2.2 引理覆盖矩阵

| 属性类别 | 当前覆盖 | 缺失引理 | Week 2-4需求 |
|----------|----------|----------|--------------|
| **Secrecy** | ✓ PSK机密性 | - | 完整 |
| **Authentication** | ✓ 路由认证 | - | 完整 |
| **Freshness** | ✓ MetricVersion | - | 完整 |
| **Loop-freedom** | ❌ 无 | +Loop detection | **Week 4 Critical** |
| **Executability** | ❌ 无 | +Non-vacuous proof | **Week 2 Critical** |
| **Sanity** | ⚠️ 仅Compromise | +Reachability | **Week 2 Critical** |

**关键缺失**:
1. **Executability lemmas**: 证明模型非空（存在有效执行路径）
2. **Sanity lemmas**: 证明基础可达性（节点能建立路由）
3. **Loop-freedom lemmas**: 证明路由无环（多跳场景）

---

## 任务3: PSK建模分析

### 3.1 PSK定义方式

**Baseline模型** (L12, L15):
```tamarin
rule Node_Init:
  [ Fr(~id), Fr(~psk), Fr(~seq) ]
  --[ NodeInit(~id), HonestNode(~id), PSKInit(~id,~psk) ]->
  [ !PSK(~id,~psk), ... ]
```

**Patched模型** (L14, L17):
```tamarin
rule Node_Init:
  [ Fr(~id), Fr(~psk), Fr(~seq) ]  
  --[ NodeInit(~id), HonestNode(~id), PSKInit(~id,~psk) ]->
  [ !PSK(~id,~psk), ... ]
```

### 3.2 PSK使用方式

**HMAC生成** (patched L26):
```tamarin
Out(<n,'zero',seq,n,'v0',hmac(<n,'zero',seq,n,'v0'>,psk)>)
```

**HMAC验证** (patched L29):
```tamarin
In(<src,metric,seq_new,nextHop,'v0',hmac(<src,metric,seq_new,nextHop,'v0'>,psk_src)>)
```

### 3.3 金钥共享范围分析

**当前建模**: **Per-node PSK** (每节点独立PSK)
- 每个节点有唯一PSK: `!PSK(~id,~psk)`
- HMAC验证需要知道源节点PSK: `!PSK(src,psk_src)`

**问题**: 这意味着**每个节点需要知道所有其他节点的PSK**，这在实际部署中不现实。

### 3.4 Source Authentication能力

**当前状态**: ✅ **可以证明source authentication**
- HMAC绑定源节点身份: `hmac(<src,...>,psk_src)`
- 只有拥有`psk_src`的节点能生成有效HMAC
- 验证节点通过`!PSK(src,psk_src)`检查

**严重程度**: ⚠️ **MEDIUM** - 功能正确但部署模型不现实

**具体问题位置**:
- **L33**: `!PSK(src,psk_src)` - 假设验证节点知道源节点PSK
- **L29**: HMAC验证逻辑 - 需要预共享所有节点PSK

**Week 3修正建议**: 改为**Shared broadcast PSK**或**PKI模型**

---

## 任务4: MetricVersion建模分析

### 4.1 MetricVersion定义

**初始化** (patched L19):
```tamarin
!MetricVersion(~id,~id,'v0')
```

**使用** (patched L24, L34):
```tamarin
!MetricVersion(n,n,'v0')    // Hello发送
!MetricVersion(src,src,'v0') // 路由验证
```

### 4.2 版本更新规则

**MetricVersion_Increment规则** (patched L39-42):
```tamarin
rule MetricVersion_Increment:
  [ !MetricVersion(n,n,'v0') ]
  --[ MetricVersionIncrement(n,'v0') ]->
  [ !MetricVersion(n,n,'v0') ]
```

**关键问题**: 输入和输出都是`'v0'` - **这是常量，不是真实递增！**

### 4.3 版本比较方式

**当前**: 无版本比较逻辑
- L2引理检查`ver1 = ver2`（相等性）
- 缺少`ver_new > ver_old`（单调性）

### 4.4 Monotonic Versioning支持

**当前状态**: ❌ **不支持真实动态版本**

**问题**:
1. 版本值硬编码为`'v0'`
2. 无递增逻辑（`'v0' → 'v1' → 'v2'`）
3. 无版本比较函数

**严重程度**: 🔴 **HIGH** - L2_RouteFreshness引理的核心假设失效

**Week 3修正需求**:
```tamarin
// 需要添加
functions: version_increment/1, version_compare/2

rule MetricVersion_Increment:
  [ !MetricVersion(n,n,ver_old) ]
  --[ MetricVersionIncrement(n,ver_old) ]->
  [ !MetricVersion(n,n,version_increment(ver_old)) ]
```

---

## 任务5: 多跳/拓扑建模分析

### 5.1 Neighbor/Topology建模

**当前建模** (baseline L17, patched L20):
```tamarin
!Neighbor(~id,~id)  // 每个节点是自己的邻居
```

**问题**: 
- 无真实拓扑建模
- 无邻居发现机制
- 无链路状态管理

### 5.2 Rebroadcast建模

**当前状态**: ❌ **完全缺失**

**缺失规则**:
```tamarin
// 需要添加
rule Hello_Rebroadcast:
  [ In(<src,metric,seq,nextHop,ver,hmac_tag>),
    !Node(relay,'honest'),
    !Neighbor(relay,nextHop) ]
  --[ Rebroadcast(relay,src,metric+1,seq) ]->
  [ Out(<src,metric+1,seq,relay,ver,hmac(...)>) ]
```

### 5.3 Per-node Route State

**当前状态**: ✅ **有基础路由表**
```tamarin
!RouteTable(dst,src,metric,nextHop,seq,ver)
```

**但缺少**:
- 路由表更新逻辑（Bellman-Ford）
- 路由老化机制
- 路由环检测

### 5.4 Loop-freedom相关规则

**当前状态**: ❌ **完全缺失**

**需要添加**:
1. 路径跟踪机制
2. 环检测规则
3. Loop-freedom引理

### 5.5 抽象级别评估

**当前抽象级别**: **高度抽象**
- Send/Receive是原子操作
- 无中间转发步骤
- 无真实多跳路径建模

**局限性**:
- 无法分析hop-by-hop攻击
- 无法验证多跳一致性
- 无法证明loop-freedom

**Week 4扩展需求**: 需要**重大架构变更**，从原子化转向分步建模

---

## 模型保真度评分

### 评分标准 (1-10分)

| 维度 | 权重 | Baseline | Patched | 评分依据 |
|------|------|----------|---------|----------|
| **协议覆盖度** | 25% | 5/10 | 6/10 | 基础DV规则完整，但缺少多跳 |
| **安全属性完整性** | 25% | 6/10 | 7/10 | 5个核心引理，但缺少executability |
| **建模真实性** | 20% | 5/10 | 5/10 | PSK模型不现实，MetricVersion为常量 |
| **可扩展性** | 15% | 6/10 | 7/10 | 架构支持扩展，但需重构 |
| **验证完整性** | 15% | 7/10 | 8/10 | 基础验证完成，结果符合预期 |

**综合评分**: **6.5/10**

### 评分理由

**优势**:
- ✅ 核心DV路由逻辑正确
- ✅ HMAC认证机制建模准确
- ✅ 基础安全属性定义完整
- ✅ 验证结果符合预期（baseline破坏，patched修复）

**不足**:
- ⚠️ 抽象级别过高（原子化操作）
- ⚠️ PSK分发模型不现实
- ⚠️ MetricVersion为常量，非动态
- ❌ 缺少多跳链建模
- ❌ 缺少loop-freedom属性
- ❌ 缺少executability验证

---

## Week 1 Triage建议

### Critical Path分析

```
Week 2 (Critical): Executability + Sanity
├── 添加executability lemmas (证明模型非空)
├── 添加sanity lemmas (证明基础可达性)  
└── 预计工期: 3-4天

Week 3 (Critical): PSK边界 + MetricVersion修正
├── 修正PSK共享模型 (shared broadcast PSK)
├── 实现真实MetricVersion递增
├── 添加版本比较函数
└── 预计工期: 4-5天

Week 4 (Major): 多跳链 + Loop-freedom
├── 重构为分步建模 (非原子化)
├── 添加hop-by-hop转发规则
├── 实现loop-freedom属性
└── 预计工期: 6-7天 (架构变更)
```

### 问题优先级

| 问题 | 严重程度 | 影响 | Week | 工期估算 |
|------|----------|------|------|----------|
| **缺少executability lemmas** | 🔴 HIGH | 模型可能vacuous | 2 | 2天 |
| **MetricVersion为常量** | 🔴 HIGH | L2引理失效 | 3 | 2天 |
| **PSK分发不现实** | 🟡 MEDIUM | 部署模型问题 | 3 | 2天 |
| **缺少多跳建模** | 🔴 HIGH | 无法分析hop攻击 | 4 | 4天 |
| **缺少loop-freedom** | 🔴 HIGH | 核心安全属性缺失 | 4 | 3天 |

### 工期评估

**总工期**: 13-16天 (2.6-3.2周)

**关键路径**:
1. Week 2: 基础完整性验证 (3-4天)
2. Week 3: 核心机制修正 (4-5天) 
3. Week 4: 架构扩展 (6-7天)

**风险因素**:
- Week 4的架构变更可能触发连锁修改
- Tamarin验证时间可能随模型复杂度指数增长
- 多跳建模可能导致状态空间爆炸

### 与OPTIMIZATION_MASTERPLAN对齐

**3个核心问题对应**:

1. **"模型抽象级别是否合适？"**
   - 当前: 过度抽象（原子化）
   - 建议: Week 4降低抽象级别，支持多跳分析

2. **"安全属性是否完整？"**
   - 当前: 基础属性完整，但缺少executability和loop-freedom
   - 建议: Week 2-3补全缺失属性

3. **"验证结果是否可信？"**
   - 当前: 基础验证可信，但模型假设需要修正
   - 建议: Week 3修正PSK和MetricVersion假设

### 最终建议

**Week 1 Triage决策**:
- ✅ **继续推进** - 基础架构健康，问题可解决
- ⚠️ **调整预期** - 从180行目标调整为200-250行（架构扩展）
- 🔄 **分阶段交付** - Week 2-3优先修正Critical问题，Week 4进行Major扩展

**成功标准**:
- Week 2结束: 所有引理有executability验证
- Week 3结束: PSK和MetricVersion建模现实化
- Week 4结束: 支持多跳分析和loop-freedom验证

---

## 附录: 代码引用索引

### 关键代码位置

**PSK建模问题**:
- `baseline_lora_dv.spthy:33` - PSK验证逻辑
- `patched_lora_dv.spthy:33` - PSK验证逻辑

**MetricVersion问题**:
- `patched_lora_dv.spthy:39-42` - 常量版本递增
- `patched_lora_dv.spthy:19` - 硬编码'v0'初始化

**多跳建模缺失**:
- 两个模型均缺少rebroadcast规则
- 两个模型均缺少hop-by-hop转发

**引理缺失**:
- 两个模型均无executability lemmas
- 两个模型均无loop-freedom lemmas

---

**分析完成时间**: 2026-03-29  
**分析者**: Claude Code (Tamarin结构分析专家)  
**状态**: Ready for Week 1 Triage Decision
