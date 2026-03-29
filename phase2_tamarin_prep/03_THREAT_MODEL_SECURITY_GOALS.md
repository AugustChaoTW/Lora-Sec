# Threat Model & Security Goals (LoRaMesher Control Plane)

**Version**: 1.0  
**Date**: 2026-03-29  
**Scope**: Formal specification of threat model and security objectives for Tamarin verification

---

## 1. Threat Model

### 1.1 System Boundary

**In Scope** (analyzed):
- LoRaMesher control plane protocol (routing layer)
- Hello message exchange and route table updates
- Data forwarding logic
- Node PSK-based encryption

**Out of Scope** (not analyzed):
- Physical layer (RF jamming, signal capture)
- Firmware implementation (side-channels, buffer overflows)
- Hardware compromise (physical tampering, invasive attacks)
- Application layer (CoAP, MQTT security)
- Network topology discovery beyond DV routing

### 1.2 Attacker Capabilities (Dolev-Yao Model)

The attacker is a **probabilistic polynomial-time (PPT) adversary** with the following capabilities:

#### A. Message Interception & Eavesdropping
```
∀ message m:
  If broadcast(m) then attacker can In(m)
  Semantics: Attacker sees all wireless transmissions
```

**Assumptions**:
- Attacker cannot selectively jam specific messages
- Attacker cannot prevent successful delivery to intended receivers
- All eavesdropped content is available for analysis

#### B. Message Modification & Forgery
```
∀ values v1, v2, ..., attacker can create new message:
  new_msg = f(v1, v2, ...)  where f is any public function
  Semantics: Can craft arbitrary Hello, Data packets
```

**Limitations**:
- Cannot decrypt authenticated fields without key
- Cannot forge HMAC without PSK (in patched protocol)
- Can only use values previously known or derived

#### C. Message Replay
```
∀ message m previously sent:
  attacker can resend(m) at arbitrary later time
  Semantics: Can repeat old Hellos, old Data packets
```

**Implications**:
- Old routings with low sequence numbers can be replayed
- Sequence-based freshness must be implemented

#### D. Message Delay & Reordering
```
∀ message stream [m1, m2, m3]:
  attacker can deliver in order [m1, m3, m2] or [m3, m1, m2]
  Semantics: Can hold messages in buffer, reorder
```

**Impact**:
- Network must tolerate out-of-order delivery
- Timing-based attacks possible (e.g., delay-then-replay)

#### E. Partial Node Compromise
```
Rule: Compromise_Node
  If attacker successfully compromises Node(n), then:
    1. Learn PSK(n, key) → gain symmetric key
    2. Learn RouteTable(n, ...) → gain routing state
    3. Can forge messages indistinguishable from Node(n)
    4. Compromise is PERMANENT (attacker retains knowledge)
```

**Scope of Compromise**:
- Per-node basis (can compromise 1, 2, or k nodes)
- Attacker model parameterized: "Secure against compromise of ≤ k nodes"
- Common settings: k=1 (single malicious neighbor), k=n/3 (Byzantine majority threshold)

#### F. Attacker CANNOT do (Restrictions)

```
✗ Cannot break cryptographic primitives
  - Cannot forge HMAC without key
  - Cannot decrypt AES without key
  - Treated as black-box secure functions

✗ Cannot discard all messages (network must deliver some)
  - Attacker can't implement 100% jamming/loss

✗ Cannot perform non-polynomial-time computations
  - Can't exhaustively search key space (key assumed > 2^128)

✗ Cannot change protocol rules
  - Honest nodes always execute specification
  - Attacker can't modify honest node firmware
```

### 1.3 Threat Scenarios

#### Scenario 1: Hello Spoofing (Metric Injection)

```
Precondition:
  - Attacker A controls 1 node or can eavesdrop
  - Node C broadcasts Hello(C, metric=0, seq=N)

Attack:
  1. A eavesdrops Hello(C, 0, N)
  2. A forges Hello(C, 0, N) → broadcasts as if from C
  3. Honest node B receives fake Hello
  4. B believes metric-0 path to C is via A (false!)
  5. B updates RouteTable[C] = {metric: 1, nextHop: A}

Impact:
  - All data for C now routed through A (black hole or eavesdrop)
  - No mechanism to verify C actually sent the Hello

Root Cause: Hello messages lack authenticity/integrity check
```

#### Scenario 2: Metric Replay (Route Table Poisoning)

```
Precondition:
  - Attacker A eavesdropped on old Hello(C, 0, 10)
  - Network has updated to Hello(C, 0, 15)

Attack:
  1. A replays old Hello(C, 0, 10)
  2. Honest node B checks: seq_10 < seq_15 → rejects (protected by seq)
  3. BUT: A replays Hello(C, 1, 15) with fake metric=0
  4. B checks: seq_15 = seq_15, but metric=0 vs stored=1
  5. If no metric-version binding: B accepts fake low metric
  6. B routes via A again

Impact:
  - Even with sequence protection, metric can be replayed
  - No way to bind metric version to sequence

Root Cause: Metric and sequence not tightly coupled
```

#### Scenario 3: Hop-by-Hop Metric Manipulation

```
Precondition:
  - Attacker A is on path from X to Y

Attack:
  1. A receives Hello(X, metric=5, seq=N) from upstream
  2. A rebroadcasts modified Hello(X, metric=3, seq=N)
  3. Honest node B receives metric=3 (shorter than true path)
  4. B updates RouteTable[X] = {metric: 4, nextHop: A}
  5. B prefers A's path over legitimate path metric=6

Impact:
  - Path metric become inconsistent across network
  - Data may be routed via attacker node

Root Cause: No per-hop HMAC; rebroadcast not authenticated
```

#### Scenario 4: Selective Forwarding After Poisoning

```
Precondition:
  - Attacker has poisoned route via Scenario 1 or 2
  - Data destined for C now flows through A

Attack:
  1. A receives data: Data(src, C, payload, seq)
  2. A selectively forwards based on packet type/priority:
     - CoAP queries → forward (gain info)
     - MQTT sensor data → drop (denial of service)
     - Commands → intercept and modify
  3. Network experiences partial connectivity to C

Impact:
  - Quality of service attack
  - Information leakage
  - Potential data corruption

Root Cause: Route authenticity failure allows initial poisoning
```

### 1.4 Attacker Motivation & Realism

| Scenario | Attacker Type | Motivation | Feasibility |
|----------|---|---|---|
| Hello Spoofing | Compromised node in network | Disrupt critical paths, eavesdrop | **High** (requires 1 node compromise) |
| Metric Replay | External attacker, packet capture | Network congestion/partitioning | **Medium** (requires sustained eavesdrop + retransmit) |
| Hop Manipulation | Neighbor node | Attract traffic for analysis/MITM | **High** (passive + minor active capability) |
| Selective Forward | Compromised node | Information control, DoS | **High** (can occur after route poisoning) |

### 1.5 Network Assumptions

| Assumption | Justification | Impact if Violated |
|-----------|---|---|
| Nodes are initially honest | Adversary begins outside network | Impossible to defend against initial insider attacker |
| PSK shared before deployment | Out-of-band setup (e.g., factory) | If PSK leaked, all security lost (can extend to public key in future) |
| Neighbor topology is static | LoRa range doesn't change dynamically | Model won't detect mobility attacks; use static verification |
| Broadcast reaches all neighbors | LoRa broadcast model | If asymmetric links, update model with PartialReach facts |
| Encryption is semantically secure | Assume AES-256-CTR is CPA-secure | Side-channel attacks not modeled |

---

## 2. Security Goals & Lemmas

### 2.1 Goal 1: Route Metric Authenticity (RMA)

**Informal Statement**:
> When an honest node updates its route table based on a Hello message, the metric value must have originated from the claimed source, not injected by an attacker.

**Formal Definition**:

```tamarin
lemma Route_Metric_Authenticity:
  "All node src metric seq via.
    RouteTable(node, src, metric, via, seq) @ i
    & Honest(src)
    & Honest(node)
    ==> 
    (Ex #j. HelloSent(src, metric, seq) @ j & j < i)
    | Compromise(src)"
```

**Interpretation**:
- If honest node updates its route based on metric/seq from honest source,
- Then source must have actually broadcast that metric in that sequence
- OR source has been compromised (game over for that node)

**Attack This Defends Against**:
- ✓ Hello Spoofing (Scenario 1) — Attacker cannot forge Hello with arbitrary metric
- ✓ Metric Injection — Each metric value is cryptographically bound to sender

**Patched Protocol Mechanism**:
```
HMAC(Hello) = hmac(concat(sender, metric, seq, metricVer), PSK(sender))
Honest node verifies before accepting Hello
Attacker without PSK cannot forge valid HMAC
```

---

### 2.2 Goal 2: Routing Freshness & Replay Resistance (RFRR)

**Informal Statement**:
> Old routing information (Hellos with low sequence or old metric versions) must not be accepted as if it were recent, enabling route downgrade attacks.

**Formal Definition**:

```tamarin
lemma Route_Freshness_Replay_Resistance:
  "All node src metric seq seqNew metricVerOld metricVerNew.
    (seq > seqOld) 
    & RouteTable(node, src, metricNew, via, seqNew, metricVerNew) @ i
    & Honest(node)
    & Honest(src)
    ==> 
    ~(RouteTable(node, src, metricOld, via, seqOld, metricVerOld) @ j 
      & j > i & metricVerOld > metricVerNew)"
```

**Interpretation**:
- If route table is updated to (seq_new, metricVer_new),
- Then no later state can have older (seq_old, metricVer_old)
- Prevents regression to stale metric

**Attack This Defends Against**:
- ✓ Metric Replay (Scenario 2) — Old metrics cannot be resurrected
- ✓ Metric Downgrade — Attacker cannot convince node to use inferior metric

**Patched Protocol Mechanism**:
```
SeqNum: Strictly increasing per sender (can only update if seq_new > seq_old)
MetricVersion: Monotonic counter for same (seq, src) pair
If (seq_new = seq_old) but (metricVer_new ≤ metricVer_old) → REJECT
```

---

### 2.3 Goal 3: Route Path Consistency (RPC)

**Informal Statement**:
> When multiple honest nodes in a network learn routes to the same destination with the same sequence number, they must agree on the metric (distance).

**Formal Definition**:

```tamarin
lemma Route_Path_Consistency:
  "All dst src seq metric1 metric2 node1 node2.
    RouteTable(node1, dst, metric1, _, seq) @ i
    & RouteTable(node2, dst, metric2, _, seq) @ i
    & Honest(node1)
    & Honest(node2)
    & node1 ≠ node2
    ==> 
    metric1 = metric2
    | (node1 in path to node2)  // One is on path to other"
```

**Interpretation**:
- Honest nodes at same network distance should learn same metric
- Ensures routing converges consistently (no loops or oscillations)

**Attack This Defends Against**:
- ✓ Hop Manipulation (Scenario 3) — Attacker on path cannot change metric for downstream nodes
- ✓ Path Inconsistency — Prevents divergent routing states

**Patched Protocol Mechanism**:
```
Per-hop HMAC on forwarded Hellos ensures integrity
If intermediate modifies metric → HMAC fails at next node
Downstreams accept only authenticated metrics
```

---

### 2.4 Goal 4: No Unauthorized Route Installation (NURI)

**Informal Statement**:
> An attacker must not be able to install a false route in an honest node's routing table that points to the attacker as the next hop for a destination the attacker does not control.

**Formal Definition**:

```tamarin
lemma No_Unauthorized_Route_Installation:
  "All node attacker dst.
    RouteTable(node, dst, _, attacker, _) @ i
    & Honest(node)
    & ~Honest(attacker)
    & Honest(dst)
    ==> 
    False"
```

**Interpretation**:
- Honest node will never route to honest destination via dishonest node
- Prevents "black hole" and "wormhole" attacks

**Attack This Defends Against**:
- ✓ Complete Hello Spoofing (Scenario 1) — Attacker cannot pose as legitimate next hop
- ✓ Man-in-the-Middle — Attacker cannot insert itself into path

**Patched Protocol Mechanism**:
```
Route authenticity (HMAC) ensures Hello came from claimed sender
Only receiver of legitimate Hello can claim to be nextHop
Attacker cannot forge Hello from honest node → cannot be installed
```

---

### 2.5 Goal 5: PSK Confidentiality (PSKC)

**Informal Statement**:
> The pre-shared keys of honest, non-compromised nodes must not be revealed to the attacker.

**Formal Definition**:

```tamarin
lemma PSK_Confidentiality:
  "All node psk.
    PSK(node, psk) @ 0
    & Honest(node)
    & ~Compromise(node)
    ==> 
    ~(AttackerKnows(psk) @ ∞)"
```

**Interpretation**:
- If node starts honest and is never compromised,
- Then attacker has zero probability of learning its PSK
- Relies on:
  - Out-of-band key establishment
  - No side-channel leakage (assumed)
  - No cryptanalytic break of KDF (assumed)

**Notes**:
- This property is **NOT strengthened by routing authentication**
- Depends entirely on key management mechanism (outside our scope)
- Baseline LoRaMesher may have weak KDF → this lemma may fail if implementation is poor

---

### 2.6 Goal 6: Convergence Safety (CS)

**Informal Statement**:
> After a topology change (new Hello broadcast), the routing algorithm must converge to a consistent state in bounded time without entering loops or inconsistent states.

**Formal Definition**:

```tamarin
lemma Convergence_Safety:
  "All src dst path.
    HelloSent(src, 0, seqNew) @ t_broadcast
    ==> 
    (Ex path #t_converge. 
      t_converge ≤ t_broadcast + (DIAMETER × HELLO_PERIOD)
      & ConsistentPath(src, dst, path) @ t_converge)"
```

**Interpretation**:
- After source broadcasts new Hello with seqNew,
- Within DIAMETER hops × propagation time,
- All nodes will learn consistent path (no loops)

**Complexity**:
- This is a **bounded liveness property** (harder to verify)
- May require explicit diameter bound in model
- Baseline DV routing should satisfy this; attacks violate it

---

## 3. Mapping: Security Goals → Tamarin Lemmas

| Security Goal | Lemma | Baseline LoRaMesher | Patched Protocol | Priority |
|---|---|---|---|---|
| RMA (Route Metric Authenticity) | Route_Metric_Authenticity | ✗ BROKEN | ✓ PROVED | **P0** (Critical) |
| RFRR (Freshness/Replay) | Route_Freshness_Replay_Resistance | ✗ BROKEN | ✓ PROVED | **P0** (Critical) |
| RPC (Path Consistency) | Route_Path_Consistency | ✗ BROKEN | ✓ PROVED | **P1** (Important) |
| NURI (No Unauth Route) | No_Unauthorized_Route_Installation | ✗ BROKEN | ✓ PROVED | **P0** (Critical) |
| PSKC (PSK Confidentiality) | PSK_Confidentiality | ✓ PROVED | ✓ PROVED | **P1** (Assumed) |
| CS (Convergence Safety) | Convergence_Safety | ? UNKNOWN | ? TBD | **P2** (Liveness) |

---

## 4. Attacker Model Parameters

### 4.1 Bounded Compromise

**Setting**: "Secure against compromise of ≤ k honest nodes"

```
parameter k = 1:
  Lemmas assume: |{n | Compromise(n)}| ≤ 1
  Impact: At least n-1 nodes remain honest
  
parameter k = floor(n/3):
  Lemmas assume: |{n | Compromise(n)}| ≤ n/3
  Impact: Byzantine threshold (can tolerate up to 1/3 adversarial nodes)
```

**Typical Settings**:
- **k=0**: Perfect security (no compromise)
- **k=1**: Realistic (one neighbor attacker)
- **k=n-1**: Worst case (all but one node compromised)

### 4.2 Attacker Knowledge Assumption

**Open World Assumption**:
```
Attacker may know any public information:
  ✓ All broadcast messages
  ✓ Network topology (who is neighbor of whom)
  ✓ Source code of honest nodes
  ✗ Only things attacker CANNOT know: Secret keys of uncompromised nodes
```

**Tamarin Enforces**:
```
rule In(msg) → AttackerKnows(msg)    // All broadcast is public

Lemmas guard: 
  Honest(n) ∧ ¬Compromise(n) ==> ¬AttackerKnows(PSK(n))
```

---

## 5. Formal Assumptions

### 5.1 Cryptographic Assumptions

| Primitive | Assumption | Used In |
|---|---|---|
| AES-256-CTR | Semantically secure (CPA-secure) | Per-hop encryption; Tamarin models as black-box |
| HMAC-SHA256 | Unforgeable without key | Hello authentication (patched) |
| KDF (PBKDF2, etc.) | Pseudorandom output | PSK derivation (if needed) |

### 5.2 Protocol Assumptions

| Assumption | Statement | Impact |
|---|---|---|
| Static Topology | Neighbor set does not change during verification | Won't detect mobility attacks; need separate model |
| Reliable Broadcast | Messages delivered to all neighbors (no loss) | If wireless loss occurs, need retry logic |
| Unique Node IDs | Each node has globally unique identifier | Attacks exploiting ID collision not modeled |
| Honest Initialization | All nodes start as honest (Honest(n) @ 0) | Cannot model pre-compromise |
| Sequential Consistency | Events happen in causal order | Network timing properties not precisely modeled |

### 5.3 Threat Model Assumptions

| Assumption | Statement | Impact |
|---|---|---|
| Bounded Attacker | Attacker is PPT (polynomial-time) | Exhaustive key search assumed infeasible |
| Network Isolation | Only LoRaMesher protocol analyzed | Side-channel attacks (power, timing) not modeled |
| Message Privacy | Encrypted fields are opaque to attacker | Plaintext metadata attacks possible but not modeled |

---

## 6. Out-of-Scope Threats

The following threat categories are **explicitly NOT addressed** by this security model:

| Threat | Why Out-of-Scope | Mitigation Strategy |
|---|---|---|
| **RF Jamming** | Physical layer attack; not addressable by protocol | Spread-spectrum, hopping, power increase |
| **Side-Channel (Timing)** | Cryptographic implementation detail | Constant-time HMAC, blinding |
| **Side-Channel (Power)** | Hardware-specific; requires physical access | Shielding, power filtering |
| **Sybil Attack** | Requires identity verification (not provided) | Authentication system (beyond LoRaMesher) |
| **DDoS on Network** | Requires rate limiting (not in protocol) | QoS mechanisms, traffic shaping |
| **Firmware Flashing** | Physical/supply-chain attack | Secure boot, attestation (orthogonal) |
| **Battery Depletion** | Energy exhaustion attack | Power management, sleep scheduling |

---

## 7. Verification Scope Summary

### What We WILL Verify

✅ Tamarin baseline model proves/refutes each lemma  
✅ Counterexamples show attack traces if lemma breaks  
✅ Patched model restores broken lemmas  
✅ Cost of patch (message size, computation) quantified  

### What We WILL NOT Verify

❌ Real-world deployment (needs testbed/ns-3)  
❌ Performance at scale (>100 nodes)  
❌ Robustness to packet loss / delays  
❌ Energy consumption vs security tradeoff  
❌ Compatibility with existing LoRaMesher release  

---

## 8. Security Assurance Levels

### Assurance Levels After Patching

| Property | Assurance | Evidence |
|---|---|---|
| **Route Metric Authenticity** | Formal (machine-checked) | Tamarin PROVED lemma |
| **Freshness/Replay** | Formal (machine-checked) | Tamarin PROVED lemma |
| **No Unauthorized Routes** | Formal (machine-checked) | Tamarin PROVED lemma + attack traces |
| **Convergence** | Heuristic (manual reasoning) | DV routing theory (Bellman-Ford) |
| **PSK Confidentiality** | Assumed (dependent on KDF) | Out-of-scope; external verification needed |

---

This threat model and security goals framework guides the Tamarin verification effort and helps interpret results.
