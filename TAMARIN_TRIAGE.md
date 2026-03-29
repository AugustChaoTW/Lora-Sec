# 🔍 Tamarin Model Triage Report
## LoRa Mesh DV Routing — 问题严重程度与修复计划

**生成日期**: 2026-03-29 (Week 1 Day 1)  
**分析对象**: 
- `phase2_tamarin_models/baseline/baseline_lora_dv.spthy` (87 lines)
- `phase2_tamarin_models/patched/patched_lora_dv.spthy` (94 lines)

---

## 📊 Executive Summary

| 问题 | 严重程度 | 工期 | 优先级 | 状态 |
|------|---------|------|--------|------|
| **问题1: PSK Source Auth** | 🔴 CRITICAL | 4-5天 | P0 | Triage完成 |
| **问题2: MetricVersion常数** | 🔴 CRITICAL | 4-5天 | P0 | Triage完成 |
| **问题3: 多跳无环** | 🟡 MAJOR | 6-7天 | P1 | 可限制范围 |

**总工期估算**:
- **最小 (P0问题1+2)**: 6-8周 ✅ **推荐**
- **完整 (P0+P1问题1+2+3)**: 10-12周
- **快速降级**: 4周 (仅限claim范围)

---

## 🔴 问题1: PSK Source Authentication — CRITICAL

### 问题描述

**当前建模** (`baseline_lora_dv.spthy` L10-18 + L26-30):
```tamarin
rule Node_Init:
  [ Fr(~id), Fr(~psk), Fr(~seq) ]
  [ !PSK(~id,~psk) ]

rule Route_Update:
  [ In(<src,metric,seq_new,nextHop>), !Node(dst,'honest') ]
  [ !RouteTable(dst,src,metric,nextHop,seq_new) ]
```

**论文声称**（implicit in abstract/intro）:
> "HMAC authentication prevents metric spoofing"
> 
> "Only authorized routes are accepted"

**实际能证明的**:
```
✅ Group Authentication: 
   "消息来自某个持有PSK的节点"
   
❌ Source Authentication:
   "消息来自特定的节点X"
   
❌ Identity Binding:
   "HMAC(msg, psk) 只能证明 'someone knows psk'"
   "无法区分哪个具体节点"
```

### 根本原因

**金钥建模不现实**:

```
当前假设:
  ∀ nodes i, j: 存在共享PSK，所以:
  - 节点i的HMAC = hmac(msg, PSK_i)
  - 节点j知道PSK_i，可以验证HMAC_i
  - 结果: 任何知道PSK的节点都可伪造任何其他节点的消息
```

**Tamarin形式化影响**:

在`patched_lora_dv.spthy` L28-37（route update verify），即使加了HMAC验证:
```tamarin
rule Route_Update_Verify:
  [ In(<src,metric,seq_new,nextHop,'v0',hmac(...)>),
    !PSK(src,psk_src),
    !PSK(dst,psk_src)  // 两个节点共享PSK
  ]
  --[ HMACVerified(dst,src,...) ]->
```

这个lemma无法真正证明 "metric来自节点src"，因为:
- 节点dst知道src的PSK
- 节点dst可以自己生成合法的HMAC
- 从密码学角度: **群组认证 (Group Auth)** ≠ **源认证 (Source Auth)**

### 投稿影响

| 场景 | 论文表述 | 评审反应 | 后果 |
|------|---------|---------|------|
| **不修** | "source auth via HMAC" | "Model只能证明group auth，claims不符" | ❌ Major Revision或Reject |
| **修为群组auth** | "group auth + integrity" | "OK，但stronger mechanism会更好" | ⚠️ Acceptable with note |
| **改为签名** | "source auth via digital signatures" | "Strong, proper source binding" | ✅ Excellent |
| **改per-node key** | "per-node keys with trusted mapping" | "Sound, deployment practical" | ✅ Good |

### 修复方案排序

#### 方案A: 数字签名（最强）
**机制**:
```tamarin
rule Node_Init_SignKey:
  [ Fr(~id), Fr(~signKey) ]
  [ !SignKey(~id, ~signKey), !VerifyKey(~id, pk(~signKey)) ]

rule Hello_Broadcast_Signed:
  [ !SignKey(n, sk), !HelloSeq(n, seq) ]
  [ Out(<n, '0', seq, sign(<n,'0',seq>, sk)>) ]

rule Route_Update_VerifySig:
  [ In(<src, metric, seq, sign_tag>),
    !VerifyKey(src, pk_src),
    Condition(verify(sign_tag, <src,metric,seq>, pk_src) = true)
  ]
  [ !RouteTable(...) ]
```

**优势**:
- ✅ 真正的source authentication
- ✅ 无法被其他节点伪造
- ✅ 论文可直接宣称"authenticated routing"

**劣势**:
- ❌ LoRa constrained device上签名开销（ESP32可能>10ms）
- ❌ 需要PKI或证书基础设施

**工期**: 45-90分钟 (含compromise分支可能到2小时)  
**复杂度**: Tamarin模型+20-30行, 验证时间可能2-5倍

#### 方案B: Per-Node唯一密钥（推荐平衡）
**机制**:
```
每节点i有唯一的认证密钥K_i
- 初始化: Node i stores K_i privately
- 其他节点j存储: KeyMapping(i, K_i) 或从PKI获取
- Hello broadcast: HMAC(msg, K_i)
- Route update verify: HMAC验证 + identity binding via KeyMapping
```

**优势**:
- ✅ 真正的source authentication
- ✅ 认证开销仅是HMAC（已实现）
- ✅ 无需public-key infrastructure

**劣势**:
- ⚠️ 需要secure key distribution mechanism (out-of-band/PKI)
- ⚠️ Tamarin模型需要显式的key mapping状态

**工期**: 60-120分钟  
**复杂度**: Tamarin模型+30-40行, 验证时间可能1.5-2倍

#### 方案C: Group Auth + 隐式信任（最快，次强）
**机制**:
```
保留当前HMAC + group PSK
但修改论文表述:
- 改为: "Group Authentication & Integrity"
- 不说: "Source Authentication"
- 说明: "HMAC prevents unauthorized modifications by non-group members"
```

**优势**:
- ✅ 无需改Tamarin代码（仅改论文claim）
- ✅ 工期最短（30-35分钟）

**劣势**:
- ❌ 论文强度大幅下降
- ❌ 无法说"secure routing"
- ❌ 接近于"integrity-only"

**工期**: 30-35分钟  
**复杂度**: 无（仅论文修改）

### 推荐决策

**选择: 方案B (Per-Node密钥)** ✅

**理由**:
1. 工期可控 (60-120分钟)
2. 安全性最强 (true source auth)
3. 与HMAC实现兼容 (仅加key mapping)
4. Tamarin模型影响中等 (可在W3实施)

**Week 3日期**: W3 Day 1-3 修复PSK + 重新验证

---

## 🔴 问题2: MetricVersion常数 — CRITICAL  

### 问题描述

**当前建模** (`patched_lora_dv.spthy` L19 + L23-26 + L39-42):
```tamarin
rule Node_Init:
  [ !MetricVersion(~id,~id,'v0') ]  // 常数 v0

rule Hello_Broadcast_Auth:
  [ !MetricVersion(n,n,'v0') ]
  [ Out(<n,'zero',seq,n,'v0',hmac(...)>) ]

rule MetricVersion_Increment:
  [ !MetricVersion(n,n,'v0') ]
  [ !MetricVersion(n,n,'v0') ]  // v0 → v0 (无变化!)
```

**论文声称**（implicit in formal model description）:
> "Metric version binding prevents metric downgrade/replay"
>
> "L2: Route freshness is guaranteed by monotonic version increments"

**实际能证明的**:
```
✅ 可以证明: "消息包含一个'v0'标记"
✅ 可以证明: "两条消息都有'v0'"

❌ 无法证明: "v0在时间上单调递增"
❌ 无法证明: "新版本 > 旧版本"
❌ Lemma L2 freshness: 逻辑上 VACUOUS
   (因为版本永不改变，freshness约束无意义)
```

### 根本原因

**版本是硬编码常数**:

```
在Bellman-Ford DV routing中，freshness依赖:
  metric_update(t+1) 包含 version(t+1)
  且 version(t+1) > version(t)
  
当前模型:
  version 永远是 'v0'
  结果: 无法验证"版本递增"
  
这导致L2 lemma成为 vacuous truth:
  "所有使用v0的消息都使用v0" ✓ (technically true, but meaningless)
```

### 投稿影响

| 场景 | 论文表述 | 评审反应 | 后果 |
|------|---------|---------|------|
| **不修** | "freshness via metric version" | "Proof looks vacuous—version is constant" | ❌ Reject (logic error) |
| **修成动态** | "freshness via monotonic version" | "Sound proof, good design" | ✅ Excellent |

### 修复方案

**单一正确方案: 实现真实的后继关系**

```tamarin
// 1. 定义版本后继关系
functions: succ/1
equations: 
  succ(succ(x)) != x  // 防止循环

// 2. 改Node_Init为实际产生版本
rule Node_Init:
  [ Fr(~id), Fr(~v0) ]
  [ !MetricVersion(~id, ~id, ~v0) ]

// 3. 改为真实递增
rule MetricVersion_Increment:
  [ !MetricVersion(n, n, v_old) ]
  --[ MetricIncrement(n, v_old) ]->
  [ !MetricVersion(n, n, succ(v_old)) ]

// 4. 修改Lemma使用版本
lemma L2_RouteFreshness:
  "All node dst src seq_new seq_old ver_new ver_old #i #j.
    RouteUpdate(dst,src,seq_new,ver_new) @ i &
    RouteUpdate(dst,src,seq_old,ver_old) @ j &
    i < j
    ==> ver_new != ver_old"  // 版本必然不同
```

**工期**: 45-90分钟  
**复杂度增加**: ~20-30行新规则，但验证时间仅+10-15%

**Week 3日期**: W3 Day 3-4 与PSK修复合并

---

## 🟡 问题3: 多跳无环 — MAJOR (条件性)

### 问题描述

**当前建模**:
```
无显式per-hop转发规则
无rebroadcast规则
无loop-detection属性
假设: rebroadcast是原子操作
```

**论文可能声称**（需要确认）:
```
Scenario A: "Routes converge without loops" ❌ 无法证明
Scenario B: "Secure propagation in stable topology" ✅ 可证明
```

### 两条路线

#### 路线A: 有限范围（推荐）
```
声称: "≤3跳链式路由的安全metric传播"
不声称: loop-freedom (无环收敛)
Limitation: "Model assumes bounded diameter ≤3"

工期: 0天（无额外工作）
可行性: 可直接写入limitations section
```

#### 路线B: 完整建模（仅if声称loop-freedom）
```
需要补充:
  - Per-node route state
  - Send/receive/rebroadcast分步
  - Loop-freedom不变量 (seq/rank/path-vector)
  
工期: 6-7天
复杂度: 模型扩展到300+行
验证时间: 可能从30分钟 → 2-4小时
```

### 推荐决策

**论文定位**: "Breaking and Repairing LoRa Mesh Security"  
**主卖点**: 形式化验证 + 实际攻击 + 最小补丁  
**可达成无**: 不必声称完整loop-freedom  

**选择: 路线A (有限范围)** ✅

**理由**:
1. LoRa mesh通常<3跳（实际约束）
2. 3跳验证足以捕获关键漏洞
3. 避免状态爆炸（per-hop routing table O(n²))
4. 工期节省6-7天

**论文limitations**:
```
"Our formal model assumes network diameter ≤3 hops,
 which is typical for LoRa mesh deployments. Extension
 to arbitrary topologies would require per-hop routing
 state and loop-prevention mechanisms (future work)."
```

**Week 3-4日期**: W3 Day 5-6 限定为3跳，写入limitations

---

## 📋 修复时间线（W2-W4）

### Week 2 (Executability + Sanity Lemmas)
```
Day 1-2: Executability lemmas (验证模型非空)
Day 3-4: Sanity lemmas (基础可达性)
Day 5: 小规模验证 (<1分钟/lemma)

Deliverable: baseline + 4 executability + 3 sanity lemmas
验证时间: 总计<5分钟
```

### Week 3 (PSK + MetricVersion 修复)
```
Day 1-3: PSK per-node key映射 + HMAC验证更新
Day 3-4: MetricVersion后继关系 + 递增规则
Day 5-6: 重新验证 + Tamarin proof日志

Deliverable: patched model with source auth + freshness
验证时间: ~30分钟
```

### Week 4 (多跳 + 限定范围)
```
Day 1-3: 多跳转发规则 (per-hop, ≤3跳限制)
Day 4-5: Loop-detection属性 (bounded)
Day 6-7: 最终验证 + Proof日志

Deliverable: 模型 ≥200行, 所有lemma证明
验证时间: ~1-2分钟
```

---

## 🎯 Week 1 Day 5 决策检查表

**用户需确认 (Day 5)**:

- [ ] **PSK修复路线**: 
  - [ ] 方案A (签名) — 最强但最复杂
  - [x] 方案B (per-node key) — 推荐
  - [ ] 方案C (仅降级claim) — 最快

- [ ] **MetricVersion修复**: 
  - [x] 实现动态递增版本 — 无其他选项

- [ ] **多跳模型路线**:
  - [x] 路线A (≤3跳有限范围) — 推荐
  - [ ] 路线B (完整无环建模) — 仅if声称

---

## 📊 Triage总结表

| 问题 | 严重度 | 修复方案 | 工期 | Week | 优先级 | 推荐 |
|------|--------|---------|------|------|--------|------|
| **PSK Auth** | 🔴 | Per-node key | 60-120min | W3-1-3 | P0 | ✅ B方案 |
| **MetricVersion** | 🔴 | 动态递增 | 45-90min | W3-3-4 | P0 | ✅ 必须 |
| **多跳无环** | 🟡 | ≤3跳限制 | 0天 | W3末 | P1 | ✅ A路线 |

**总工期**: **13-16天** (W2-1 → W4-3)  
**目标线数**: baseline ≥180行, patched ≥200行  
**目标引理**: 10-12个 (含executability/sanity)

---

## ✅ Week 1 Day 1 Triage完成

**状态**: ✅ COMPLETE — 3个问题全部分类

**后续行动** (Day 2-5):
- [ ] Day 2: ns-3修复验证 (9场景子集)
- [ ] Day 3-4: 27场景重新运行
- [ ] Day 5: 里程碑决策 (Week 2-4工期确认)

---

**Prepared by**: Sisyphus (Analysis Mode)  
**Date**: 2026-03-29 (Week 1 Day 1)  
**Review**: Oracle (LLM-1) consulted on all 3 issues  
**Status**: Ready for Week 2-4 execution
