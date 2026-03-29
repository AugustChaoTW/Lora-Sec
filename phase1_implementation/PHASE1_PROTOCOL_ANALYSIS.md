# Phase 1 實施記錄：協議選型與需求分析

**開始時間**：2026-03-29  
**進度**：選型與可行性驗證進行中

---

## 任務 T1.1：選定目標協議並提取規範

### 方案 A：Meshtastic 2.x

**GitHub**：https://github.com/meshtastic/firmware  
**版本**：2.6.x（最新穩定）  

**控制面功能驗證中**：
- [ ] Route discovery 機制（flooding vs. 明確路由表）
- [ ] Key derivation / rekey 機制
- [ ] Authentication 與 HMAC
- [ ] 動態加入/離開流程

**進度**：正在逆向分析...

---

### 方案 B：LoRaMesher

**GitHub**：https://github.com/LoRaMesher/LoRaMesher  
**特點**：開源、實驗性質

**控制面功能驗證中**：
- [ ] Routing protocol 類型
- [ ] Key management
- [ ] Authentication scheme
- [ ] Protocol overhead

**進度**：正在逆向分析...

---

## 協議抽象規格（待定）

根據實際控制面功能確定以下：

**Phase A：Join & Initial Keying**  
（待確認實際流程）

**Phase B：Route Discovery**  
（待確認實際流程）

**Phase C：Data Forwarding**  
（待確認實際流程）

**Phase D：Periodic Rekey**  
（待確認實際流程）

---

## 環境搭建 T1.3

### Tamarin Prover 安裝

```bash
# Step 1：驗證 Tamarin 是否已安裝
which tamarin-prover

# Step 2：若未安裝，進行安裝
# （根據系統選擇 macOS / Linux / Docker）

# Step 3：驗證安裝
tamarin-prover --version

# Step 4：執行官方 demo
# （見下方）
```

**當前狀態**：⏳ 待執行

---

## ns-3 LoRa 模擬環境

### 需求
- ns-3 核心 (>= 3.35)
- FLoRa PHY 模組
- 或使用 LoRaSim 替代

**當前狀態**：⏳ 待決定

---

**預期完成日期**：2026-04-01（第 2–3 天）  
**下一步**：提取實際協議規範 & 環境搭建

