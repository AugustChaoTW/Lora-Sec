# 硬體實驗規劃

**建立日期**: 2026-05-05  
**設備**: ESP32-S3（已確認，/dev/cu.usbmodem134201 @ augustmacbook-pro）  
**論文目標**: IEEE IoTJ IOTJ-Aug.tex  
**核心動機**: 關閉 Reviewer Risk R2（HMAC timing 目前來自 mbedTLS benchmark，非實測）

---

## 現狀分析

| 論文現有數據 | 來源 | Reviewer 風險 |
|------------|------|--------------|
| HMAC 0.3 ms / 10,000 iter | mbedTLS 2.28 文件 benchmark | **R2 medium** — 非實測 |
| ns-3 latency 245–250 ms | C++ simulation | R1 medium — 非硬體 |
| Attack 0% / 100% PDR | ns-3 simulation | R1 medium — 非硬體 |

硬體實驗目標：將 R2 從 **open** 變成 **closed**，R1 部分緩解。

---

## 實驗優先順序

### Phase 1：HMAC Micro-Benchmark（最高優先）

**目標**: 在真實 ESP32-S3 上實測 HMAC-SHA256 時間，取代論文中的 mbedTLS benchmark 說法

**需要設備**: 1 個 ESP32-S3（已有）

**實驗步驟**:
1. 在 Arduino IDE / ESP-IDF 寫 HMAC 計時 firmware
2. 使用 `esp_timer_get_time()`（microsecond 精度）
3. 計算 HMAC-SHA256 (16-byte tag, 64-byte payload) 10,000 次
4. 輸出 mean ± std (µs)
5. 對比論文數字：目標確認 ≈ 300 µs (0.3 ms)

**論文修改**:
- §6.1 第 705 行：`"mbedTLS 2.28 on ESP32-S3 @ 240 MHz"` → `"measured on [device] @ 240 MHz"`
- Tab:esp32：新增 `Measured` 欄，與 benchmark 對比
- R2 狀態：**open → closed**
- C3 confidence：0.92 → 0.98

**估計工時**: 2–3 小時（含 firmware 撰寫 + 燒錄 + 量測）

---

### Phase 2：LoRaMesher PoC Attack Demo（次高優先）

**目標**: 在真實硬體上示範 Route Injection 攻擊與防禦，從「simulation-only」升級為「hardware-validated」

**需要設備**: 2–3 個帶 LoRa radio 的 ESP32 板（需確認手邊設備是否有 SX1262/SX1276）

**實驗步驟**:
1. 安裝 LoRaMesher Arduino 函式庫
2. **Node A + B**: Flash baseline LoRaMesher，建立 mesh
3. **Attacker node（或 Node A 作為 attacker）**: 發送 forged RouteReply(metric=1)
4. 確認 Node B 路由表被污染（serial log 觀察）
5. Flash patched LoRaMesher（加入 HMAC 驗證）
6. 重複攻擊，確認被阻擋（HMAC failure log）

**論文修改**:
- §7 新增 §7.4 "Hardware Validation"（約 0.5 col）
- 新增 Tab:hardware：device model、attack success on hw、HMAC verify time
- Abstract 最後一句：加入 "confirmed on real ESP32-S3 hardware"
- R1 狀態：部分緩解

**估計工時**: 4–6 小時（若設備有 LoRa radio）

---

### Phase 3：Over-the-Air Latency Measurement（選做）

**目標**: 實測 baseline vs. patched 的端對端 latency，驗證 ns-3 模擬數字的可信度

**需要設備**: 2 個帶 LoRa radio 的 ESP32 板

**實驗步驟**:
1. Node A 送 64-byte packet，Node B 接收
2. 用 timestamp 計算 one-way latency（或 RTT / 2）
3. Baseline 和 patched 各量 100 次
4. 對比 ns-3 數字：baseline ~245 ms，patched ~250 ms

**論文修改**:
- Tab:overhead 或 Tab:hardware 新增「Hardware measured」欄
- 討論 simulation vs. hardware 的誤差範圍

**估計工時**: 3–4 小時

---

## 設備確認清單（2026-05-05 更新）

| 項目 | 狀態 | 說明 |
|------|------|------|
| ESP32-S3 已偵測 | ✅ | /dev/cu.usbmodem134201 @ augustmacbook-pro |
| 晶片型號 | ✅ | ESP32-S3 (QFN56, revision v0.2) |
| Flash / PSRAM | ✅ | 4MB Flash (XMC) + 2MB PSRAM |
| LoRa radio 型號 | ✅ | **SX1261** (Semtech SX126x, 150–960 MHz) |
| Framework | ✅ | Arduino on ESP-IDF v4.4.5 |
| 設備數量 | ✅ | **6 個**（Phase 1 需 1，Phase 2/3 需 2）|
| 板子完整型號 | ✅ | **Heltec WiFi LoRa 32 V3**（pin 序列 8,9,10,11,12,13 比對確認）|
| 頻段設定 | ✅ | **AS923 Plan（920–925 MHz）**，台灣合法頻段，E5b 設定 923 MHz |
| LoRaMesher 函式庫 | ❓ | GitHub: LoRaMesher/LoRaMesher，需安裝 |
| Arduino IDE | ✅ | 已安裝於 augustmacbook-pro |

---

## 論文升級效益總結

| 實驗 | 關閉的 Risk | 升級的 Contribution | 工時 |
|------|-----------|-------------------|------|
| Phase 1 HMAC timing | R2 (medium) | C3: 0.92 → 0.98 | 2–3 hr |
| Phase 2 PoC Demo | R1 (partially) | C4: new hw evidence | 4–6 hr |
| Phase 3 Latency | R1 (partially) | C4: sim fidelity | 3–4 hr |

**建議**: 至少完成 Phase 1，IoTJ 審稿人最在意的是 "actual hardware" 這句話。

---

## 下一步

1. 確認設備型號（在 Mac 上跑 `esptool.py --port /dev/cu.usbmodem134201 chip_id`）
2. 確認設備數量和 LoRa radio 型號
3. 確認 Arduino IDE 是否已安裝
4. 依此決定做到 Phase 1 還是 Phase 1+2
