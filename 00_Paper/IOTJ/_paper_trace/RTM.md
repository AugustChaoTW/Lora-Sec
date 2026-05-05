# Requirements Traceability Matrix (RTM)

Paper: `/home/augchao/Work/Lora-Sec/00_Paper/IOTJ/IOTJ-Aug.tex`  
Trace: `/home/augchao/Work/Lora-Sec/00_Paper/IOTJ/_paper_trace/v4`

## RQ → Contribution → Experiment → Figure/Table

| RQ | Contributions | Experiments | Figures/Tables | Risks |
|----|----|----|----|-----|
| RQ1 | C1, C2 | E1, E2, E4 | fig:attack-overview, fig:attack-seq, tab:tamarin, tab:attacks | — |
| RQ2 | C2, C3 | E3, E4, **E5a** | tab:esp32, tab:tamarin, tab:overhead | R2 |
| RQ3 | C4, C5 | E1, E2, E3, **E5b, E5c** | fig:eval-results, tab:attacks, tab:overhead | R1 |

## Risk Register Summary

| Risk ID | Severity | Status | Description | 更新 |
|---------|----------|--------|-------------|------|
| R1 | medium | open | 9-node ns-3 vs. large-scale gap | — |
| R2 | medium | **CLOSED** | HMAC timing benchmark→實測 | 2026-05-05: 實測 98 µs (0.098 ms) @ ESP32-S3 240 MHz，論文已更新 |
| R3 | low | open | Dolev-Yao 不覆蓋 impl. bugs | — |
| R4 | low | open | Single attacker only | — |
| R5 | low | open | LoRaMesher 特定，未驗證其他 stack | — |

## Hardware Experiment Log (E5)

| 日期 | 項目 | 結果 |
|------|------|------|
| 2026-05-05 | 設備取得，esptool 確認晶片 | ESP32-S3 (QFN56 rev0.2), 4MB/2MB, SX1262 |
| 2026-05-05 | 板子型號確認 | **Heltec WiFi LoRa 32 V3**（pin 序列比對） |
| 2026-05-05 | 頻段 | ✅ **AS923 Plan，920–925 MHz**（台灣合法），實驗設定 923 MHz |
| 2026-05-05 | E5a HMAC timing | ✅ **98 µs ± 2 µs (0.098 ms)** — 3× faster than 0.3 ms claim; overhead 0.06% |
| TBD | E5b Route injection PoC | pending |
| TBD | E5c OTA latency | pending |