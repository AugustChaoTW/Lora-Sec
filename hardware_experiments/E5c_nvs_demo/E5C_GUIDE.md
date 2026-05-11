# E5c: NVS-Persisted MetricVersion Counter — Reboot Replay Attack Demo

## 目的

證明 NVS persistence 消除了 MetricVersion 計數器在重開機後的攻擊窗口。

## 節點角色

| 板子 | 角色 | sketch |
|------|------|--------|
| Board 1 | Node A (Sender) | node_a/node_a.ino |
| Board 2 | Node B (Gateway) | node_b/node_b.ino |
| Board 3 | Node C (Attacker) | node_c/node_c.ino |

## 三組對比實驗

### 實驗 1 — SECURITY_MODE 0（baseline，無驗證）
- Node A `#define SECURITY_MODE 0`
- **預期**：任何 Hello 都被接受；Node C 不需要重播，直接偽造即可

### 實驗 2 — SECURITY_MODE 1（HMAC only，無 MetricVersion）
- Node A `#define SECURITY_MODE 1`，Node B `#define SECURITY_MODE 1`
- **攻擊步驟**：
  1. Node C 捕捉 Node B 的 Hello（mv=5，合法 HMAC）
  2. Reboot Node A（按 EN 按鈕）
  3. Node C 重播 Hello（mv=5，HMAC 仍有效）
  4. Node A 重開後 MetricVersion=0，接受 mv=5 → 路由被毒化
- **預期**：`[ROUTE] Added dst=0xBB via=0xCC` — 攻擊成功

### 實驗 3 — SECURITY_MODE 2（HMAC + MetricVersion + NVS，完整修補）
- Node A `#define SECURITY_MODE 2`，Node B `#define SECURITY_MODE 2`
- **攻擊步驟**：同上（Node C 捕捉 mv=5，Reboot Node A，重播）
- Node A 重開後從 NVS 讀到 mv[BB]=10（已累積），拒絕 mv=5
- **預期**：`[DROP] Hello from 0xBB: MV=5 <= stored=10 (REPLAY REJECTED)`

## 燒錄步驟

```bash
# Node A
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc node_a/
arduino-cli upload  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  -p /dev/ttyACM0 node_a/

# Node B
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc node_b/
arduino-cli upload  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  -p /dev/ttyACM1 node_b/

# Node C
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc node_c/
arduino-cli upload  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  -p /dev/ttyACM2 node_c/
```

## 讀 Serial 輸出

```bash
# 三個視窗分別：
python3 -c "
import serial, sys
s = serial.Serial(sys.argv[1], 115200, timeout=1)
while True:
    l = s.readline()
    if l: print(l.decode(errors='replace'), end='')
" /dev/ttyACM0   # 換成實際 port
```

## 關鍵 log 訊息對照表

| 訊息 | 意義 |
|------|------|
| `[NVS] Loaded mv[0xBB]=10` | Node A 重開機後成功讀取 NVS，攻擊窗口關閉 |
| `[DROP] Hello from 0xBB: MV=5 <= stored=10` | Replay 被拒絕 |
| `[ROUTE] Added dst=0xBB via=0xCC` | 路由被毒化（攻擊成功，MODE 0/1） |
| `[ATTACK] Replayed Hello from 0xBB mv=5` | Node C 已送出重播封包 |

## 結果記錄（填入 results/ 目錄）

實驗跑完後，將三個 MODE 的 Serial log 儲存為：
- `results/mode0_node_a.log`
- `results/mode1_node_a.log`
- `results/mode2_node_a.log`

---

## 硬體規格（Ebyte EoRa PI）

**產品頁**：https://www.ebyte.com/product/2122.html  
**手冊**：EoRa_PI_UserManual_CN_v1.1.pdf  
**原理圖**：EoRa PI開發板原理圖.pdf（ZIP 內）

### 板子型號

| 項目 | 規格 |
|------|------|
| 完整型號 | EoRa-S3-900TB |
| MCU | ESP32-S3FH4R2（240 MHz，4 MB Flash，2 MB PSRAM） |
| LoRa 模組 | E22-900MM22S（SX1262，850–930 MHz） |
| OLED | 0.96 吋 SSD1306，128×64 |
| USB | Type-C，USB 2.0 CDC（Native ESP32-S3 USB） |
| FQBN | `esp32:esp32:esp32s3:CDCOnBoot=cdc` |

### LoRa SPI 腳位（E5c firmware 使用）

| 信號 | GPIO |
|------|------|
| NSS (CS) | 7 |
| SCK | 5 |
| MOSI | 6 |
| MISO | 3 |
| RST | 8 |
| BUSY | 34 |
| DIO1 | 33 |

### OLED I2C 腳位（從原理圖 + I2C scanner 實測確認）

| 信號 | GPIO | 備註 |
|------|------|------|
| SDA | **18** | ⚠️ 原理圖標示有誤（標為 SCL），實測 SDA=18 |
| SCL | **17** | ⚠️ 原理圖標示有誤（標為 SDA），實測 SCL=17 |
| I2C 位址 | 0x3C | I2C scanner 實測確認 |

```cpp
// 正確初始化方式
Wire.begin(18, 17);                    // SDA=18, SCL=17
SSD1306Wire oled(0x3c, 18, 17);       // address, SDA, SCL
```

> **注意**：原理圖 I/O mapping table 中 OLED 的 SDA/SCL 標示顛倒，
> 實際上 GPIO18=SDA、GPIO17=SCL，已透過 I2C scanner 實測驗證。

### 其他板載資源

| 功能 | GPIO |
|------|------|
| RGB LED | 37 |
| BOOT 按鍵 | 0 |
| EN（Reset）| RST pin |
| macOS port 格式 | `/dev/cu.usbmodem*` |

### macOS 燒錄指令（augustmacbook-pro 實測）

```bash
~/bin/arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" \
  --library "/Users/fychao/Documents/Arduino/libraries/RadioLib" \
  --library "/Users/fychao/Documents/Arduino/libraries/ESP8266_and_ESP32_OLED_driver_for_SSD1306_displays" \
  --upload -p /dev/cu.usbmodem1341301 \
  node_a/
```
