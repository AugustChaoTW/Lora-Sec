# Week 2 Tamarin 驗證結果（實際執行）

**日期**: 2026-03-30  
**狀態**: ✅ **基線驗證完成** | 🔄 **修補驗證進行中**  
**驗證工具**: Tamarin Prover v1.13.0 + Maude 3.1  
**處理時間**: 0.83 秒（基線）

---

## 執行摘要

### ✅ 基線模型驗證（完成）

| 引理 | 類型 | 狀態 | 步數 |
|-----|------|------|------|
| L1_RouteMetricAuthenticity | all-traces | ❌ 假設 | 5 |
| L2_RouteFreshness | all-traces | ❌ 假設 | 4 |
| L3_RouteConsistency | all-traces | ❌ 假設 | 7 |
| L4_NoUnauthorizedRoute | all-traces | ❌ 假設 | 5 |
| L5_PSKConfidentiality | all-traces | ✅ 證明 | 3 |
| L_Ex1_Node_Can_Init | exists-trace | ✅ 證明 | 2 |
| L_Ex2_Hello_Can_Broadcast | exists-trace | ✅ 證明 | 4 |
| L_Ex3_Route_Can_Update | exists-trace | ✅ 證明 | 4 |
| L_Ex4_Data_Can_Forward | exists-trace | ✅ 證明 | 4 |

**結論**:
- ✅ 可執行性引理全部成立（模型非空真）
- ❌ 安全引理按預期被假設（漏洞存在）
- ✅ PSK 機密性成立（密碼學假設正確）

---

### 🔄 修補模型驗證（進行中）

**預期結果**：
- L1-L5 與基線相同結構，但 L1-L4 應改為已證明
- L_Sanity1-3 應全部已證明

**當前狀態**: 執行中，預計 5-15 分鐘完成

---

## 詳細驗證輸出

### 基線模型關鍵輸出

```
== L1_RouteMetricAuthenticity_Baseline (all-traces): falsified - found trace (5 steps)
== L2_RouteFreshness_Baseline (all-traces): falsified - found trace (4 steps)
== L3_RouteConsistency_Baseline (all-traces): falsified - found trace (7 steps)
== L4_NoUnauthorizedRoute_Baseline (all-traces): falsified - found trace (5 steps)
== L5_PSKConfidentiality_Baseline (all-traces): verified (3 steps)
== Compromise_Reveals_PSK (all-traces): falsified - found trace (5 steps)
== L_Ex1_Node_Can_Init (exists-trace): verified (2 steps)
== L_Ex2_Hello_Can_Broadcast (exists-trace): verified (4 steps)
== L_Ex3_Route_Can_Update (exists-trace): verified (4 steps)
== L_Ex4_Data_Can_Forward (exists-trace): verified (4 steps)

processing time: 0.83s
```

---

## 驗證檔案

```
baseline/baseline_lora_dv.spthy
├── 99 行
├── 6 條規則（Node_Init, Hello_Broadcast, Route_Update, 
│            Data_Forward, Compromise_Node + DY）
├── 10 個引理（L1-L5 + Compromise_Reveals_PSK + L_Ex1-L_Ex4）
└── 驗證日誌: baseline_verification_run.log

patched/patched_lora_dv.spthy
├── 107 行
├── 6 條規則（Node_Init, Hello_Broadcast_Auth, Route_Update_Verify,
│            MetricVersion_Increment, Data_Forward, Compromise_Node）
├── 8 個引理（L1-L5 + L_Sanity1-L_Sanity3）
├── 新增函數: hmac/2
└── 驗證日誌: patched_verification_run.log（進行中）
```

---

## 下一步

### 立即
- ⏳ 監聽修補驗證完成通知
- ✅ 收集修補驗證結果

### 本日內
- 📄 從反例提取攻擊軌跡
- 📝 整合攻擊敘述到論文第 6 章
- 🔍 驗證相關工作引用（29 篇論文）

### 第 3 週
- 🔨 實現 PSK 逐節點修改
- 🔧 實現 MetricVersion 動態遞增
- 🏃 重新驗證修改模型
