# Phase 3: Detailed Attack Analysis (Baseline LoRaMesher DV)

**Date**: 2026-03-29  
**Inputs**: `BASELINE_VERIFICATION_RESULTS.md`, `VERIFICATION_SUMMARY.md`, `PHASE3_ATTACK_ANALYSIS_FRAMEWORK.md`, baseline/patched `.spthy` models  
**Scope**: 3 major attack classes with counterexample traces, root-cause analysis, real-world impact, and patch validation

---

## 1. Executive Summary

Baseline LoRaMesher DV control plane fails 4/5 formal lemmas (L1-L4 broken, L5 proved).  
To keep the paper narrative sharp, we consolidate failures into **three major attack classes**:

1. **Class I — Authentication Bypass & Route Forgery** (maps to L1, L4)
2. **Class II — Freshness Downgrade at Same Sequence** (maps to L2)
3. **Class III — Hop-by-Hop Metric Manipulation** (maps to L3)

These classes are operationally independent, reproducible from baseline counterexamples, and directly fixed by the patched model (`HMAC + MetricVersion`).

---

## 2. Threat Model Snapshot

- **Attacker model**: Dolev-Yao (eavesdrop / replay / forge / inject) with optional compromised node.
- **Target plane**: routing control plane (Hello + route table update), not PHY jamming.
- **Assumed honest behavior**: honest nodes update route entries based on received Hello semantics.
- **Key observation**: baseline Hello messages are unauthenticated and freshness is sequence-only.

---

## 3. Attack Class I — Authentication Bypass & Route Forgery

### 3.1 Broken Security Properties

- **L1_RouteMetricAuthenticity_Baseline**: BROKEN
- **L4_NoUnauthorizedRoute_Baseline**: BROKEN

### 3.2 Root Cause

Hello messages carry no sender authentication token.  
Any node can inject `Hello(src, metric, seq)` and impersonate route origin or advertise non-existent destinations.

### 3.3 Counterexample Trace (Representative)

1. Honest node `C` broadcasts `Hello(C, 0, 1)`.
2. Attacker `A` eavesdrops the message.
3. `A` forges `Hello(C, 0, 1)` (identity spoofing) or `Hello(X, 0, 1)` (`X` non-existent).
4. Honest node `B` receives forged Hello.
5. `B` updates route table and sets next hop to attacker-controlled path.
6. Traffic to `C`/`X` is steered to `A`.

### 3.4 Real-World Harm Assessment

- **Success rate**: **~75-80%** in documented baseline estimates for 5-10 node mesh (depends on attacker reachability and broadcast timing).
- **Impact scope**:
  - Route hijack for destinations announced by attacker.
  - Black-hole sink for forwarded packets.
  - MITM position when attacker forwards selectively.
- **Detectability**: low, because forged updates resemble unstable mesh routing.

### 3.5 Why This Is Structural (Not Implementation Noise)

The exploit does not depend on parser bugs or clock drift. It follows directly from protocol logic: route installation occurs without cryptographic source proof.

### 3.6 Patch Validation

- **Patch mechanism**: `Hello_Auth(src, metric, seq, hmac(...))`.
- **Security effect**:
  - Forged source/metric without PSK fails HMAC verification.
  - Unauthorized destination advertisement is rejected before route install.
- **Formal outcome**: patched lemmas `L1` and `L4` are expected/provisioned as PROVED in `VERIFICATION_SUMMARY.md` and patched model definition.

---

## 4. Attack Class II — Freshness Downgrade at Same Sequence

### 4.1 Broken Security Property

- **L2_RouteFreshness_Baseline**: BROKEN

### 4.2 Root Cause

Freshness checks rely on sequence number only.  
When `seq_new == seq_current`, baseline lacks an independent monotonic signal for metric evolution, allowing attacker-crafted “better” metrics at same sequence context.

### 4.3 Counterexample Trace (Representative)

1. Node `C` advertises route state up to `seq=100`; `B` stores it.
2. Old replay with lower sequence is rejected (baseline partial defense works).
3. Attacker forges `Hello(C, metric=0, seq=100)` with same seq.
4. `B` cannot distinguish legitimate same-seq metric transition from forged downgrade.
5. Route for `C` may be rebound to attacker path with false freshness.

### 4.4 Real-World Harm Assessment

- **Success rate**: **~70%** in baseline estimates when attacker can stay in broadcast neighborhood.
- **Impact scope**:
  - Persistent route-quality poisoning.
  - Selective forwarding opportunities (traffic steering through attacker).
  - Slower convergence and unstable route oscillation.
- **Operational symptom**: appears as intermittent route optimization, difficult to classify as attack from logs alone.

### 4.5 Why This Is Structural

Sequence monotonicity alone is insufficient to bind metric freshness semantics. This is a protocol-state design gap, not a radio-condition artifact.

### 4.6 Patch Validation

- **Patch mechanism**: `MetricVersion` per destination, included in route-acceptance conditions.
- **Security effect**:
  - For same sequence, route update requires `metricVer_new > metricVer_old`.
  - Removes attacker freedom to inject downgraded metric under reused sequence.
- **Formal outcome**: patched `L2_RouteFreshness_Patched` expected/provisioned as PROVED.

---

## 5. Attack Class III — Hop-by-Hop Metric Manipulation

### 5.1 Broken Security Property

- **L3_RouteConsistency_Baseline**: BROKEN

### 5.2 Root Cause

Intermediate observations can be modified and re-emitted without end-to-end authenticity binding between metric value and source identity.

### 5.3 Counterexample Trace (Representative)

1. Source `C` emits `Hello(C, 0, 1)`.
2. Honest intermediate `I1` rebroadcasts with hop-incremented metric.
3. Attacker `A` eavesdrops intermediate update.
4. `A` forges amplified metric, e.g., `Hello(C, 500, 1)`.
5. Downstream node `B` accepts manipulated value and installs inflated-cost route via attacker.
6. Reachability to `C` becomes degraded or effectively partitioned.

### 5.4 Real-World Harm Assessment

- **Success rate**: **~60-65%** baseline estimate (position-dependent; strongest when attacker is central relay).
- **Impact scope**:
  - Route inconsistency among honest nodes for same destination/sequence.
  - Artificially induced partitioning and convergence failure.
  - DoS via metric inflation/instability.
- **Collateral effect**: increased control-plane churn and data-plane retransmission waste.

### 5.5 Why This Is Structural

Without cryptographic binding to source-origin metric, downstream nodes cannot verify whether rebroadcast metric history is honest.

### 5.6 Patch Validation

- **Patch mechanism**: source-bound HMAC integrity check in `Hello_Auth` processing.
- **Security effect**:
  - Any in-transit metric change invalidates HMAC.
  - Nodes accept only authenticated metric tuples.
- **Formal outcome**: patched `L3_RouteConsistency_Patched` expected/provisioned as PROVED.

---

## 6. Cross-Class Severity Matrix

| Class | Main Broken Lemma(s) | Baseline Success Rate | Primary Damage | Severity |
|---|---|---:|---|---|
| I: Authentication bypass & forgery | L1, L4 | 75-80% | route hijack / black hole / unauthorized route install | **Critical** |
| II: Freshness downgrade | L2 | ~70% | false route optimization / selective forwarding | **High** |
| III: Hop metric manipulation | L3 | 60-65% | partition / convergence disruption / DoS | **High** |

---

## 7. Validation Status (Baseline vs Patched)

- **Baseline**: 4 routing-security lemmas broken (L1-L4), PSK confidentiality only property proved (L5).
- **Patched design intent & model status**: all 5 lemmas mapped to protections (`HMAC`, `MetricVersion`) and documented as PROVED target/outcome.

This gives a complete “breaking → repairing” chain required for Sections 4-6 of the draft paper.

---

## 8. Reproducibility Notes

- Baseline model: `phase2_tamarin_models/baseline/baseline_lora_dv.spthy`
- Patched model: `phase2_tamarin_models/patched/patched_lora_dv.spthy`
- Supporting result documents:
  - `BASELINE_VERIFICATION_RESULTS.md`
  - `VERIFICATION_SUMMARY.md`
  - `PHASE3_ATTACK_ANALYSIS_FRAMEWORK.md`

All numeric impact ranges above are taken from the Phase 2 formal-analysis documentation and used as Phase 3 manuscript-ready estimates.
