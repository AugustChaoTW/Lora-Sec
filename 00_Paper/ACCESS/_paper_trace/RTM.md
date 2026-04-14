# Requirements Traceability Matrix (RTM)

Paper: `/home/augchao/Work/Lora-Sec/00_Paper/ACCESS/ACCESS-Aug.tex`  
Trace: `/home/augchao/Work/Lora-Sec/00_Paper/ACCESS/_paper_trace/v2`

## RQ → Contribution → Experiment → Figure/Table

| RQ | Contributions | Experiments | Figures/Tables | Risks |
|----|----|----|----|-----|
| RQ1 | C5 | E2, E4 | T:1, T:2 | — |
| RQ2 | C3, C5 | E2, E4 | T:1, T:2 | — |
| RQ3 | C4, C5 | E3 | T:3 | — |

## Risk Register Summary

| Risk ID | Severity | Status | Description |
|---------|----------|--------|-------------|
| R1 | medium | open | (auto-generated placeholder — add specific reviewer risks)... |
| R2 | medium | open | 所有實驗均為 ns-3 模擬，缺乏 ESP32 等資源受限硬體的實機驗證，評審者可能購疑碼表、HMAC 中斷延遲對電池寿... |
| R3 | low | open | Tamarin 使用保守假設（無側通道、無時序攻擊），審查者可說形式模型跟實際 LoRa 物理層語意不符... |
| R4 | medium | open | 模擬拓撲最多 9 個節點，實際大規模 LoRa Mesh（>50 nodes）可能呈現不同的收斂動態和攻擊表面... |
| R5 | low | open | 協議形式化對象為 LoRaMesher，審查者可能購疑是否適用於 Meshtastic、OpenThread 等相似協議... |