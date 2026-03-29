# Protocol Abstraction Specification: LoRaMesher Distance-Vector Routing

**Version**: 1.0  
**Date**: 2026-03-29  
**Status**: Ready for Tamarin Modeling  
**Scope**: LoRaMesher Control Plane (Routing Layer)

---

## 1. Protocol Overview

### 1.1 High-Level Description

LoRaMesher implements a **Distance-Vector (DV) routing protocol** for multi-hop LoRa mesh networks. Each node:
1. Periodically broadcasts a **Hello** message containing its known routes
2. Maintains a **RouteTable** (node → {metric, nextHop, sequence})
3. Selects the **minimum-metric path** for forwarding
4. Uses **per-hop encryption** with shared PSK (no route-specific key derivation)

### 1.2 Key Simplifications from Implementation

| Aspect | Implementation Detail | Tamarin Model | Justification |
|--------|---|---|---|
| Neighborhood discovery | Link quality estimation (RSSI-based) | Static `Neighbor(i, j)` facts | Protocol-level security unaffected by PHY tuning |
| Metric calculation | Multi-factor (hop-count, quality, battery) | Abstract metric ∈ ℕ | Only order matters for DV correctness |
| Hello period | Configurable ~1-5 minutes | Event ordering (no absolute time) | Verification ignores concrete timing |
| PSK distribution | Out-of-band (pre-shared before deployment) | `PSK(node, key)` facts | Key exchange not analyzed; focus on control plane |
| Encryption | AES-256-CTR per hop | Symbolic `enc_PSK(msg)` | Assume cipher is secure; analyze routing layer |

---

## 2. Protocol Phases

### Phase 0: Initialization
```
Node setup:
  1. Load PSK (out-of-band, e.g., hardcoded or pre-provisioned)
  2. Initialize HelloSeq = 0, RouteTable = {}, LocalMetric = 0
  3. Join network by listening to incoming Hellos
```

**Tamarin representation**:
```tamarin
rule Node_Init:
  [ Fr(~node_id), Fr(~psk) ]
  --[ NodeInit(~node_id) ]->
  [ Node(~node_id, Honest), 
    PSK(~node_id, ~psk),
    HelloSeq(~node_id, '0'),
    RouteTable(~node_id, empty)
  ]
```

---

### Phase A: Hello Broadcast (Periodic)

**Purpose**: Announce this node's reachability and known routes to neighbors

**Message format (current LoRaMesher)**:
```
Hello {
  sender_id: uint16
  metric: uint8           // 0 = self
  seq_num: uint16
  routing_info: {address, metric}[]  // Known routes
}
```

**Tamarin message**:
```tamarin
Hello(sender, metric, seq, routing_table)
```

**Rules**:
```tamarin
rule Node_Hello_Broadcast:
  [ Node(n, role),
    HelloSeq(n, seq),
    PSK(n, psk),
    Fr(~new_seq)
  ]
  --[ HelloSent(n, 0, ~new_seq) ]->
  [ Out(Hello(n, 0, ~new_seq, <routeInfo>)),
    HelloSeq(n, ~new_seq)
  ]
```

---

### Phase B: Hello Reception & RouteTable Update (Bellman-Ford Update)

**Purpose**: Learn routes to distant nodes via intermediate hops

**Logic**:
```
Upon receiving Hello(src, metric_src, seq_src, routes):
  1. If seq_src ≤ stored_seq[src]:
       Reject (older sequence, prevent replay)
  2. new_metric = metric_src + 1 (one more hop)
  3. If new_metric < stored_metric[src]:
       Update RouteTable[src] = {metric: new_metric, nextHop: src_1hop_neighbor, seq: seq_src}
  4. Rebroadcast Hello with updated metric
```

**Tamarin rules**:
```tamarin
rule Route_Update_Direct:
  // Direct neighbor (1-hop) update
  [ In(Hello(src, 0, seq_new, routes)),
    Node(dst, role),
    Neighbor(dst, src),
    RouteTable(dst, src, _, old_seq),
    PSK(src, psk)
  ]
  --[ RouteUpdate(dst, src, 1, seq_new),
      RouteMetric(dst, src, 1) ]->
  [ RouteTable(dst, src, 1, seq_new)
  ]

rule Route_Update_Indirect:
  // Multi-hop update via intermediate
  [ In(Hello(src, metric_src, seq_src, routes)),
    In(From(via)),
    Node(dst, role),
    Neighbor(dst, via),
    RouteTable(dst, src, metric_old, seq_old)
  ]
  --[ 
      Condition(seq_src > seq_old  ∨  (seq_src = seq_old ∧ metric_src + 1 < metric_old))
  ]->
  [ RouteTable(dst, src, metric_src + 1, seq_src)
  ]
```

---

### Phase C: Data Forwarding

**Purpose**: Route data packets toward destination using learned routes

**Logic**:
```
Upon receiving Data(src, dst, payload):
  1. Lookup RouteTable[dst] → {metric, nextHop, seq}
  2. If no route found:
       Discard (no reachability)
  3. Encrypt payload with PSK_shared (per-hop encryption)
  4. Forward to nextHop
```

**Tamarin rule**:
```tamarin
rule Data_Forward:
  [ In(Data(src, dst, payload, seq_data)),
    Node(forwarder, role),
    RouteTable(forwarder, dst, metric, nextHop, seq_route),
    PSK(forwarder, psk)
  ]
  --[ Forward(forwarder, src, dst, nextHop, metric),
      DataFwd(forwarder, dst, seq_data) ]->
  [ Out(Encrypt(payload, psk, forwarder, nextHop))
  ]
```

---

### Phase D: Route Staleness & Metric Infinity (Implicit Timeout)

**Purpose**: Remove stale routes and prevent loops

**Tamarin simplification**: Mark routes as stale after N non-updates
```tamarin
rule Route_Timeout:
  [ RouteTable(node, dst, metric, nextHop, seq),
    !LastUpdate(node, dst) > TIMEOUT
  ]
  --[ RouteTimeout(node, dst) ]->
  [ RouteTable(node, dst, ∞, ⊥, seq)  // Mark unreachable
  ]
```

---

## 3. Attacker Model & Compromise

### 3.1 Dolev-Yao Attacker

The attacker can:
- **Eavesdrop**: Read all broadcast messages
- **Replay**: Resend old messages
- **Modify**: Change message contents
- **Delay**: Hold and reorder messages
- **Craft**: Create new messages using known values

**Tamarin representation**:
```tamarin
rule Attacker_Message_Input:
  [ In(m) ]
  -->
  [ AttackerKnows(m) ]
```

### 3.2 Node Compromise

```tamarin
rule Compromise_Node:
  [ Node(n, role),
    PSK(n, psk),
    RouteTable(n, ...)
  ]
  --[ Compromise(n) ]->
  [ Out(psk),
    Out(RouteTable(n, ...))
  ]
```

The attacker gains:
- Node's PSK
- Current RouteTable state
- Ability to forge Hello/Data messages indistinguishably

---

## 4. Cryptographic Abstractions

### 4.1 Symmetric Encryption (Per-Hop)

```tamarin
// Encrypt operation
functions: enc(plaintext, key) → ciphertext

// Decrypt (private, not modeled as explicit rule)
equations: dec(enc(plaintext, key), key) = plaintext
```

**Assumption**: Only party with `key` can decrypt.

### 4.2 HMAC (For Patched Protocol)

```tamarin
functions: hmac(message, key) → tag

// Verification (implicit in rule conditions)
// Attacker cannot forge without key knowledge
```

---

## 5. Protocol Artifacts & Invariants

### 5.1 Protocol Invariants (Baseline LoRaMesher)

1. **Metric Monotonicity**: metric_via_X > metric_via_direct (X ≠ direct)
2. **Sequence Ordering**: seq_new ≥ seq_old for same dst (monotonic increase)
3. **Route Table Bounded**: |RouteTable| ≤ |Network nodes|

### 5.2 Intended Security Properties (What Protocol CLAIMS)

1. **Key Confidentiality**: PSK not leaked to attackers
2. **Routing Correctness**: Data reaches intended destination
3. **Loop Prevention**: No circular routes

### 5.3 What Protocol LACKS (Vulnerabilities)

1. ❌ **No Route Authenticity**: Hello messages can be forged
2. ❌ **No Freshness Verification**: Old metrics can be replayed
3. ❌ **No Hop Authentication**: Forwarders can modify metrics
4. ❌ **No Metric Binding**: Sequence # and Metric not tied together

---

## 6. Patched Protocol (Enhancement Layer)

### 6.1 Add Route Authentication with HMAC

```tamarin
rule Hello_Broadcast_Patched:
  [ Node(n, role),
    PSK(n, psk),
    HelloSeq(n, seq)
  ]
  --[ HelloSent(n, 0, seq) ]->
  [ Out(Hello_Secure(n, 0, seq, hmac(concat(n, 0, seq), psk)))
  ]

rule Route_Update_Verify:
  [ In(Hello_Secure(src, metric_src, seq_src, hmac_tag)),
    PSK(src, psk_src),
    Verify(hmac_tag = hmac(concat(src, metric_src, seq_src), psk_src))
  ]
  --[ VerifyOK ]->
  [ // Accept and update
  ]
```

### 6.2 Add Metric Version Binding

```tamarin
rule Hello_With_MetricVersion:
  [ Node(n, role),
    MetricVersion(n, ver),
    PSK(n, psk)
  ]
  --[ HelloSent(n, ver) ]->
  [ Out(Hello_Versioned(n, 0, seq, ver, hmac(...))),
    MetricVersion(n, ver++)
  ]
```

---

## 7. Abstraction Limitations & Non-Modeled Aspects

| Aspect | Reason Not Modeled | Impact on Verification |
|--------|---|---|
| Physical layer jamming | Outside protocol scope; requires PHY analysis | Won't detect jamming-based DoS |
| Dynamic topology change | Assume static neighbor topology | Won't verify handling of mobility |
| Link quality variation | Metric treated as symbolic, not numerical | Won't catch metric underflow/overflow bugs |
| Precise time constraints | Event-ordering verification, not real-time | Won't guarantee latency bounds |
| Firmware side-channels | Cryptographic module assumed secure | Won't catch timing attacks on HMAC |

---

## 8. Protocol State Machine Summary

```
                    ┌──────────────────┐
                    │  Node Startup    │
                    └────────┬─────────┘
                             │ (init PSK, HelloSeq=0)
                             ▼
         ┌───────────────────────────────────────┐
         │  Periodic Hello Broadcast             │
         │  (every ~1-5 minutes)                 │
         │  HelloSeq++                           │
         └───────────┬───────────────────────────┘
                     │ Out(Hello)
                     │
         ┌───────────▼───────────────────────────┐
         │  Receive Hello from Neighbors         │
         │  Bellman-Ford Update:                 │
         │  metric_new = metric_src + 1          │
         └───────────┬───────────────────────────┘
                     │ Update RouteTable
                     │
         ┌───────────▼───────────────────────────┐
         │  Data Forwarding Loop                 │
         │  LookUp: RouteTable[dst]→nextHop     │
         │  Forward: encrypt & send              │
         └───────────┬───────────────────────────┘
                     │ Repeat
                     └─────────────┐
                                   │
                    ┌──────────────┴─────────────┐
                    │  Route Timeout             │
                    │  (no update in T seconds)  │
                    │  Mark metric = ∞           │
                    └────────────────────────────┘
```

---

## Appendix: Comparison with Meshtastic

| Feature | Meshtastic (Assumed in v1.0) | LoRaMesher (v1.1) | Impact |
|---------|---|---|---|
| Route discovery | On-demand RREQ/RREP flooding | Periodic Hello DV | ✓ Simplifies to stateless periodic updates |
| Key management | Multi-level (PSK + MK + KDF) | Single PSK only | ✓ Removes key derivation analysis, focuses on routing auth |
| Authentication | Partial (PKI for some) | None | ✗ Makes routing easier to attack |
| Rekey mechanism | Explicit RekeyBroadcast | None | ✓ Removes rekey-sync analysis |

This abstraction allows us to focus on **core routing vulnerability** without distraction from complex key management.
