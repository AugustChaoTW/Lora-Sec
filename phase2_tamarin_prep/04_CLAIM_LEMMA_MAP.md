# Claim ↔ Lemma ↔ Assumption Mapping for Tamarin Verification

**Version**: 1.0  
**Date**: 2026-03-29  
**Purpose**: Bridge between informal security claims, formal Tamarin lemmas, and underlying assumptions

---

## 1. Overview: Three-Layer Mapping

```
┌─────────────────────────────────────────────────────────┐
│  INFORMAL SECURITY CLAIM                                │
│  (What we want to achieve in English)                   │
├─────────────────────────────────────────────────────────┤
│                          ↓                              │
├─────────────────────────────────────────────────────────┤
│  FORMAL TAMARIN LEMMA                                   │
│  (What we prove/refute with Tamarin)                    │
├─────────────────────────────────────────────────────────┤
│                          ↓                              │
├─────────────────────────────────────────────────────────┤
│  UNDERLYING ASSUMPTIONS                                 │
│  (What must be true for lemma to be meaningful)         │
└─────────────────────────────────────────────────────────┘
```

This document maps each claim through all three layers.

---

## 2. Core Claims & Mappings

### 2.1 Claim #1: Route Metrics Are Authentic

#### Informal Claim
> When an honest node A learns a route to destination D with metric M and sequence number S, that metric must have originated from D (or intermediate node acting on D's behalf), not injected by an arbitrary attacker.

#### Formal Tamarin Lemma

```tamarin
// Baseline (LoRaMesher) - Expected to BREAK
lemma Route_Metric_Authenticity_Baseline:
  "All node dst src metric seq via.
    RouteTable(node, dst, metric, via, seq) @ i
    & Honest(dst)
    & Honest(node)
    & node ≠ dst
    ==> 
    (Ex #j. HelloSent(src, metric, seq) @ j & j < i & SamePath(src, dst, via))
    | Honest(via) = ⊥"
    // Intuitively: If node learns route to honest dst, either:
    // - Honest src broadcast this metric (path authenticity), OR
    // - Intermediate via is dishonest (path can't be verified)

// Patched LoRaMesher - Expected to PROVE
lemma Route_Metric_Authenticity_Patched:
  "All node dst src metric seq via hmac_tag.
    In(Hello_Auth(src, metric, seq, hmac_tag)) @ i
    & Verify_HMAC(hmac_tag, src, metric, seq, PSK(src)) @ i
    & RouteTable(node, dst, metric, via, seq) @ j
    & j > i
    & Honest(dst)
    & Honest(node)
    & node ≠ dst
    ==> 
    (Ex #k. HelloSent(src, metric, seq) @ k & k < i)"
    // With HMAC: No honest node updates route unless Hello is authentically signed
```

#### Mapping to Implementation

| Implementation Aspect | Baseline LoRaMesher | Patched Protocol | Verification |
|---|---|---|---|
| **Hello Format** | `Hello(id, metric, seq, routes)` | `Hello_Auth(id, metric, seq, ver, hmac)` | HMAC field added |
| **Verification Step** | None (blindly trust metric) | `HMAC(concat(...), PSK) == tag` | Explicit check before accept |
| **RouteTable Update** | Immediate on seq check | Only if HMAC verifies | Guards `RouteTable` modification |
| **Attacker Capability** | Can forge any metric | Cannot forge metric+seq+HMAC combo | Bounded by cryptography |

#### Underlying Assumptions

| Assumption | Statement | Justification | Risk |
|---|---|---|---|
| **HMAC Unforgeability** | Attacker without PSK cannot produce valid HMAC | Standard cryptographic assumption (SHA256-based HMAC secure against forgery) | **Low** — standard assumption |
| **PSK Secrecy** | Honest nodes' PSKs are not leaked to attacker (except via Compromise rule) | Key management is out-of-band (pre-deployment) | **Medium** — depends on deployment practice |
| **Authentic Message Reception** | Honest node receives messages from In() facts (as broadcast by sender) | Dolev-Yao attacker controls network but cannot intercept before reception | **Low** — inherent to DY model |
| **Deterministic Verification** | `Verify_HMAC` rule succeeds iff HMAC is correct | Cryptographic verification is deterministic and reliable | **Low** — cryptographic primitive |

#### Counterexample (Baseline - Expected)

```
Attack Trace (Baseline):
  1. Honest node C broadcasts: Hello(C, 0, seq=1, routes)
  2. Attacker A eavesdrops: In(Hello(C, 0, seq=1, routes))
  3. Attacker A forges: Out(Hello(C, 0, seq=1, <modified_routes>))
  4. Honest node B receives A's forged message
  5. B updates: RouteTable(B, C, 1, A, seq=1)
  6. BROKEN: Metric=1 to C via A, but A did not send Hello with that metric
  
Tamarin finds: Counterexample violates Lemma_RouteMetricAuthenticity_Baseline
Output: Attack trace with ~10-15 steps
```

#### Verification Status

| Protocol | Expected Result | Confidence |
|---|---|---|
| **Baseline LoRaMesher** | ✗ **BROKEN** (counterexample exists) | **High** (inherent design flaw) |
| **Patched with HMAC** | ✓ **PROVED** (no counterexample) | **High** (standard authentication pattern) |

---

### 2.2 Claim #2: Old Routes Cannot Be Replayed

#### Informal Claim
> Once a node updates its route to destination D with a newer sequence number S_new, the node must not accept older sequence numbers S_old (where S_old < S_new) and revert to stale metrics, even if an attacker replays the old Hello.

#### Formal Tamarin Lemma

```tamarin
// Baseline - Expected to BREAK
lemma Route_Freshness_Baseline:
  "All node dst metric_old metric_new seq_old seq_new via.
    RouteTable(node, dst, metric_old, via, seq_old) @ i
    & seq_old < seq_new
    & RouteTable(node, dst, metric_new, via, seq_new) @ j
    & j > i
    & Honest(node)
    & Honest(dst)
    ==> 
    ~(In(Hello(dst, metric_old, seq_old, _)) @ k & k > j & metric_old < metric_new)"
    // Intent: After update to seq_new, node should not accept seq_old
    // But without freshness check, it might accept if metric looks better

// Patched - Expected to PROVE
lemma Route_Freshness_Patched:
  "All node dst metric_old metric_new seq_old seq_new via metricVer.
    RouteTable(node, dst, metric_new, via, seq_new, metricVer) @ j
    & Honest(node)
    ==> 
    ~(AcceptRoute(node, dst, metric_old, seq_old, metricVer') @ k 
      & (seq_old < seq_new | (seq_old = seq_new & metricVer' < metricVer)))"
    // With metricVer binding: Cannot accept inferior metric at same seq
```

#### Mapping to Implementation

| Aspect | Baseline | Patched | Verification |
|---|---|---|---|
| **Freshness Check** | `if (seq_new > seq_old) accept else reject` | Same, but also check `metricVer` | Two-part check |
| **Metric Version** | Not tracked | `RouteTable(..., metricVer)` incremented per metric update | State tracking |
| **Rebroadcast Logic** | Forwards any newer seq, any metric | Rejects if `(seq=old AND metricVer<new)` | Guard on rebroadcast |

#### Underlying Assumptions

| Assumption | Statement | Justification | Risk |
|---|---|---|---|
| **Sequence Monotonicity** | HelloSeq counter always increases | Nodes initialize seq=0 and increment each broadcast | **Low** — protocol design |
| **MetricVersion Monotonicity** | MetricVersion counter always increases for same (dst, seq) | Enforced in patched rule: `MetricVersion(n, dst, ver++)`  | **Low** — rule-based |
| **Clock not exploitable** | Sequence numbers don't wrap/overflow within verification time | Assume seq is 16-bit (or larger); verification time << 2^16 broadcasts | **Medium** — long-running networks vulnerable to seq wrap |

#### Counterexample (Baseline - Expected)

```
Attack Trace (Baseline):
  1. Node C broadcasts: Hello(C, 0, seq=100)
  2. Node B learns: RouteTable(B, C, 1, C, seq=100) 
  3. Time passes... C broadcasts: Hello(C, 0, seq=150)
  4. Node B learns: RouteTable(B, C, 1, C, seq=150)
  5. Attacker replays old: Hello(C, 0, seq=100)
  6. B checks: seq_old=100 < seq_new=150 → rejects ✓
  
  HOWEVER (attack variant):
  5. Attacker forges: Hello(C, 1, seq=150) with metric=0 (false!)
  6. B checks: seq=150 is current... but metric=0 vs stored=1?
  7. WITHOUT metricVer binding, B might accept metric=0
  8. B updates: RouteTable(B, C, 0, attacker, seq=150) ← FALSE route!
  
Tamarin finds: Counterexample where old metric at new seq is accepted
```

#### Verification Status

| Protocol | Expected Result | Confidence |
|---|---|---|
| **Baseline LoRaMesher** | ✗ **BROKEN** (metric can be downgraded at same seq) | **High** |
| **Patched with MetricVersion** | ✓ **PROVED** (cannot downgrade metric at same seq) | **High** |

---

### 2.3 Claim #3: Routes Converge Consistently

#### Informal Claim
> All honest nodes in the network, after receiving updates from the same source with the same sequence number, should agree on the metric (distance) to that source, preventing loops and inconsistent routing.

#### Formal Tamarin Lemma

```tamarin
// Baseline - Expected to BREAK
lemma Route_Consistency_Baseline:
  "All dst seq metric1 metric2 node1 node2.
    RouteTable(node1, dst, metric1, _, seq) @ i
    & RouteTable(node2, dst, metric2, _, seq) @ i
    & Honest(node1)
    & Honest(node2)
    & node1 ≠ node2
    & network_hops(node1, node2) = h
    ==> 
    abs(metric1 - metric2) ≤ h"  // Metrics should be close in distance
    // Expected to break: Attacker manipulates metrics for different nodes

// Patched - Expected to PROVE
lemma Route_Consistency_Patched:
  "All dst seq metric1 metric2 node1 node2 hmac1 hmac2.
    In(Hello_Auth(dst, metric1, seq, hmac1)) @ i
    & Verify_HMAC(hmac1, dst, metric1, seq, PSK(dst)) @ i
    & RouteTable(node1, dst, metric1', _, seq) @ j
    & j > i
    & 
    In(Hello_Auth(dst, metric2, seq, hmac2)) @ i'
    & Verify_HMAC(hmac2, dst, metric2, seq, PSK(dst)) @ i'
    & RouteTable(node2, dst, metric2', _, seq) @ j'
    & j' > i'
    & 
    Honest(node1)
    & Honest(node2)
    & Honest(dst)
    ==> 
    metric1' = metric1  // Both nodes learn same authentic metric
    & metric2' = metric2
    & metric1' = metric2' / abs(hop_distance) ∈ {-1, 0, +1}"
    // With HMAC: All nodes receive same Hello; metrics consistent up to topology
```

#### Mapping to Implementation

| Aspect | Baseline | Patched | Impact |
|---|---|---|---|
| **Hop-by-Hop Modification** | Any node can change metric freely | HMAC on every Hello prevents modification | Consistency guaranteed |
| **Convergence Time** | Unbounded (attacker can prevent) | Bounded by DV diameter | Formal property |

#### Underlying Assumptions

| Assumption | Statement | Justification | Risk |
|---|---|---|---|
| **Static Network** | Topology does not change (no node joins/leaves) | DV routing converges on fixed topology; mobility not modeled | **Medium** — real networks have mobility |
| **No Selective Loss** | All broadcasts reach all neighbors (or none) | Attacker cannot jam selectively; either broadcasts succeed or all fail | **Medium** — realistic links may have asymmetry |
| **Bounded Diameter** | Network has maximum diameter D (all paths ≤ D hops) | Assume mesh is tree or low-cycle | **Low** — can verify on bounded topology |

#### Verification Status

| Protocol | Expected Result | Confidence |
|---|---|---|
| **Baseline LoRaMesher** | ✗ **BROKEN** (hop-by-hop manipulation) | **High** |
| **Patched with HMAC** | ✓ **PROVED** (authentication prevents modification) | **High** |

---

### 2.4 Claim #4: No Unauthorized Routes Installed

#### Informal Claim
> An honest node must never route data to an honest destination through a dishonest node, if that dishonest node is not the legitimate next-hop announced by the destination.

#### Formal Tamarin Lemma

```tamarin
// Baseline - Expected to BREAK
lemma No_Unauthorized_Route_Installation_Baseline:
  "All node attacker dst.
    RouteTable(node, dst, _, attacker, _) @ i
    & Honest(node)
    & ~Honest(attacker)
    & Honest(dst)
    ==> 
    False"
    // Claims: Honest node will never route to honest dst via dishonest intermediate
    // Expected to break: Attacker can forge Hello claiming low metric to dst

// Patched - Expected to PROVE
lemma No_Unauthorized_Route_Installation_Patched:
  "All node attacker dst.
    RouteTable(node, dst, _, attacker, _) @ i
    & Honest(node)
    & ~Honest(attacker)
    & Honest(dst)
    & (ForAll #j. In(Hello_Auth(dst, metric, seq, hmac)) @ j & j < i
                  ==> Verify_HMAC(hmac, dst, _, _, PSK(dst)) fails)
    ==> 
    False"
    // With HMAC: Only way to get route to dst is via authenticated Hello from dst
    // Attacker without dst's PSK cannot forge Hello
```

#### Mapping to Implementation

| Aspect | Baseline | Patched |
|---|---|---|
| **Hello Forgery** | Attacker can claim "I have metric=0 to dst" | Attacker cannot sign with PSK(dst) |
| **Route Installation** | Blind acceptance of low metric | Only authenticated Hello accepted |

#### Underlying Assumptions

| Assumption | Statement | Justification | Risk |
|---|---|---|---|
| **Exclusive PSK** | Each node's PSK known only to itself and key provider | Key is not shared among neighbors | **Medium** — depends on key management |
| **Attacker Not Key Provider** | Attacker did not participate in initial key setup | Attacker joins network after keys distributed | **High** — fundamental threat assumption |

#### Verification Status

| Protocol | Expected Result | Confidence |
|---|---|---|
| **Baseline LoRaMesher** | ✗ **BROKEN** (any attacker can inject route) | **High** |
| **Patched with HMAC** | ✓ **PROVED** (only authenticated routes accepted) | **High** |

---

### 2.5 Claim #5: PSK Remains Secret (Cryptographic Assumption)

#### Informal Claim
> The pre-shared cryptographic keys of non-compromised honest nodes must not be discovered by the attacker through passive or active network attacks.

#### Formal Tamarin Lemma

```tamarin
// Baseline & Patched (should be identical)
lemma PSK_Confidentiality:
  "All node psk.
    PSK(node, psk) @ 0
    & Honest(node)
    & ~Compromise(node)
    ==> 
    ~AttackerKnows(psk) @ ∞"
```

#### Mapping to Implementation

| Aspect | Baseline | Patched | Notes |
|---|---|---|---|
| **Key Transmission** | Static PSK in code / firmware | Same | Keys not transmitted (out-of-band) |
| **Key Derivation** | KDF applied to PSK | Same | KDF security assumed |
| **Key Usage** | Encryption, HMAC | Enhanced (HMAC) | More usage doesn't strengthen secrecy |

#### Underlying Assumptions

| Assumption | Statement | Justification | Risk |
|---|---|---|---|
| **KDF Security** | Key derivation function produces pseudorandom output | PBKDF2, HKDF are standard PRFs | **Low** — standard crypto |
| **No Side-Channels** | Attacker cannot extract keys via timing/power analysis | Cryptographic modules assumed constant-time | **Medium** — implementation detail |
| **No Firmware Leak** | Keys not accessible in plaintext memory | Firmware protection mechanisms in place | **High** — depends on hardware |
| **Dolev-Yao Model** | Network-only attacker (no physical access) | Attacker is outside the network | **Critical** — threat model boundary |

#### Verification Status

| Protocol | Expected Result | Confidence |
|---|---|---|
| **Baseline & Patched** | ✓ **PROVED** (Dolev-Yao model, no key leak in protocol) | **High** |
| **Real Implementation** | ❓ **UNKNOWN** (depends on KDF quality, side-channel resistance) | **Low** — needs implementation testing |

---

## 3. Assumption Summary & Risk Matrix

### 3.1 Critical Assumptions (Must Hold for Lemmas to be Meaningful)

| Assumption | Category | Risk Level | Mitigation |
|---|---|---|---|
| **HMAC Unforgeability** | Cryptographic | **Low** | Standard assumption; verified empirically |
| **Sequence Monotonicity** | Protocol Design | **Low** | Enforced by rules; guaranteed by design |
| **Static Topology** | Network Model | **Medium** | Valid for snapshot verification; extend for dynamic |
| **Dolev-Yao Network** | Threat Model | **Low** | Standard assumption in formal verification |
| **Honest Nodes Execute Spec** | Implementation | **High** | Firmware must match specification exactly |

### 3.2 Assumption Validation Strategy

```
For each critical assumption:

  1. IDENTIFY where assumption is used in lemma
  2. CHECK if assumption is enforced by Tamarin rules
  3. DOCUMENT what happens if assumption is violated
  4. PLAN real-world validation (testing, deployment)
```

**Example**: Sequence Monotonicity
- **Used in**: Route_Freshness lemma
- **Enforced by**: HelloSeq rule (seq++ on each broadcast)
- **Violation**: If nodes wrap sequence or reset seq, lemma breaks
- **Real-world test**: Measure seq wrap time in deployment (months/years)

---

## 4. Lemma Checklist for Tamarin Implementation

### Pre-Verification Checklist

- [ ] **L1_RouteMetricAuthenticity_Baseline** — Baseline model, expect BROKEN
  - [ ] Define `HelloSent` action for metric broadcast
  - [ ] Define `RouteTable` fact structure
  - [ ] Write lemma with all guards: Honest(dst), Honest(node), node≠dst
  - [ ] Run `tamarin-prover baseline.spthy --prove L1_...`
  - [ ] Document counterexample trace (~15 steps expected)

- [ ] **L2_RouteFreshness_Baseline** — Baseline model, expect BROKEN
  - [ ] Track `LastSeq` state fact
  - [ ] Write freshness check condition
  - [ ] Run verification
  - [ ] Analyze counterexample (metric downgrade at same seq)

- [ ] **L3_RouteConsistency_Baseline** — Baseline model, expect BROKEN
  - [ ] Define multi-node route table queries
  - [ ] State expected metric consistency bound
  - [ ] Run verification
  - [ ] Expect: Attacker can cause inconsistency via hop-by-hop modification

- [ ] **L4_NoUnauthorizedRoute_Baseline** — Baseline model, expect BROKEN
  - [ ] Query: `RouteTable(..., attacker, ...)` where attacker is dishonest
  - [ ] Guard: only honest nodes, honest destinations
  - [ ] Run verification
  - [ ] Expect: Counterexample shows attacker installing false route

- [ ] **L5_PSKConfidentiality_Baseline** — Baseline model, expect PROVED
  - [ ] Guard: `Honest(node) ∧ ¬Compromise(node)`
  - [ ] Query: `AttackerKnows(PSK(node))`
  - [ ] Run verification (should pass without finding counterexample)

---

### Post-Patch Verification Checklist

- [ ] **L1_RouteMetricAuthenticity_Patched** — Patched model, expect PROVED
  - [ ] Add HMAC rules for Hello_Auth message
  - [ ] Add Verify_HMAC guard to RouteTable update rule
  - [ ] Run verification
  - [ ] Expect: PROVED (no counterexample found)

- [ ] **L2_RouteFreshness_Patched** — Patched model, expect PROVED
  - [ ] Track `MetricVersion` in RouteTable
  - [ ] Add rule: reject if metricVer_new ≤ metricVer_old at same seq
  - [ ] Run verification
  - [ ] Expect: PROVED

- [ ] **L3_RouteConsistency_Patched** — Patched model, expect PROVED
  - [ ] Add HMAC to Hello; all nodes receive same authenticated metric
  - [ ] Consistency follows from authentication
  - [ ] Run verification
  - [ ] Expect: PROVED

- [ ] **L4_NoUnauthorizedRoute_Patched** — Patched model, expect PROVED
  - [ ] Only way to route to D is via Hello_Auth(D, ...)
  - [ ] Attacker cannot forge without PSK(D)
  - [ ] Run verification
  - [ ] Expect: PROVED

- [ ] **L5_PSKConfidentiality_Patched** — Patched model, expect PROVED
  - [ ] Same as baseline (no change in PSK handling)
  - [ ] Should still prove

---

## 5. Interpretation Guide: Reading Tamarin Results

### When Lemma is PROVED

```
Output: 
  All traces are valid.
  
Interpretation:
  ✓ For all possible execution sequences, the lemma holds
  ✓ No counterexample found (attack does not exist)
  ✓ Security property guaranteed (under stated assumptions)
  
Example:
  [+] 1427 goals processed by SAPIC
  [+] 0 goals remaining
  Summary: All goals derived from lemma proven
```

### When Lemma is BROKEN (Counterexample Found)

```
Output:
  Counterexample found:
  1: rule_1 action @ 1
  2: rule_2 action @ 2
  ...
  15: Lemma_violated @ 15

Interpretation:
  ✗ An execution trace exists where lemma is violated
  ✗ Attack/vulnerability found
  ✗ Patch needed, or assumptions too weak
  
Example:
  Attack Trace: 
    Attacker -> Out(forged Hello)
    Honest -> In(forged Hello)
    Honest -> RouteTable(via attacker)
    Lemma violated: No_Unauthorized_Route
```

---

## 6. Tamarin Script Template

```tamarin
theory LoRaMesher_Baseline

begin

// ============= TYPES & FUNCTIONS =============
type NodeID = nat
type Metric = nat
type SeqNum = nat
type PSK = value

// ============= FACTS =============
// Node state
Node(NodeID, role: {honest, attacker})
Honest(NodeID)
PSK(NodeID, PSK)
Compromise(NodeID)

// Routing state
HelloSeq(NodeID, SeqNum)
RouteTable(NodeID, NodeID, Metric, NodeID, SeqNum)  // owner, dst, metric, nextHop, seq

// Topology
Neighbor(NodeID, NodeID)

// Attacker knowledge
AttackerKnows(value)

// ============= RULES =============
rule Node_Init:
  [ Fr(~id), Fr(~psk) ]
  --[ NodeInit(~id) ]->
  [ Node(~id, honest),
    Honest(~id),
    PSK(~id, ~psk),
    HelloSeq(~id, '0'),
    RouteTable(~id, empty)
  ]

rule Hello_Broadcast:
  [ Node(n, honest),
    HelloSeq(n, seq),
    Fr(~newSeq)
  ]
  --[ HelloSent(n, metric=0, ~newSeq) ]->
  [ Out(Hello(n, 0, ~newSeq, <routes>)),
    HelloSeq(n, ~newSeq)
  ]

rule Route_Update:
  [ In(Hello(src, metric_src, seq_src, routes)),
    Node(dst, honest),
    Neighbor(dst, src),
    RouteTable(dst, src, _, old_seq)
  ]
  --[ 
      Condition(seq_src > old_seq),
      RouteUpdate(dst, src, metric_src + 1, seq_src)
  ]->
  [ RouteTable(dst, src, metric_src + 1, src, seq_src)
  ]

// ... more rules ...

// ============= LEMMAS =============
lemma Route_Metric_Authenticity_Baseline:
  "All node dst metric seq.
    RouteTable(node, dst, metric, _, seq) @ i
    & Honest(dst)
    & Honest(node)
    ==> 
    (Ex #j. HelloSent(dst, metric, seq) @ j & j < i)
    | Compromise(dst)"

end
```

---

This mapping ensures that every Tamarin lemma has:
1. Clear informal justification (Claim)
2. Precise formal specification (Lemma)
3. Documented assumptions (Underpinnings)
4. Expected verification outcome (PROVED / BROKEN)

This supports both verification execution and result interpretation.
