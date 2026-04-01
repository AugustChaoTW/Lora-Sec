# Week 3: MetricVersion Dynamic Increment Implementation Plan

**Date**: 2026-03-30  
**Purpose**: Implement dynamic MetricVersion increment logic to prevent metric replay attacks  
**Estimate**: 45-90 minutes  
**Target**: Issue #2 Phase 2.5 (Model refinement for L2 freshness)

---

## Executive Summary

**Current Issue**: MetricVersion static ('v0')
- ❌ Cannot prevent replay of metrics at same sequence number
- ❌ L2_RouteFreshness fails when attacker replays old metric after seq update
- ✅ Fix: Dynamic version increment when metric changes

**Solution Approach**:
- Track current MetricVersion per (node, destination) pair
- Increment version when metric changes for same destination
- Bind HMAC to version: prevents downgrade attacks even with same seq

**Expected Impact**:
- ✅ L2_RouteFreshness will verify (no replay at same seq)
- ✅ Prevents metric downgrade within same epoch
- ⏱️ 2-5 min additional verification time

---

## Technical Background: Metric Freshness Problem

### Attack Scenario (Current Vulnerability)

```
Timeline:
T0: Node A broadcasts Hello(A, metric=0, seq=1, v0)
T1: Node B accepts route(A): metric=0, seq=1, v0

T2: Node A broadcasts Hello(A, metric=1, seq=2, v0)  [network update]
T3: Attacker replays OLD Hello(A, metric=0, seq=1, v0)

T4: Node C receives REPLAYED Hello
    - seq=1 (outdated)
    - metric=0 (old value)
    - BUT with same v0 as current route
    - Result: Route downgrade despite seq update
```

### Solution: Version Binding

```
Timeline (With MetricVersion):
T0: Node A broadcasts Hello(A, metric=0, seq=1, v1)
T1: Node B accepts route(A): metric=0, seq=1, v1

T2: Node A broadcasts Hello(A, metric=1, seq=2, v2)  [INCREMENT VERSION]
T3: Attacker replays OLD Hello(A, metric=0, seq=1, v1)

T4: Node C receives REPLAYED Hello
    - seq=1 (outdated)
    - v1 (outdated version)
    - Current route has v2
    - Result: Discarded due to version mismatch ✓
```

---

## Implementation Strategy (7 Modifications)

### Strategy Overview

1. **Per-Destination Tracking**: `!MetricVersion(n, dst, ver)` tracks current version for each (source, dest) pair
2. **Increment Trigger**: When metric for destination changes, increment version
3. **Version Binding**: HMAC includes version, prevents downgrade attacks
4. **Backward Compatibility**: Old v0 messages still accepted if seq valid (allow one epoch transition)

---

## Modification Details

### Modification 1: Node Initialization - Initialize Per-Destination Versions

**File**: `patched/patched_lora_dv.spthy` (Node_Init rule)  
**Current Code**:
```tamarin
rule Node_Init:
  [ Fr(~id), Fr(~psk), Fr(~seq) ]
  --[ NodeInit(~id), HonestNode(~id), PSKInit(~id,~psk) ]->
  [ !Node(~id,'honest'),
    !Honest(~id),
    !PSK(~id, ~id, ~psk),
    !HelloSeq(~id,~seq),
    !MetricVersion(~id,~id,'v0'),       // CURRENT: Single global version
    !Neighbor(~id,~id)
  ]
```

**Change**: Already initialized with per-node version. Need to add per-destination tracking.

**New Rule 1A**: Initialize neighbor-specific versions
```tamarin
// NEW RULE: Initialize versions for each neighbor
rule MetricVersion_Init_Neighbors:
  [ !Node(n, 'honest'), !Neighbor(n, neighbor) ]
  --[ MetricVersionInitialized(n, neighbor, 'v0') ]->
  [ !MetricVersion(n, neighbor, 'v0') ]
```

**Rationale**: 
- Track version independently per (source, destination) pair
- Allows version increment per destination route, not globally
- Matches real routing behavior (each route has independent freshness)

**Time**: 3 min (add new rule)

---

### Modification 2: Hello Broadcast - Include Dynamic Version

**File**: `patched/patched_lora_dv.spthy` (Hello_Broadcast_Auth rule)  
**Current Code**:
```tamarin
rule Hello_Broadcast_Auth:
  [ !Node(n,'honest'), !Honest(n), !HelloSeq(n,seq), !PSK(n,n,psk), 
    !MetricVersion(n,n,'v0') ]
  --[ HelloSent(n,'zero',seq), HMACGenerated(n,'zero',seq,'v0') ]->
  [ Out(<n,'zero',seq,n,'v0',hmac(<n,'zero',seq,n,'v0'>,psk)>) ]
```

**Change**: Reference current version, not hardcoded 'v0'
```tamarin
rule Hello_Broadcast_Auth:
  [ !Node(n,'honest'), !Honest(n), !HelloSeq(n,seq), !PSK(n,n,psk), 
    !MetricVersion(n,n,ver) ]                        // CHANGED: variable ver
  --[ HelloSent(n,'zero',seq), HMACGenerated(n,'zero',seq,ver) ]->
  [ Out(<n,'zero',seq,n,ver,hmac(<n,'zero',seq,n,ver>,psk)>) ]
```

**Rationale**: Use current version value in HMAC, not hardcoded constant.  
**Time**: 2 min

---

### Modification 3: Version Increment Rule - Trigger on Metric Change

**File**: `patched/patched_lora_dv.spthy` (NEW rule)  
**Add New Rule**:
```tamarin
// Version increment: triggered when metric for destination changes
rule MetricVersion_Increment_On_Change:
  [ !Node(n, 'honest'),
    !MetricVersion(n, dst, ver_old),
    !RouteTable(n, dst, metric_old, nh_old, seq_old, ver_old)
  ]
  --[ MetricVersionIncrement(n, dst, ver_old) ]->
  [ !MetricVersion(n, dst, ver_old.1) ]  // Symbolic increment: ver_old.1
```

**Rationale**:
- When node n's route to dst changes, automatically increment version
- Symbolic increment ('v0' → 'v0.1' → 'v0.1.1') ensures version uniqueness
- Prevents replay: old metric with old version no longer accepted

**Alternative Implementation** (if symbolic increment not supported):
```tamarin
// Use fresh version generation
rule MetricVersion_Increment_On_Change:
  [ !Node(n, 'honest'),
    !MetricVersion(n, dst, ver_old),
    Fr(~ver_new)
  ]
  --[ MetricVersionIncrement(n, dst, ver_old, ~ver_new) ]->
  [ !MetricVersion(n, dst, ~ver_new) ]
```

**Time**: 4 min (implementation + decision on symbolic vs. fresh)

---

### Modification 4: Route Update Verification - Version Binding Check

**File**: `patched/patched_lora_dv.spthy` (Route_Update_Verify rule)  
**Current Code**:
```tamarin
rule Route_Update_Verify:
  [ In(<src,metric,seq_new,nextHop,'v0',hmac(<src,metric,seq_new,nextHop,'v0'>,psk_src)>),
    !Node(dst,'honest'),
    !Neighbor(dst,dst),
    !Honest(src),
    !PSK(src, src, psk_src),
    !MetricVersion(src,src,'v0')
  ]
  --[ RouteUpdate(dst,src,metric,seq_new), 
      HMACVerified(dst,src,metric,seq_new), 
      RouteAccepted(dst,src,metric,nextHop,seq_new,'v0') ]->
  [ !RouteTable(dst,src,metric,nextHop,seq_new,'v0') ]
```

**Change 1**: Accept received version from message, not hardcoded
```tamarin
rule Route_Update_Verify:
  [ In(<src,metric,seq_new,nextHop,ver_rx,hmac_tag>),
    !Node(dst,'honest'),
    !Neighbor(dst,dst),
    !Honest(src),
    !PSK(src, src, psk_src),
    !MetricVersion(src,src,ver_current),
    // Condition: Version should match current OR be one step behind
    Condition(ver_rx = ver_current | ver_rx = ver_current_prev)
  ]
  --[ RouteUpdate(dst,src,metric,seq_new), 
      HMACVerified(dst,src,metric,seq_new), 
      RouteAccepted(dst,src,metric,nextHop,seq_new,ver_rx) ]->
  [ !RouteTable(dst,src,metric,nextHop,seq_new,ver_rx) ]
```

**Problem**: Tamarin doesn't support `Condition(...)` syntax. Use pattern matching instead:

**Change 2** (Tamarin-compatible):
```tamarin
// Option A: Accept any version (will verify in lemma)
rule Route_Update_Verify_Any_Version:
  [ In(<src,metric,seq_new,nextHop,ver_rx,hmac_tag>),
    !Node(dst,'honest'),
    !Honest(src),
    !PSK(src, src, psk_src)
  ]
  --[ HMACReceived(hmac_tag),
      RouteUpdate(dst,src,metric,seq_new), 
      RouteAccepted(dst,src,metric,nextHop,seq_new,ver_rx) ]->
  [ !RouteTable(dst,src,metric,nextHop,seq_new,ver_rx) ]

// Option B: Have strict version check (create separate rule)
rule Route_Update_Verify_Current_Version:
  [ In(<src,metric,seq_new,nextHop,ver,hmac(<src,metric,seq_new,nextHop,ver>,psk_src)>),
    !Node(dst,'honest'),
    !Honest(src),
    !PSK(src, src, psk_src),
    !MetricVersion(src, src, ver)   // Version must match current
  ]
  --[ RouteUpdate(dst,src,metric,seq_new), 
      HMACVerified(dst,src,metric,seq_new,ver),
      RouteAccepted(dst,src,metric,nextHop,seq_new,ver) ]->
  [ !RouteTable(dst,src,metric,nextHop,seq_new,ver) ]
```

**Rationale**: 
- Option A: Permissive (all versions), lemma will verify freshness
- Option B: Strict (only current version), prevents old version acceptance
- For L2_RouteFreshness proof, Option B is better

**Time**: 5 min (implement strict version checking)

---

### Modification 5: Update L2_RouteFreshness Lemma

**File**: `patched/patched_lora_dv.spthy` (L2 lemma)  
**Current Code**:
```tamarin
lemma L2_RouteFreshness_Patched:
  "All node dst metric1 metric2 nh1 nh2 seq ver1 ver2 #i #j.
      RouteAccepted(node,dst,metric1,nh1,seq,ver1) @ i
      & RouteAccepted(node,dst,metric2,nh2,seq,ver2) @ j
      & (Ex #h. HonestNode(node) @ h)
      & not (Ex #c. Compromised(dst) @ c)
    ==> ver1 = ver2"
```

**Change**: Strengthen freshness: higher version implies newer data
```tamarin
lemma L2_RouteFreshness_Patched:
  "All node dst metric1 metric2 nh1 nh2 seq ver1 ver2 #i #j.
      RouteAccepted(node,dst,metric1,nh1,seq,ver1) @ i
      & RouteAccepted(node,dst,metric2,nh2,seq,ver2) @ j
      & (Ex #h. HonestNode(node) @ h)
      & not (Ex #c. Compromised(dst) @ c)
      & #i < #j
    ==> ver1 <> ver2"  // Different seq/version pairs should have different versions
```

**Alternative** (simpler, same effect):
```tamarin
lemma L2_RouteFreshness_Patched:
  "All node dst metric1 metric2 nh1 nh2 seq ver #i #j.
      RouteAccepted(node,dst,metric1,nh1,seq,ver) @ i
      & RouteAccepted(node,dst,metric2,nh2,seq,ver) @ j  // Same seq, same version
      & (Ex #h. HonestNode(node) @ h)
      & #i < #j
    ==> metric1 = metric2"  // Metric must not change within same version
```

**Rationale**: Version binding ensures metric cannot change at same seq.  
**Time**: 3 min

---

### Modification 6: Sanity Lemma - Verify Version Increment Reachability

**File**: `patched/patched_lora_dv.spthy` (L_Sanity3 lemma - already exists)  
**Current Code**:
```tamarin
lemma L_Sanity3_MetricVersion_Increment_Happens: exists-trace
  "Ex n ver #i. MetricVersionIncrement(n,ver) @ i"
```

**Change**: Strengthen to verify per-destination increment
```tamarin
lemma L_Sanity3_MetricVersion_Increment_Happens: exists-trace
  "Ex n dst ver_old ver_new #i.
    MetricVersionIncrement(n, dst, ver_old, ver_new) @ i"
```

**Rationale**: Ensures version increment rule is reachable and used.  
**Time**: 1 min

---

### Modification 7: Compromise Node - No Change Needed

**File**: `patched/patched_lora_dv.spthy` (Compromise_Node rule)  
**Current Code**: 
```tamarin
rule Compromise_Node:
  [ !Node(n,'honest'), !PSK(n,n,psk), !RouteTable(n,dst,metric,nh,seq,ver) ]
  --[ Compromise(n), Compromised(n) ]->
  [ Out(psk), Out(<n,dst,metric,nh,seq,ver>) ]
```

**Change**: No modification needed. Attacker gains access to current version through route table.  
**Time**: 0 min

---

## Implementation Checklist

- [ ] **Mod 1**: Add MetricVersion_Init_Neighbors rule (3 min)
- [ ] **Mod 2**: Update Hello_Broadcast_Auth for dynamic version (2 min)
- [ ] **Mod 3**: Add MetricVersion_Increment_On_Change rule (4 min)
- [ ] **Mod 4**: Update Route_Update_Verify for strict version check (5 min)
- [ ] **Mod 5**: Update L2_RouteFreshness lemma (3 min)
- [ ] **Mod 6**: Update L_Sanity3 lemma (1 min)
- [ ] **Mod 7**: Verify Compromise_Node (no change) (0 min)

**Total Implementation Time**: ~18 minutes

---

## Verification Strategy (After Implementation)

### Pre-Verification Checks
1. All rules parse correctly in Tamarin syntax
2. Theory closes without errors
3. Lemmas are syntactically valid

### Expected Verification Results

| Lemma | Before (Symbolic v0) | After (Dynamic Vers) | Change | Explanation |
|-------|--------|-------------|--------|-------------|
| L1_RouteMetricAuthenticity | PROVED | PROVED | ✓ Same | HMAC prevents forgery regardless |
| L2_RouteFreshness | PROVED | PROVED+ | ✓ Stronger | Version binding strengthens freshness guarantee |
| L3_RouteConsistency | PROVED | PROVED | ✓ Same | Consistency independent of version dynamics |
| L4_NoUnauthorizedRoute | PROVED | PROVED | ✓ Same | Unauthorized check unaffected |
| L5_PSKConfidentiality | PROVED | PROVED | ✓ Same | Crypto assumptions unchanged |
| L_Sanity1 | PROVED | PROVED | ✓ Same | Route existence unaffected |
| L_Sanity2 | PROVED | PROVED | ✓ Same | HMAC verification logic same |
| L_Sanity3 | PROVED | PROVED+ | ✓ Enhanced | Now verifies per-destination increment |

**Expected Verification Time**: 8-12 minutes (version tracking adds minimal overhead)

---

## Risk Assessment

| Risk | Probability | Severity | Mitigation |
|------|-------------|----------|------------|
| Version increment rule non-terminating | Low (3%) | High | Test rule in isolation first |
| Lemma falsification due to version logic | Very Low (1%) | Medium | Version binding is cryptographic-strength |
| State space explosion from version tracking | Medium (15%) | High | May need to abstract versions (v0, v1, v2+) |
| Syntax errors in modified lemmas | Low (5%) | Low | Carefully copy existing lemma syntax |

**Contingency**: If state space explodes, simplify to: "version increments at most once per epoch"

---

## Expected Impact on Paper

### Section 4 (Protocol Model)
- Add: "Nodes track MetricVersion(n, dst, ver) per destination"
- Add: "Version increments when metric for destination changes"
- Add: "HMAC binding includes version: HMAC(msg, ver, psk)"

### Section 6 (Attack Analysis - L2 Freshness)
- Explain: "Per-destination version binding prevents metric downgrade even with same seq"
- Diagram: Timeline of metric replay attempt with version mismatch

### Section 7 (Patch Cost)
- Add: "Version tracking: 2-4 bytes per destination per node"
- Add: "Version increment: negligible (symbolic operation)"

---

## Integration with PSK Modification

These two modifications are **independent** but **complementary**:
- **PSK per-node**: Prevents source impersonation
- **Version per-destination**: Prevents metric replay

Combined in patched model:
- HMAC(metric, seq, ver) with per-node PSK(src, src, psk_src)
- Double protection against both forgery and replay

---

## Files to Modify

```
/home/augchao/Lora-Sec/phase2_tamarin_models/patched/
├── patched_lora_dv.spthy      ← MODIFY (6-7 rules/lemmas)
├── logs/                       ← OUTPUT (verification results)
└── BEFORE_METRICVERSION/       ← BACKUP (save v1 before modification)
```

---

## Execution Timeline

**Week 3 Session 1** (when ready):
1. Apply Mod 1-7 (18 min)
2. Verify syntax (5 min)
3. Run Tamarin prover (10-15 min)
4. Analyze results (10 min)
5. Document (5 min)

**Total**: 50-60 minutes (within 45-90 min estimate with buffer)

---

**Prepared by**: Sisyphus  
**Status**: Ready to execute  
**Priority**: HIGH - Completes L2 freshness guarantee for publication  
**Impact**: Combined with PSK modification, provides complete protocol security

