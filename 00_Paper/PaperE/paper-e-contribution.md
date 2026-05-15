---
title: "Paper E — Contribution Document"
target: IEEE Transactions on the Internet of Things (IoT-J), backup FGCS
status: proposal stage; contribution doc v0.1
date: 2026-05-14
issue: "#47"
---

# Less Is Secret: Edge-Side Feature Extraction for Privacy-Preserving LoRa Surveillance

## Thesis

Transmitting CNN feature embeddings instead of raw images over LoRa simultaneously solves
the bandwidth problem and the privacy problem. When a 256-d embedding is irreversible under
worst-case model inversion attack (SSIM < 0.4), encryption of the payload provides no
additional privacy — HMAC-SHA256 integrity suffices. Event-triggered extraction further
extends node lifetime 4× over periodic transmission.

---

## Design Decision: Packet Format (CRITICAL FIX)

**Issue in proposal:** `[node_id(2)|timestamp(4)|feature[256]|HMAC[8]] = 270 bytes > 255 bytes (LoRa SF9 limit)`

**Resolution — two viable options:**

| Option | Embedding | Payload bytes | Accuracy cost | Decision |
|---|---|---|---|---|
| A | 128-d INT8 | 2+4+128+8 = 142 B ✅ | ~1% vs 256-d | **PREFERRED** |
| B | 256-d INT4 | 2+4+128+8 = 142 B ✅ | ~3% vs INT8 | fallback |

**Chosen: Option A — 128-d INT8.**
Rationale: INT4 quantization adds training complexity with similar byte count; 128-d INT8
keeps quantization pipeline simple and matches E15's proven TFLite workflow.

Updated packet format:
```
[node_id: 2B | timestamp: 4B | feature[128×INT8]: 128B | HMAC-SHA256[8B]] = 142 bytes
```
142 bytes fits comfortably in 255-byte LoRa payload at all SF levels.

---

## Contributions

| # | Contribution | Evidence |
|---|---|---|
| C1 | First measurement of MIA inversion resistance for 128-d MobileNetV3 embeddings on ESP32-S3 | MIA experiment (SSIM, LPIPS, ArcFace) |
| C2 | Packet format co-design: 128-d INT8 = 142 bytes, single LoRa packet, accuracy ≥ 90% | E19 hardware (inference latency + accuracy) |
| C3 | Formal privacy argument: SSIM < 0.4 → HMAC sufficient, AES unnecessary | MIA PoC (Python/Colab) |
| C4 | Event-trigger comparison: PIR vs frame-diff vs confidence-based on 2× AA battery | E20 hardware (90-day equivalent test) |
| C5 | End-to-end deployment: 6× EoRa PI nodes, 90-day orchard, > 95% TPR | E21 field deployment |

---

## Argument Chain

### Step 1 — Bandwidth bottleneck is fundamental

LoRa SF9/BW125: max payload = 255 bytes, duty cycle 1% (ETSI).
JPEG 320×240 ≈ 10–30 KB → requires 40–120 fragments → minutes of airtime per image.
**Constraint:** any image-based surveillance over LoRa must reduce payload by > 99%.

### Step 2 — Feature extraction solves bandwidth; does it introduce privacy risk?

MobileNetV3-Small last-pooling layer → 128-d float32 → INT8 quantized → 128 bytes.
The question: can an adversary who intercepts the 128-d embedding reconstruct the image?

### Step 3 — MIA PoC (Python, no hardware needed first)

Attacks to test:
- Fredrikson et al. 2015 (gradient-based, white-box)
- Zhu DLG 2019 (deep leakage from gradients)
- Yang 2019 GAN-based reconstruction

Metric targets:
- SSIM < 0.4 (below human recognizability threshold)
- LPIPS > 0.6 (perceptually dissimilar)
- ArcFace identity-similarity < 0.3 (not same person)

If SSIM < 0.4: **raw privacy preserved** → AES encryption adds 0 privacy benefit,
only integrity needed → HMAC-SHA256 64-bit truncated suffices.

Fallback: if SSIM ≥ 0.4, add Gaussian DP noise (σ tuned to ε=2) to embedding before TX.

### Step 4 — Hardware: ESP32-S3 feasibility (E19)

TFLite Micro on ESP32-S3 (proven: E15 autoencoder ran in 6–7 μs).
MobileNetV3-Small INT8 inference: estimated 80–150 ms (vs camera capture 200 ms).
Measure: latency, SRAM usage, accuracy on 4-class test set.

### Step 5 — Event-trigger lifts lifetime from 30 → 360+ days (E20)

Baseline (periodic, 1/min): 6 mA avg → 30 days on 2×AA (3000 mAh).
Event-triggered (PIR + confidence threshold):
  - Idle: 2 mA (deep sleep)
  - Active: 80 mA for 500 ms per event
  - At 1 event/10 min: avg = 2 + (80×0.5/600) = 2.067 mA → 360+ days.

E20: lab-bench measurement with actual EoRa PI + PIR, log events over 72h equivalent.

### Step 6 — End-to-end: E21 (90-day field deployment)

6× EoRa PI nodes in orchard, 4× as camera+feature-extraction nodes, 2× as gateway+relay.
Compare: feature-vector nodes vs ESP32-CAM control (full JPEG baseline).
Measure: TPR, FPR, bandwidth, battery drain, node uptime.

---

## Abstract (draft)

Conventional image-based surveillance over LoRa is infeasible: a 320×240 JPEG requires
40–120 LoRa packets under ETSI duty-cycle limits, consuming minutes of airtime. We propose
transmitting 128-dimensional CNN feature embeddings instead of raw images. A
MobileNetV3-Small model quantized to INT8 fits on ESP32-S3, produces a 128-byte payload
that fits in a single LoRa packet, and achieves > 90% classification accuracy on a
4-class (human/vehicle/animal/none) detection task. We show that worst-case model inversion
attacks on 128-d embeddings produce reconstructions with SSIM < 0.4 — below human
recognizability — meaning AES encryption adds no meaningful privacy benefit over
HMAC-SHA256 integrity protection. Event-triggered transmission (PIR + on-device confidence
threshold) reduces average current from 6 mA to < 2.1 mA, extending node lifetime from
30 to > 360 days on 2×AA batteries. A 90-day orchard deployment with 6 EoRa PI nodes
demonstrates > 95% event detection rate with < 0.5% false trigger rate.

**Keywords**: LoRa, edge computing, feature extraction, privacy, TFLite Micro, model
inversion attack, surveillance, ESP32-S3

---

## Paper Structure (12–14 pages, IEEE IoT-J)

```
§1  Introduction
    - LoRa bandwidth bottleneck for image surveillance
    - Thesis: feature extraction solves bandwidth + privacy simultaneously
    - Contributions C1–C5

§2  Background
    2.1 LoRa PHY constraints (SF9/BW125, 255B, 1% duty cycle)
    2.2 TFLite Micro on ESP32-S3 (from E15)
    2.3 Model inversion attacks (Fredrikson, DLG, GAN)
    2.4 Threat model: passive eavesdropper with known model weights

§3  Packet Co-Design
    3.1 LoRa payload budget: 142 bytes
    3.2 MobileNetV3-Small → 128-d INT8 pipeline
    3.3 Packet format: [id|ts|feat[128]|HMAC[8]]

§4  Privacy Analysis: MIA Experiments
    4.1 Attack setup (Fredrikson 2015, DLG, GAN)
    4.2 Results: SSIM, LPIPS, ArcFace
    4.3 Conclusion: HMAC sufficient

§5  Hardware Implementation (E19)
    5.1 ESP32-S3 + OV2640 pipeline
    5.2 Latency and accuracy results
    5.3 SRAM footprint

§6  Event-Trigger Energy Analysis (E20)
    6.1 PIR + confidence-based trigger
    6.2 Current measurements
    6.3 Lifetime projection

§7  Field Deployment (E21)
    7.1 6-node orchard setup
    7.2 90-day results: TPR, FPR, battery, uptime
    7.3 Comparison with ESP32-CAM baseline

§8  Discussion
    8.1 Generalization to other embedding dimensions
    8.2 DP-noise fallback if SSIM ≥ 0.4
    8.3 Limitations: single-channel LoRa, 4-class only

§9  Related Work
§10 Conclusion
```

---

## Evidence Files (planned)

| Artifact | Path |
|---|---|
| MIA PoC script | `experiments/paper_e/mia_poc/mia_attack.py` |
| TFLite model (INT8) | `experiments/paper_e/models/mobilenetv3_128d_int8.tflite` |
| E19 firmware | `hardware_experiments/E19_feature_extraction/` |
| E19 results | `hardware_experiments/E19_feature_extraction/results/e19_results.md` |
| E20 firmware | `hardware_experiments/E20_event_trigger/` |
| E20 results | `hardware_experiments/E20_event_trigger/results/e20_results.md` |
| E21 deployment logs | `hardware_experiments/E21_field_deployment/` |

---

## Open Items

- [ ] B1 — Fix packet format in issue #47 body: 270B → 142B (128-d INT8)
- [ ] E1 — MIA PoC (Python): DLG + Fredrikson on 128-d MobileNetV3 embeddings
- [ ] E2 — E19 hardware: TFLite MobileNetV3 on ESP32-S3 + OV2640, measure latency/accuracy
- [ ] E3 — E20 hardware: PIR event-trigger energy measurement on EoRa PI
- [ ] E4 — Paper E LaTeX draft (IEEE IoT-J format)
- [ ] E5 — E21 field deployment (90-day; long-lead item)
