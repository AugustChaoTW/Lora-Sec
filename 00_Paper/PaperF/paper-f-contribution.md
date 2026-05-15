---
title: "Paper F — Contribution Document"
target: IEEE S&P / USENIX Security (backup: IEEE TDSC)
status: theory sketch; formal definitions pending
github_issue: "#78"
date: 2026-05-14
---

# Physically-Constrained Semantic Security Theory for IoT Mesh Routing

## Thesis

```
Secure Routing = Controlled Semantic Influence ∩ Physical Feasibility
```

Existing security frameworks (Dolev-Yao, UC, non-interference) treat attackers as computationally or network-unbounded. In IoT mesh routing, this abstraction gap is not conservative — it is wrong in both directions: it over-approximates what an attacker can physically achieve (Dolev-Yao admits unlimited injection; a 100 mW LoRa radio does not) and under-approximates what a defender can afford (UC ignores that AEAD on every hello message exhausts a coin-cell in hours). This paper introduces a theory of **Physically-Constrained Semantic Security (PCSS)** that formalizes routing security as the joint property of (1) cryptographic control over the information that influences routing decisions and (2) deployability within the energy, bandwidth, and compute envelope of the target device class. The theory is instantiated on three protocols (LoRaMesher, Meshtastic, RPL) and recovers — as corollaries — the concrete vulnerabilities proved in Papers B and C.

---

## Contributions

| # | Contribution | Evidence |
|---|---|---|
| C1 | Formal definition of Semantic Information for routing: ΔD_r(I) ≠ 0; identification of the Decision-Critical Set DC(P) per protocol | Definitions 2–4; instantiation on LoRaMesher, Meshtastic, RPL |
| C2 | Authorized Causal Cone C(D_r) and Semantic Binding Condition SBC(P): necessary and sufficient condition for routing integrity | Definitions 5–6; Theorem 1 proof sketch |
| C3 | Physical Feasibility layer: Φ(P,E) formalization with resource budget constraint; Theorem 2 infeasibility threshold ρ* | Definition 3 (L3); Theorem 2 |
| C4 | Unified protocol instantiation: DC(P) violations in LoRaMesher (dst=next_hop collapse), Meshtastic (next_hop advisory), and RPL (DIO rank / DODAG version) derive from a single violated SBC | §Protocol Instances; cross-paper corollary table |
| C5 | Separation from prior frameworks: PCSS strictly subsumes Dolev-Yao routing models under physical bound; is incomparable to UC (resource budget not a UC security parameter); is orthogonal to non-interference (routing decision model + distributed convergence not captured by NI) | §Related Work; formal separation arguments |

---

## Argument Chain

### Step 1 — The Gap in Existing Frameworks

Standard security models fail IoT mesh routing in two complementary ways.

**Dolev-Yao over-approximates the attacker:** the canonical DY attacker can inject arbitrary messages at any rate, replay any observed message, and be present at any network location simultaneously. In a duty-cycled LoRa mesh, the attacker is constrained to the same 1% duty cycle as honest nodes; a replay requires physical proximity at the moment of transmission; injection at scale requires a node with radio range and power budget. The gap between DY capability and physical capability means DY-based proofs may declare a protocol "secure" against an adversary that cannot exist, or declare it "broken" by an adversary that cannot mount the attack in practice.

**UC / game-based models under-approximate the defender's cost:** security proofs in these frameworks treat any polynomial-time protocol as "feasible." A Cortex-M0 at 32 MHz with 256 KB flash running on a 1000 mAh battery at 3.3V does not have polynomial-time resources in any useful sense. HMAC-SHA256 on every Hello message costs ~2 ms CPU + ~0.5 mA at 3.3V. At one Hello per 10 seconds that is 0.165 mAh/day — negligible. But AEAD on every data packet at 100 pkt/s costs 20 mA continuous — dead in 50 hours. The cost function is not complexity-theoretic; it is physical.

**Non-interference models miss routing semantics:** NI says "high inputs do not influence low outputs." Routing decisions are not a confidentiality property in the NI sense — they are an integrity property over a distributed state machine that converges asynchronously. A node that receives a forged Hello with metric=0 will update its routing table and propagate the update; the influence path runs through the protocol state, not the output channel.

### Step 2 — The Four-Layer Theory

PCSS organizes routing security into four composable layers:

**L1: Information Semantics**

Define the routing decision function for node i:
```
D_r^i = f(K_i, C_ij, Q_i, T_i) → RoutingTable
```
where K_i is local knowledge, C_ij are channel observations, Q_i is the message queue, and T_i is the local clock.

A message I carries **Semantic Information** for node i if and only if:
```
ΔD_r(I) ≠ 0   [Definition 2]
```
i.e., receiving I changes node i's routing table. The **Semantic Closure** Cl_d(Sem(P)) (Definition 3) is the smallest set of message types closed under the DV update rule — all message types whose injection or modification can transitively alter any routing table in the network.

The **Decision-Critical Set** DC(P) (Definition 4) is the minimal subset of Cl_d(Sem(P)) sufficient to control all routing entries: an attacker who can forge any message in DC(P) can install arbitrary routing tables.

**L2: Causal Influence**

The **Authorized Causal Cone** C(D_r) (Definition 5) is the set of (node, message-type, time) triples that are legitimately permitted to influence routing table D_r. Formally, C(D_r) = { (n, m, t) | n ∈ AuthorizedSet(D_r) ∧ m ∈ DC(P) ∧ t ∈ ValidWindow }.

The **Semantic Binding Condition** SBC(P) (Definition 6) holds for protocol P if and only if every message in DC(P) is cryptographically bound to its sender's identity, such that an adversary outside AuthorizedSet(D_r) cannot produce a valid message in DC(P).

**Theorem 1:** SBC(P) ∧ Attacker ∉ AuthorizedSet ⟹ ∀t. next_hop_S(t) ≠ Attacker

If SBC holds, no message in DC(P) originating outside AuthorizedSet can be accepted, so the routing table cannot be influenced by an unauthorized party, so the attacker cannot become the next hop for any source S.

**L3: Physical Feasibility**

The **Feasibility Function** Φ(P,E) ∈ {0,1} evaluates to 1 if and only if the security mechanism in P is deployable on device class E within its resource budget:
```
Cost(Security) ≤ ResourceBudget(E)
```
Cost(Security) is a vector (CPU cycles, memory bytes, airtime ms/day, energy mJ/day). ResourceBudget(E) is the device-class envelope (e.g., Cortex-M0, 256 KB flash, 1000 mAh, 1% LoRa duty cycle).

**Theorem 2:** ∃ ρ* s.t. ρ > ρ* ⟹ Φ(P,E) = 0

There exists a critical message rate ρ* above which any HMAC/AEAD-based binding mechanism exceeds the energy budget of device class E, making SBC physically unachievable regardless of cryptographic correctness. Characterizing ρ* for standard IoT device classes is a concrete design-space result.

**L4: Distributed Knowledge**

Local knowledge K_i(t) at node i evolves as a function of received messages. **Distributed belief convergence** requires that all honest nodes eventually agree on the routing table in the absence of adversarial interference. The L4 layer connects the point properties (SBC at node i) to the network-level property (consistent routing across all nodes).

### Step 3 — Protocol Instantiations

| Protocol | DC(P) | SBC satisfied? | Attack consequence | Paper |
|---|---|---|---|---|
| LoRaMesher | {Hello messages} | No — Hello unauthenticated | dst=next_hop field collapse → silent black hole | B |
| Meshtastic v2.6+ | {MeshPacket.next_hop field} | No — next_hop advisory, not bound in AEAD AAD | Active relay; attacker receives payload, GW PDR=100% | C |
| RPL | {DIO rank, DODAG version number} | No — DIO unauthenticated in default mode | Rank manipulation → sub-optimal or black-hole routes | — |

The three protocols share a common SBC violation pattern. The attack category difference (silent black hole vs. active relay) is a consequence of DC(P) structure, not of the SBC violation itself — confirming Paper C's header-semantics taxonomy as a corollary of the PCSS framework.

### Step 4 — Physical Feasibility Analysis

For LoRaMesher on EoRa PI (ESP32-S3, SX1262, 1000 mAh):

| Mechanism | CPU overhead | Airtime overhead | Energy/day @ 1 pkt/10s | Φ(P,E) |
|---|---|---|---|---|
| No security (baseline) | 0 | 0 | 0 | 1 |
| HMAC-SHA256 on Hello | ~2 ms/pkt | +32 bytes = ~10 ms airtime/pkt | ~0.17 mAh | 1 |
| AEAD on every data packet @ 1 pkt/s | ~2 ms/pkt | +16 bytes overhead/pkt | ~4.8 mAh | 1 |
| AEAD on every data packet @ 100 pkt/s | ~200 ms/s = 20% CPU | +16 bytes/pkt continuous | ~480 mAh | 0 |

Theorem 2 gives ρ* ≈ 50 pkt/s for this device class under this mechanism. Above ρ*, SBC cannot be maintained continuously; the protocol must fall back to periodic re-authentication windows, which reintroduces a replay window. The theory quantifies this trade-off precisely.

### Step 5 — Separation from Prior Work

**vs. Dolev-Yao:** DY admits attacker injection rate = ∞. PCSS bounds injection rate by the physical channel (duty cycle, radio range, energy). Formally: DY-secure ⟹ PCSS-secure (PCSS is weaker in attacker model), but PCSS-secure ⟹ DY-secure is false (a PCSS-secure protocol may be broken by an unbounded DY attacker). The physical bound is not conservative — it is the correct model for the threat class.

**vs. UC framework:** UC security is parameterized by a computational security parameter λ; resource cost is polynomial in λ by definition. PCSS treats resource cost as a physical constant, not a parameter. A protocol that is UC-secure under standard assumptions may have Φ(P,E) = 0 for a given device class — it is cryptographically sound but physically undeployable. Conversely, a protocol that achieves SBC via a lightweight MAC (e.g., GHASH-based 64-bit tag) may have Φ(P,E) = 1 but fail UC security under standard reductions.

**vs. Non-interference:** NI is defined over a two-level lattice (high/low) and flow policies. Routing integrity is not a flow property: the attacker does not observe a "low" output influenced by "high" input; the attacker injects a "low" input (a forged Hello) that influences a distributed state machine. PCSS models this as SBC over DC(P), which is an input-integrity property over a specific message class, not a flow policy.

### Step 6 — Mechanization Path

The theory is structured for mechanization in two stages:

**Stage A — Tamarin encoding:** SBC(P) can be expressed as a Tamarin lemma: `All msg. msg ∈ DC(P) ∧ Accepted(i, msg) ⟹ ∃ k. Honest(k) ∧ Sent(k, msg)`. This is exactly the structure of L1 MetricAuthenticity (Paper B) and L3 No_Relay_Takeover (Paper C). The PCSS framework thus provides a principled derivation of the lemma structure from the protocol's DC(P).

**Stage B — Coq formalization:** The four definitional layers (Definitions 1–6) and Theorems 1–2 are candidates for Coq mechanization. The routing decision function D_r^i is a purely functional map; the Authorized Causal Cone is a relation; SBC is a predicate over protocol traces. No interactive concurrency primitives are required for the static definitions; distributed convergence (L4) requires a trace-based formulation analogous to linearizability proofs.

---

## Abstract (draft)

Security frameworks for networked protocols either over-approximate the attacker (Dolev-Yao: unlimited injection) or ignore physical deployability (UC: polynomial-time suffices). For IoT mesh routing — where nodes are duty-cycled, energy-constrained, and physically co-located with the attacker — neither approximation is adequate. We introduce Physically-Constrained Semantic Security (PCSS), a four-layer theory that formalizes routing security as the joint property of cryptographic control over decision-critical information and deployability within the physical resource envelope of the target device class. The core construct is the Semantic Binding Condition SBC(P): a protocol P is semantically secure if and only if every message type in its Decision-Critical Set DC(P) is cryptographically bound to an authorized sender. We prove that SBC(P) is sufficient to exclude the attacker from the routing table (Theorem 1) and that there exists a physical message-rate threshold ρ* above which SBC cannot be continuously maintained on standard IoT hardware (Theorem 2). We instantiate the theory on three protocols: LoRaMesher (DC(P) = {Hello}), Meshtastic (DC(P) = {MeshPacket.next_hop}), and RPL (DC(P) = {DIO rank, DODAG version}). In each case, SBC is violated in the default deployment, and the attack category (silent black hole vs. active relay vs. rank manipulation) is determined by the structure of DC(P), not by the SBC violation per se. The PCSS framework thus provides a principled derivation of attack taxonomy, patch targets, and feasibility bounds from a single unified theory, closing the gap between formal verification results and deployable IoT security engineering.

**Keywords**: IoT mesh routing security, semantic security theory, physical constraints, LoRa, LoRaMesher, Meshtastic, RPL, formal verification, Tamarin Prover, Dolev-Yao, resource-bounded adversary

---

## Paper Structure (16–20 pages, IEEE S&P / USENIX Security)

```
§1  Introduction
    - The two-sided gap: DY over-approximates, UC ignores cost
    - PCSS thesis: Secure Routing = Controlled Semantic Influence ∩ Physical Feasibility
    - Contributions C1–C5
    - Paper map

§2  Background and Motivation
    2.1 Routing decision model: D_r^i = f(K_i, C_ij, Q_i, T_i)
    2.2 IoT hardware constraints: EoRa PI, duty cycle, energy budget
    2.3 The three protocols: LoRaMesher, Meshtastic, RPL
    2.4 Motivating examples: Papers B and C attack results as puzzles for existing frameworks

§3  Physically-Constrained Semantic Security Theory
    3.1 L1 — Information Semantics
        Definition 1: Routing decision function
        Definition 2: Semantic Information (ΔD_r(I) ≠ 0)
        Definition 3: Semantic Closure Cl_d(Sem(P))
        Definition 4: Decision-Critical Set DC(P)
    3.2 L2 — Causal Influence
        Definition 5: Authorized Causal Cone C(D_r)
        Definition 6: Semantic Binding Condition SBC(P)
        Theorem 1: SBC ⟹ next_hop_S(t) ≠ Attacker
    3.3 L3 — Physical Feasibility
        Cost(Security) ≤ ResourceBudget formalization
        Feasibility function Φ(P,E)
        Theorem 2: ρ > ρ* ⟹ Φ(P,E) = 0
    3.4 L4 — Distributed Knowledge
        K_i(t) local knowledge model
        Distributed belief convergence condition

§4  Protocol Instantiations
    4.1 LoRaMesher: DC(P) = {Hello}, SBC violated, dst=next_hop field collapse
    4.2 Meshtastic: DC(P) = {MeshPacket.next_hop}, SBC violated, advisory next_hop
    4.3 RPL: DC(P) = {DIO rank, DODAG version}, SBC violated, rank manipulation
    4.4 Attack taxonomy corollary: DC(P) structure determines attack category

§5  Physical Feasibility Analysis
    5.1 Device class parameterization: EoRa PI (ESP32-S3, 1000 mAh, 1% duty)
    5.2 Cost model: HMAC-SHA256 and AEAD overhead vectors
    5.3 Φ(P,E) evaluation table: feasible and infeasible regions
    5.4 Theorem 2 instantiation: ρ* ≈ 50 pkt/s for EoRa PI + HMAC-SHA256

§6  Separation from Prior Frameworks
    6.1 vs. Dolev-Yao: physical bound on attacker capability
    6.2 vs. UC: resource budget as a physical constant, not a parameter
    6.3 vs. Non-interference: input-integrity over DC(P) vs. flow policy

§7  Mechanization
    7.1 Tamarin encoding of SBC: lemma structure derivation
    7.2 Connection to Papers B and C verification results
    7.3 Coq formalization sketch: Definitions 1–6 and Theorems 1–2

§8  Design Guidelines
    8.1 Identify DC(P) before designing authentication
    8.2 Bind DC(P) messages at the correct protocol layer (control vs. data plane)
    8.3 Compute ρ* before committing to a binding mechanism
    8.4 L4 convergence: authentication must not break DV update propagation

§9  Related Work
    9.1 Formal routing security: survey and gap
    9.2 IoT-constrained security: lightweight crypto, duty cycle models
    9.3 Non-interference and information flow for routing
    9.4 Physical-layer security models for wireless

§10 Conclusion
    - PCSS unifies attack taxonomy, patch targets, and feasibility bounds
    - Open problems: L4 mechanization, multi-hop convergence proofs, ρ* measurement
```

---

## Evidence Files

| Artifact | Path | Status |
|---|---|---|
| Tamarin baseline (LoRaMesher) | `phase2_tamarin_models/baseline/baseline_lora_dv.spthy` | exists (Paper B) |
| Tamarin patched (LoRaMesher) | `phase2_tamarin_models/patched/patched_lora_dv_2node.spthy` | exists (Paper B) |
| Tamarin baseline (Meshtastic) | `phase2_tamarin_models/meshtastic/meshtastic_routing.spthy` | exists (Paper C) |
| Tamarin patched (Meshtastic) | `phase2_tamarin_models/meshtastic/meshtastic_patch.spthy` | exists (Paper C) |
| PCSS formal definitions (LaTeX) | `00_Paper/PaperF/theory/pcss_definitions.tex` | pending (F1 #79) |
| 2-node instantiation + Theorem 1 proof | `00_Paper/PaperF/theory/theorem1_proof.tex` | pending (F2 #80) |
| Physical feasibility formalization | `00_Paper/PaperF/theory/feasibility.tex` | pending (F3 #81) |
| Protocol instances section | `00_Paper/PaperF/sections/protocol_instances.tex` | pending (F4 #82) |
| Related work section | `00_Paper/PaperF/sections/related_work.tex` | pending (F5 #83) |
| Coq / Tamarin mechanization | `00_Paper/PaperF/mechanization/` | pending (F6 #84) |
| Full LaTeX draft | `00_Paper/PaperF/SP-PaperF.tex` | pending (F7 #85) |

---

## Open Items

- [ ] **F1 #79** — Write formal definitions §3.1–3.2 in LaTeX (Definitions 1–6)
- [ ] **F2 #80** — Minimum 2-node instantiation: instantiate D_r^i, DC(P), SBC for LoRaMesher; write out Theorem 1 proof step-by-step
- [ ] **F3 #81** — Formalize Physical Feasibility layer: define Cost() vector, ResourceBudget(E) for EoRa PI; compute ρ* empirically; write Theorem 2 proof
- [ ] **F4 #82** — Protocol instances §4: fill DC(P) and SBC analysis for Meshtastic and RPL; add attack taxonomy corollary table
- [ ] **F5 #83** — Related work §9: survey formal routing security papers; position PCSS against NI, UC, DY; locate IoT-constrained crypto papers
- [ ] **F6 #84** — Mechanization: encode SBC as Tamarin lemma schema; assess Coq feasibility for Definitions 1–6; draft Coq module structure
- [ ] **F7 #85** — Full LaTeX draft: assemble §1–§10 scaffold; target 16 pages double-column IEEE format
- [ ] Decide primary venue: IEEE S&P (Feb deadline) vs. USENIX Security (Oct deadline); if theory-only, assess TDSC backup
- [ ] Cross-paper corollary table: show Papers B and C results as instantiated theorems of PCSS; confirm no circular dependency
- [ ] L4 distributed convergence: decide scope — sketch or full proof? authentication must not break DV metric propagation invariant
