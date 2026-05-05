# E5a 燒錄指南 — Heltec WiFi LoRa 32 V3

## 確認資訊
- **Board**: Heltec WiFi LoRa 32 V3（ESP32-S3 + SX1262）
- **Port**: `/dev/cu.usbmodem134201` @ augustmacbook-pro
- **IDE**: Arduino IDE (已安裝)
- **注意**: LoRa radio 本實驗**不使用**，純 CPU HMAC benchmark

---

## 方法 A：Arduino IDE GUI（推薦）

### Step 1 — 安裝 Heltec ESP32 Board Package
1. 開啟 Arduino IDE
2. `Arduino IDE → Preferences → Additional boards manager URLs`，加入：
   ```
   https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series/releases/download/0.0.9/package_heltec_esp32_index.json
   ```
3. `Tools → Board → Boards Manager` → 搜尋 `Heltec` → Install

### Step 2 — 選擇板子
```
Tools → Board → Heltec ESP32 Series Dev-boards → WiFi LoRa 32(V3)
Tools → Port → /dev/cu.usbmodem134201
Tools → CPU Frequency → 240MHz
Tools → USB CDC On Boot → Enabled
```

### Step 3 — 開啟 Sketch
```
File → Open → hardware_experiments/E5a_hmac_timing/hmac_benchmark/hmac_benchmark.ino
```

### Step 4 — 編譯 & 燒錄
- 按 `→ Upload`（或 Cmd+U）
- 等燒錄完成（約 30 秒）

### Step 5 — 讀取結果
- `Tools → Serial Monitor` → Baud rate: `115200`
- 板子自動重置後開始跑 benchmark（約 30 秒）
- 複製整段輸出，存到 `e5a_raw.log`

---

## 方法 B：命令列（SSH 遠端執行）

```bash
# 在 augustmacbook-pro 上安裝 arduino-cli 並燒錄
ssh fychao@augustmacbook-pro
cd /path/to/Lora-Sec/hardware_experiments/E5a_hmac_timing
bash flash_and_run.sh /dev/cu.usbmodem134201
```

---

## Step 6 — 解析結果（在 Linux 主機上）

把 serial output 複製到本機，然後：
```bash
cd /home/augchao/Work/Lora-Sec/hardware_experiments/E5a_hmac_timing
python3 parse_e5a.py --file e5a_raw.log
```

或直接從 serial port 捕捉（在 Mac 上）：
```bash
python3 parse_e5a.py --port /dev/cu.usbmodem134201
```

---

## 預期輸出

```
=== E5a: HMAC-SHA256 Benchmark ===
Chip:     ESP32-S3 rev0  @ 240 MHz
...
=== RESULTS ===
mean:   XXX.X µs  (X.XXX ms)    ← 論文目標確認 ~300 µs
std:    XX.X µs
p50:    XXX.X µs
p95:    XXX.X µs
LoRa air-time ref: ~165000 µs  → HMAC overhead: X.XX%

JSON_BEGIN
{"exp":"E5a","chip":"ESP32-S3",...}
JSON_END
```

---

## 結果確認後論文修改位置

**IOTJ-Aug.tex 第 705 行：**
```
改前: "0.3~ms (mbedTLS~2.28 on ESP32-S3 @ 240~MHz), measured over 10,000 iterations"
改後: "X.XX~ms (XXX~µs ± XX~µs, 10,000 iterations, Heltec WiFi LoRa 32 V3, ESP32-S3 @ 240~MHz)"
```

**R2 reviewer risk → status: closed**
