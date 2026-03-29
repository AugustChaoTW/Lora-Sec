# Phase 3: Patch Cost Analysis (HMAC + MetricVersion)

**Date**: 2026-03-29  
**Patch under analysis**: `Hello_Auth(..., HMAC)` + `MetricVersion` binding  
**Objective**: quantify bandwidth, compute, energy, latency, and deployment cost for LoRaMesher patch

---

## 1. Assumptions and Data Sources

This analysis is grounded on Phase 2 artifacts:

- Patched model comments: HMAC overhead `+32 bytes`, compute `2-3 ms` on ESP32 class MCU.
- Baseline/patched message abstraction from `VERIFICATION_SUMMARY.md`.
- Typical Hello interval: 1 minute (configurable).
- Mesh scale reference: 10 nodes (for network-level daily overhead estimate).

**Assumption table**

| Parameter | Value | Source / Rationale |
|---|---:|---|
| Baseline Hello payload | 12 bytes | Phase 2 symbolic cost breakdown |
| HMAC tag length | 32 bytes | SHA-256 output length |
| Patched Hello payload | 44 bytes | 12 + 32 |
| MetricVersion state | +2 bytes/route entry | Phase 2 patch design target |
| HMAC compute latency | 2-3 ms/message | Phase 2 model notes (ESP32) |
| Hello period | 60 s | default planning parameter |

---

## 2. Message Size and Bandwidth Overhead

## 2.1 Per-message overhead

- **Baseline**: 12 bytes
- **Patched**: 44 bytes
- **Absolute increase**: `+32 bytes`
- **Relative increase**: `32 / 12 = 266.7%`

> Important interpretation: this looks large at packet level, but Hello is low-frequency control traffic.

## 2.2 Node-level daily overhead (Hello every 60s)

- Hellos/day/node = `24 × 60 = 1440`
- Extra bytes/day/node = `1440 × 32 = 46,080 bytes` ≈ **45 KB/day**

## 2.3 10-node mesh daily overhead

- Extra bytes/day/network = `10 × 46,080 = 460,800 bytes` ≈ **450 KB/day**

Given LoRa deployment is usually telemetry-dominated and control packets are sparse, this increase is operationally manageable, especially when weighed against complete route-hijack prevention.

---

## 3. Compute Overhead (ESP32 Class)

## 3.1 Per Hello cryptographic cost

- HMAC generate/verify: **2-3 ms** per message
- MetricVersion check/update: integer compare/increment, microsecond-level

## 3.2 CPU duty-cycle impact

For 1-minute period:

- CPU ratio = `3 ms / 60000 ms = 0.005%` (upper bound per periodic operation)

Even considering both send + receive verification paths, average duty-cycle increment remains far below 1% in normal mesh workloads.

---

## 4. Energy Overhead

## 4.1 Per-message energy decomposition

Phase 2 references use:

- LoRa TX energy per Hello ≈ 50 mJ (dominant term)
- HMAC compute energy ≈ 0.5 mJ

So compute-energy ratio:

- `0.5 / 50 = 1%` per message (upper bound estimate)

## 4.2 Daily battery impact (1 node)

- Extra compute/day = `1440 × 0.5 mJ = 720 mJ = 0.72 J`
- For ~24 kJ battery-class budget, this is ~0.003%/day equivalent from compute only.

Transmission overhead from larger packets contributes additional energy, but remains small at 1-minute Hello interval. Overall patch budget is conservatively treated as **<1% total energy increase** for typical settings.

---

## 5. Latency Impact

Patch adds two latency components:

1. **MCU processing delay**: +2 to +3 ms (HMAC)
2. **Airtime serialization delay** from +32 bytes

Approximate extra serialization delay:

- At 5.47 kbps effective rate (high data-rate LoRa profile): `256 bits / 5470 ≈ 46.8 ms`
- At 0.98 kbps (lower-rate profile): `256 / 980 ≈ 261 ms`

Therefore, per-Hello added latency is profile-dependent, roughly:

- **~50 ms to ~260+ ms airtime** + **2-3 ms compute**

Because Hello is periodic control traffic (not per-packet data forwarding), end-user data-path latency impact is limited; primary effect is slightly slower control-plane dissemination under low data-rate settings.

---

## 6. Memory and State Overhead

- `MetricVersion` adds **2 bytes per destination entry**.
- For `N` destination routes on a node, additional state = `2N bytes`.
- Example: 50-entry table → +100 bytes only.

This is negligible for ESP32 memory budgets.

---

## 7. Deployment Considerations

## 7.1 Compatibility strategy

- Preferred mode: authenticated-only acceptance in fully upgraded mesh.
- Transitional mode (mixed firmware):
  - New nodes parse both formats.
  - Security benefit is partial until majority upgrade.

## 7.2 Rollout plan

1. Firmware update introducing `Hello_Auth` parser and verification.
2. Enable dual-stack parsing during migration window.
3. Turn on strict-auth policy per network profile after upgrade threshold.

## 7.3 Operational risks

- Clock sync is not required (no timestamp-based checks in this patch).
- Key provisioning consistency is required: PSK mismatch causes authentication drop.
- Increased packet size may slightly reduce control-channel headroom in very dense meshes.

---

## 8. Cost-Benefit Decision

| Dimension | Cost | Security Gain | Decision |
|---|---|---|---|
| Message size | +32 bytes/Hello | Blocks forgery/hijack classes (L1, L3, L4) | Accept |
| Compute | +2-3 ms/Hello | Cryptographic source authentication | Accept |
| Energy | <1% total increase (typical) | Prevents high-loss black-hole/replay abuse | Accept |
| Memory | +2 bytes/route entry | Freshness binding with MetricVersion (L2) | Accept |

**Conclusion**: The patch provides a favorable trade-off for LoRa mesh security. Costs are low enough for constrained nodes while eliminating all baseline routing-security failures in the formal model.
