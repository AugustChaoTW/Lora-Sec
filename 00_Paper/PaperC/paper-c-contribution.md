---
title: "Paper C — Contribution Document"
target: IEEE Transactions on Information Forensics and Security (TIFS)
status: experiments complete, draft pending
date: 2026-05-14
---

# The Attack Category Gap: How Constrained-Header Design Determines Routing Attack Taxonomy in LoRa Mesh Protocols

## Thesis

Two widely deployed LoRa mesh protocols (LoRaMesher, Meshtastic) both yield an "adversarial relay" prediction under the same formal routing model, yet deployed firmware produces categorically different attack behaviors. This formal-firmware divergence is not a modeling error — it is a direct consequence of header semantic design choices, and it determines the correct defense architecture.

---

## Contributions

| # | Contribution | Evidence |
|---|---|---|
| C1 | First quantification of formal-firmware divergence at the header-semantics layer | Tamarin baseline vs two firmware behaviors |
| C2 | Cross-protocol attack taxonomy: same formal prediction, different attack categories | E16 hardware (100% intercept, GW PDR=100%) |
| C3 | Passive IDS is fundamentally blind to Meshtastic active relay (FNR=100% for all strategies) | E17 hardware (both IDS strategies fail) |
| C4 | Patch asymmetry: same HMAC primitive at different layers produces asymmetric outcomes | E18 hardware (LM blocked 7/7, Mesh bypassed 7/7) |
| C5 | Formal verification of Meshtastic AEAD AAD patch (L3 proved) | meshtastic_patch.spthy, 20s |

---

## Argument Chain

### Step 1 — The Formal Prediction (same for both protocols)

Standard Tamarin routing models predict: an attacker who controls the relay hint can become the sole relay, enabling intercept + selective drop.

Both `baseline_lora_dv.spthy` (Paper B) and `meshtastic_routing.spthy` (Paper C) falsify the same lemmas:
- L2 Packet_Reachability: FALSIFIED
- L3 No_Relay_Takeover: FALSIFIED

Formal models at this abstraction level cannot distinguish the two protocols — the header field encoding is below the model's grain.

### Step 2 — The Header Semantic Difference

| Protocol | Header encoding | Attacker receives packet? |
|---|---|---|
| LoRaMesher | `dst = next_hop` (field collapse) | **No** — Node B sees `dst ≠ self`, discards silently |
| Meshtastic v2.6+ | `to = final_dst`, `next_hop = relay hint` (separate) | **Yes** — Node C sees `next_hop = self`, receives and processes |

The field collapse in LoRaMesher means the intended relay field doubles as the discard predicate at honest intermediate nodes. When poisoned, the packet reaches an absent destination. In Meshtastic, `to` is always the gateway; `next_hop` is advisory. The attacker is reachable as a relay, not an absent destination.

### Step 3 — Hardware Validation: E16 (3× EoRa PI, 120s)

```
Sender TX:          25 packets
Attacker intercept: 25/25 = 100%  (active relay — receives payload)
Gateway PDR:        25/25 = 100%  (RELAY_HIJACK mode)
Only gateway signal: hop_limit 3→2 (relay used, attacker identity unknown)
```

Contrast with LoRaMesher E5b (Paper B):
- E5b gateway PDR = 0%  → attack detectable by availability monitor
- E16 gateway PDR = 100% → availability monitor gives FNR = 100%

Serial log (direct):
```
[ATTACKER] ACTIVE RELAY — attacker receives payload
[CONTRAST] LoRaMesher: pkt carried dst=0xCC, discarded silently at Node B.
           Here: attacker RECEIVES the packet.
[GATEWAY]  BLIND: no anomaly signal at gateway for relay-path attacks
```

### Step 4 — IDS Architecture Consequence: E17 (4× EoRa PI, 120s)

|  | GW-centric IDS | Relay-side dst-binding IDS |
|---|---|---|
| LoRaMesher silent BH | FNR = 100% | FNR ≈ 0% (dst=0xCC anomalous) |
| Meshtastic active relay | FNR = 100% | **FNR = 100%** (to=GW, indistinguishable) |

Meshtastic attack packets carry `to=0xBB` (gateway) — identical to legitimate relay traffic. No passive observer can distinguish attacker relay from honest relay. This is not a placement problem; it is a cryptographic binding problem. Defense requires `next_hop` to be authenticated at the protocol layer.

### Step 5 — Patch Asymmetry: E18 (4× EoRa PI, 120s)

Same HMAC-SHA256 primitive applied at different protocol layers:

**LoRaMesher + HMAC on Hello (control plane):**
- Attack vector = forged Hello → routing table poisoned
- HMAC verification fails on forged Hello → attack BLOCKED
- 7/7 attempts rejected, routing table intact throughout

**Meshtastic + HMAC on NodeInfo (control plane):**
- Attack vector = ROUTE_POISON on `MeshPacket.next_hop` (data plane)
- ROUTE_POISON is not a NodeInfo → HMAC on NodeInfo is irrelevant
- 7/7 ROUTE_POISON accepted, `next_hop=0xCC` from packet #1

Root cause: control-plane authentication ≠ data-plane field protection. The attack layer and the patched layer do not overlap.

Serial log (direct):
```
[ATTACKER] LoRaMesher+HMAC: MAC verification FAILS — attack BLOCKED
[ATTACKER] Meshtastic+HMAC: poison accepted — attack SUCCEEDS
[MESH]     ROOT-CAUSE: Attack vector is MeshPacket.next_hop (data plane),
           not NodeInfo (control plane). Different layer.
[MESH]     CORRECT-FIX: Bind next_hop in AEAD AAD or sign MeshPacket header.
```

### Step 6 — Formal Verification of Correct Patch: meshtastic_patch.spthy

Patch: `tag = HMAC(from || to || next_hop || pkt_id || ciphertext, chan_psk)`

Any tamper with `next_hop` invalidates `tag` at receiving relay. Relay_Forward_Patched requires decryption with shared channel PSK — attacker without PSK cannot produce valid AEAD ciphertext with modified `next_hop`.

Tamarin results (20.16s, lora-mesh-tamarin Docker):

| Lemma | Baseline | Patched |
|---|---|---|
| L1 NodeInfo_Authenticity | FALSIFIED | FALSIFIED (NodeInfo unpatched — independent) |
| L2 Packet_Reachability | FALSIFIED | **PROVED** (exists-trace) |
| L3 No_Relay_Takeover | FALSIFIED | **PROVED** (key result) |
| L4 Admin_Authenticity | FALSIFIED | FALSIFIED (target not bound — independent) |
| L5 Executability | PROVED | PROVED |

L3 PROVED: in all traces, `PacketRelayed('attacker', ...)` is impossible after patch. The attacker cannot inject a new `next_hop` without the channel PSK.

---

## Abstract (draft)

Formal security models for mesh routing protocols predict that an attacker who controls routing state can become an adversarial relay — intercepting, inspecting, or dropping traffic. We show that this prediction, while formally correct, masks a categorical difference in deployed behavior that determines the appropriate defense architecture. In LoRaMesher, a field-collapse design (`dst = next_hop`) causes poisoned packets to reach an absent destination, producing a silent black hole: the gateway sees zero PDR, but the attacker never receives the payload. In Meshtastic v2.6+, separate `to` and `next_hop` fields allow the attacker to become an active relay: the gateway sees 100% PDR while the attacker intercepts every packet. We validate both behaviors on physical EoRa PI hardware (6× ESP32-S3 + SX1262 boards). We further show that the attack category difference implies a defense category difference: relay-side anomaly detection closes the LoRaMesher gap (FNR ≈ 0%) but is fundamentally blind to the Meshtastic attack (FNR = 100% for all passive IDS strategies), and HMAC on control messages closes LoRaMesher but not Meshtastic. The correct Meshtastic fix binds `next_hop` in AEAD additional authenticated data — a property we formally verify with Tamarin Prover (L3 No_Relay_Takeover: proved after patch). Our results yield concrete design guidelines for constrained-airtime mesh protocols: header field separation creates active relay exposure, and any unprotected routing hint field requires cryptographic binding to the payload ciphertext.

**Keywords**: LoRa mesh security, formal verification, Tamarin Prover, routing attack taxonomy, active relay, silent black hole, Meshtastic, LoRaMesher

---

## Paper Structure (12–14 pages, IEEE TIFS)

```
§1  Introduction
    - Motivation: formal models predict same attack for both protocols
    - Gap: firmware behavior diverges categorically
    - Contributions C1–C5

§2  Background
    2.1 LoRaMesher: DV routing, dst=next_hop field collapse
    2.2 Meshtastic v2.6+: managed flooding, separate to/next_hop
    2.3 Threat model: Dolev-Yao adversary, physical LoRa layer

§3  Formal Model — Baseline
    3.1 Tamarin encoding: meshtastic_routing.spthy
    3.2 Lemma results: L1–L4 FALSIFIED, L5 PROVED
    3.3 What the model cannot capture: header field encoding

§4  The Formal-Firmware Gap
    4.1 LoRaMesher: dst=next_hop → silent black hole mechanism
    4.2 Meshtastic: separate next_hop → active relay mechanism
    4.3 Taxonomy: availability attack vs confidentiality + selective drop

§5  Hardware Validation: E16
    5.1 Setup: 3× EoRa PI, ROUTE_POISON attack
    5.2 Results: 100% intercept, GW PDR=100%
    5.3 Contrast with E5b (Paper B): PDR=0% vs PDR=100%

§6  IDS Architecture Consequences: E17
    6.1 Strategy A (GW-centric): FNR=100% for both protocols
    6.2 Strategy B (relay-side dst-binding): FNR≈0% for LM, FNR=100% for Mesh
    6.3 Why passive IDS fails for active relay (structural argument)

§7  Patch Asymmetry: E18
    7.1 HMAC on Hello closes LM (7/7 blocked)
    7.2 HMAC on NodeInfo fails for Mesh (7/7 bypassed)
    7.3 Root cause: control-plane auth ≠ data-plane field protection

§8  Correct Patch and Formal Verification
    8.1 AEAD AAD binding: tag = HMAC(from||to||next_hop||pkt_id||ct, psk)
    8.2 Tamarin patch model: meshtastic_patch.spthy
    8.3 L3 No_Relay_Takeover: PROVED

§9  Discussion
    9.1 Design guidelines for constrained-airtime mesh headers
    9.2 Generalization: RPL, 6LoWPAN, other compact routing formats
    9.3 Limitations: single-channel model, no multi-hop chain

§10 Related Work
    - LoRa/LoRaWAN security surveys
    - Formal routing protocol verification (Tamarin-based)
    - Meshtastic security research

§11 Conclusion
```

---

## Evidence Files

| Artifact | Path |
|---|---|
| Tamarin baseline | `phase2_tamarin_models/meshtastic/meshtastic_routing.spthy` |
| Tamarin patch | `phase2_tamarin_models/meshtastic/meshtastic_patch.spthy` |
| Patch verification log | `phase2_tamarin_models/meshtastic/results/meshtastic_patch_verification.log` |
| E16 firmware | `hardware_experiments/E16_meshtastic_nexthop/` |
| E16 results | `hardware_experiments/E16_meshtastic_nexthop/results/e16_results.md` |
| E17 firmware | `hardware_experiments/E17_ids_comparison/` |
| E17 results | `hardware_experiments/E17_ids_comparison/results/e17_results.md` |
| E18 firmware | `hardware_experiments/E18_patch_asymmetry/` |
| E18 results | `hardware_experiments/E18_patch_asymmetry/results/e18_results.md` |
| Board config | `hardware_experiments/meshtastic_eora_pi/variants/EBYTE_EoRa-PI/variant.h` |

---

## Open Items

- [ ] Write full LaTeX draft (§1–§11)
- [ ] Run Tamarin on baseline model with verbose proof output (for §3 table)
- [ ] Add quantitative overhead comparison: AEAD tag overhead on LoRa airtime
- [ ] Related work: search for any existing Meshtastic security papers
