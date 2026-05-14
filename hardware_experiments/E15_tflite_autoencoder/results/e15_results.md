# E15 Hardware Experiment Results: TFLite Autoencoder IDS

**Platform**: Ebyte EoRa PI (ESP32-S3 + SX1262)  
**Nodes**: 5 (A=0xAA sender, R1=0x11 relay, C=0xCC rogue relay, B=0xBB gateway, D=0xDD autoencoder IDS)  
**Topology**: A → R1 → C(rogue) → B, IDS=D promiscuous  
**LoRa**: 923.0 MHz, SF9/BW125/CR4/7, 10 dBm  
**Attack**: Same rogue relay as E13 (valid PSK, HMAC-valid Hellos, drops all data)  
**Model**: TFLite autoencoder trained on normal node behavior, threshold=0.015424 (loaded from NVS)  
**Date**: 2026-05-14

---

## Autoencoder Feature Vector (per node, per observation)

```
[rssi, hop_count, interval_s, mv_delta, pdr]
```

Threshold computed offline during baseline phase (`train_autoencoder.py`), saved to NVS.

---

## MSE Results

| Node | Role | MSE range | Threshold | Flagged? | Verdict |
|------|------|-----------|-----------|----------|---------|
| 0xCC | Rogue relay | 0.053–0.063 | 0.015424 | ✅ 28 alerts | **TRUE POSITIVE** |
| 0x11 | Honest relay | 0.030–0.040 | 0.015424 | ⚠️ 29 alerts | **FALSE POSITIVE** |

Both nodes produce MSE well above threshold. The autoencoder correctly identifies 0xCC as anomalous (MSE 3.5–4× threshold) but also flags 0x11 (2–2.5× threshold).

---

## Detection Performance

| Metric | Value |
|--------|-------|
| TPR (0xCC detected) | 100% (all 9 observations flagged) |
| FPR (0x11 falsely flagged) | 100% (all 9 observations flagged) |
| Precision | 50% (1 true anomaly / 2 flagged nodes) |
| Inference time | 6–7 μs per sample (ESP32-S3 SRAM model) |
| Threshold | 0.015424 (NVS-loaded from prior baseline run) |

---

## Attacker / Gateway Counts

| Metric | Value |
|--------|-------|
| Attacker total_dropped | 46 packets |
| Gateway RX | 18 packets |
| Rogue Hellos (HMAC valid) | ✅ accepted |

---

## Key Findings

### 1. TFLite on ESP32-S3: Feasible (6–7 μs inference)
Autoencoder inference runs in 6–7 μs on ESP32-S3 — negligible overhead for IDS on
a node with LoRa airtime dominating (1692 ms per fragment). Model fits in SRAM.

### 2. Attacker detected (TPR=100%)
0xCC produces feature vectors with irregular hop_count + high mv_delta + elevated
interval variance → MSE consistently 3.5–4× above trained threshold.

### 3. High false positive rate (FPR=100%)
Honest relay 0x11 also exceeds threshold during attack phase. Likely cause:
- During the attack, 0x11's routing table is poisoned → it retransmits to 0xCC,
  causing unusual interval and pdr patterns relative to the baseline (clean network)
- Threshold 0.015424 was calibrated on attack-free baseline; once rogue is present,
  even honest node behavior deviates from baseline

### 4. Threshold sensitivity gap
The gap between honest relay MSE (0.030–0.040) and attacker MSE (0.053–0.063) is
small but consistent. A threshold of ~0.045 would separate them:
- TPR = 100% (0xCC still detected: 0.053 > 0.045)
- FPR = 0% (0x11 not flagged: 0.040 < 0.045)

**Requires re-calibration** with attack-phase data to compute a context-aware threshold.

---

## Comparison with E13

| IDS Strategy | Rogue detected? | FPR | Inference cost |
|---|---|---|---|
| E13: KLD + Hamming Distance | ❌ FNR=100% | 1 FP (0x11 flood) | None (rule-based) |
| E15: TFLite Autoencoder | ✅ TPR=100% | ⚠️ FPR=100% (threshold issue) | **6–7 μs** |

TFLite autoencoder has the **right signal** (attacker MSE > honest MSE) but needs threshold tuning.
KLD has the wrong signal (routing timing, not data-plane drop) and cannot detect this attack class.

---

## Files

- Logs: `e15_node_{a,b,c,d,r1}.log`
- Training script: `../train_autoencoder.py`
- Common header: `../e15_common.h`
- Firmware: `../node_{a,b,c_attacker,d_autoencoder,r1}/`
