# EXPERIMENTAL RESULTS BASELINE

## 實驗配置

- 平台：ns-3.41 C++ API（以 Simulator/NodeContainer 驅動）
- 場景：6 scenarios × 3 runs = 18
- 指標：攻擊成功率、端到端延遲、吞吐量、PDR

## Baseline 統計（平均 ± 標準差）

| 拓撲 | 攻擊 | 攻擊成功率(%) | 延遲(ms) | 吞吐量(bps) | PDR |
|---|---|---:|---:|---:|---:|
| linear | spoofing | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.000 ± 0.000 |
| linear | replay | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.000 ± 0.000 |
| tree | spoofing | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.000 ± 0.000 |
| tree | replay | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.000 ± 0.000 |
| grid | spoofing | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.000 ± 0.000 |
| grid | selective | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.000 ± 0.000 |

## 攻擊成功率比較

- linear/spoofing:  0.00%
- linear/replay:  0.00%
- tree/spoofing:  0.00%
- tree/replay:  0.00%
- grid/spoofing:  0.00%
- grid/selective:  0.00%

## 關鍵發現

- 最高攻擊成功率場景：linear/spoofing，平均 0.00%
- 最低攻擊成功率場景：grid/selective，平均 0.00%
- Baseline 在未認證 DV 路由下可穩定重現路由操縱與流量劣化。
