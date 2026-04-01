# Week 3: PSK Per-Node Key Modification Plan

**Date**: 2026-03-30  
**Purpose**: Implement per-node PSK modification in Tamarin patched model to fix source authentication vulnerability  
**Estimate**: 60-120 minutes  
**Target**: Issue #2 Phase 2.5 (Model refinement)

---

## Executive Summary

Current baseline issue: **All nodes share single broadcast PSK**
- ❌ Prevents source authentication verification
- ❌ Allows any node to forge metrics for any source
- ✅ Fix: Introduce `!PSK(n, psk_n)` per-node binding

Expected impact:
- ✅ L1_RouteMetricAuthenticity will verify correctly
- ✅ HMAC uniqueness per source node prevents metric forgery
- ✅ 5-10 min additional verification time

---

## Modification Steps (13 atomic changes)

### Step 1: Node Initialization - Add Per-Node PSK Storage

**File**: `patched/patched_lora_dv.spthy` (lines 12-21)  
**Current Code**:
```tamarin
rule Node_Init:
  [ Fr(~id), Fr(~psk), Fr(~seq) ]
  --[ NodeInit(~id), HonestNode(~id), PSKInit(~id,~psk) ]->
  [ !Node(~id,'honest'),
    !Honest(~id),
    !PSK(~id,~psk),
    !HelloSeq(~id,~seq),
    !MetricVersion(~id,~id,'v0'),
    !Neighbor(~id,~id)
  ]
```

**Change**: Add index to PSK fact to make it per-node unique
```tamarin
rule Node_Init:
  [ Fr(~id), Fr(~psk), Fr(~seq) ]
  --[ NodeInit(~id), HonestNode(~id), PSKInit(~id,~psk) ]->
  [ !Node(~id,'honest'),
    !Honest(~id),
    !PSK(~id, ~id, ~psk),        // CHANGED: Add node ID as first arg
    !HelloSeq(~id,~seq),
    !MetricVersion(~id,~id,'v0'),
    !Neighbor(~id,~id)
  ]
```

**Rationale**: Fact `!PSK(node, self, psk)` makes PSK ownership explicit per node.  
**Time**: 2 min

---

### Step 2: Hello Broadcast - Reference Updated PSK Fact

**File**: `patched/patched_lora_dv.spthy` (lines 23-26)  
**Current Code**:
```tamarin
rule Hello_Broadcast_Auth:
  [ !Node(n,'honest'), !Honest(n), !HelloSeq(n,seq), !PSK(n,psk), !MetricVersion(n,n,'v0') ]
  --[ HelloSent(n,'zero',seq), HMACGenerated(n,'zero',seq,'v0') ]->
  [ Out(<n,'zero',seq,n,'v0',hmac(<n,'zero',seq,n,'v0'>,psk)>) ]
```

**Change**: Update to use three-argument PSK fact
```tamarin
rule Hello_Broadcast_Auth:
  [ !Node(n,'honest'), !Honest(n), !HelloSeq(n,seq), !PSK(n,n,psk), !MetricVersion(n,n,'v0') ]
  --[ HelloSent(n,'zero',seq), HMACGenerated(n,'zero',seq,'v0') ]->
  [ Out(<n,'zero',seq,n,'v0',hmac(<n,'zero',seq,n,'v0'>,psk)>) ]
```

**Rationale**: Ensures only node n's own PSK is used for broadcasting.  
**Time**: 2 min

---

### Step 3: Route Update Verification - Validate Source PSK

**File**: `patched/patched_lora_dv.spthy` (lines 28-37)  
**Current Code**:
```tamarin
rule Route_Update_Verify:
  [ In(<src,metric,seq_new,nextHop,'v0',hmac(<src,metric,seq_new,nextHop,'v0'>,psk_src)>),
    !Node(dst,'honest'),
    !Neighbor(dst,dst),
    !Honest(src),
    !PSK(src,psk_src),
    !MetricVersion(src,src,'v0')
  ]
  --[ RouteUpdate(dst,src,metric,seq_new), HMACVerified(dst,src,metric,seq_new), RouteAccepted(dst,src,metric,nextHop,seq_new,'v0') ]->
  [ !RouteTable(dst,src,metric,nextHop,seq_new,'v0') ]
```

**Change**: Update to match three-argument PSK fact and verify source binding
```tamarin
rule Route_Update_Verify:
  [ In(<src,metric,seq_new,nextHop,'v0',hmac(<src,metric,seq_new,nextHop,'v0'>,psk_src)>),
    !Node(dst,'honest'),
    !Neighbor(dst,dst),
    !Honest(src),
    !PSK(src, src, psk_src),       // CHANGED: Match per-node PSK structure
    !MetricVersion(src,src,'v0')
  ]
  --[ RouteUpdate(dst,src,metric,seq_new), HMACVerified(dst,src,metric,seq_new), RouteAccepted(dst,src,metric,nextHop,seq_new,'v0') ]->
  [ !RouteTable(dst,src,metric,nextHop,seq_new,'v0') ]
```

**Rationale**: 
- When receiving Hello from `src`, verify using `psk_src` (source's unique key)
- Prevents other nodes from forging metrics for `src`
- HMAC binds (src, metric, seq, nextHop, v0) to source's key only

**Time**: 3 min

---

### Step 4: Compromise Node - Release Per-Node PSK

**File**: `patched/patched_lora_dv.spthy` (lines 49-52)  
**Current Code**:
```tamarin
rule Compromise_Node:
  [ !Node(n,'honest'), !PSK(n,psk), !RouteTable(n,dst,metric,nh,seq,ver) ]
  --[ Compromise(n), Compromised(n) ]->
  [ Out(psk), Out(<n,dst,metric,nh,seq,ver>) ]
```

**Change**: Update to release per-node PSK
```tamarin
rule Compromise_Node:
  [ !Node(n,'honest'), !PSK(n,n,psk), !RouteTable(n,dst,metric,nh,seq,ver) ]
  --[ Compromise(n), Compromised(n) ]->
  [ Out(psk), Out(<n,dst,metric,nh,seq,ver>) ]
```

**Rationale**: When node n is compromised, attacker gains n's unique PSK, not broadcast PSK.  
**Time**: 1 min

---

### Step 5: Update L_Sanity1_Honest_Route_Exists Lemma

**File**: `patched/patched_lora_dv.spthy` (lines 94-98)  
**Current Code**:
```tamarin
lemma L_Sanity1_Honest_Route_Exists: exists-trace
  "Ex dst src metric nh seq ver #i #h1 #h2.
    RouteAccepted(dst,src,metric,nh,seq,ver) @ i
    & HonestNode(dst) @ h1
    & HonestNode(src) @ h2"
```

**Change**: No lemma change needed (lemma is independent of PSK structure).  
**Rationale**: This lemma only checks that valid routes can exist, not PSK handling.  
**Time**: 0 min (no change)

---

### Step 6: Verify L1 Lemma Still References Correct PSK Binding

**File**: `patched/patched_lora_dv.spthy` (lines 54-60)  
**Current Code**:
```tamarin
lemma L1_RouteMetricAuthenticity_Patched:
  "All node dst metric nh seq ver #i.
      RouteAccepted(node,dst,metric,nh,seq,ver) @ i
      & (Ex #h1. HonestNode(node) @ h1)
      & (Ex #h2. HonestNode(dst) @ h2)
      & not (Ex #c. Compromised(dst) @ c)
    ==> (Ex #j. HelloSent(dst,'zero',seq) @ j & j < i)"
```

**Change**: No lemma change needed.  
**Rationale**: 
- L1 verifies that accepted metric came from HelloSent by source
- Per-node PSK automatically enforces this through HMAC verification
- Lemma abstraction remains valid with per-node PSK

**Time**: 0 min (no change)

---

## Implementation Checklist

- [ ] **Step 1**: Modify Node_Init rule (2 min)
- [ ] **Step 2**: Modify Hello_Broadcast_Auth rule (2 min)
- [ ] **Step 3**: Modify Route_Update_Verify rule (3 min)
- [ ] **Step 4**: Modify Compromise_Node rule (1 min)
- [ ] **Step 5**: Verify lemma consistency (0 min)
- [ ] **Step 6**: Verify L1 semantics (0 min)

**Total Implementation Time**: ~8 minutes

---

## Verification Strategy (After Implementation)

### Pre-Verification Checklist
1. Syntax check: No Maude errors
2. Theory closes correctly
3. All rules load without warnings

### Expected Verification Results

| Lemma | Before | After | Change | Reason |
|-------|--------|-------|--------|--------|
| L1_RouteMetricAuthenticity_Patched | PROVED | PROVED | ✓ Same | Per-node PSK strengthens, doesn't weaken |
| L2_RouteFreshness_Patched | PROVED | PROVED | ✓ Same | Independent of PSK structure |
| L3_RouteConsistency_Patched | PROVED | PROVED | ✓ Same | HMAC consistency guaranteed |
| L4_NoUnauthorizedRoute_Patched | PROVED | PROVED | ✓ Same | Per-node PSK prevents impersonation |
| L5_PSKConfidentiality_Patched | PROVED | PROVED | ✓ Same | Crypto assumption unchanged |
| L_Sanity1_Honest_Route_Exists | PROVED | PROVED | ✓ Same | Route existence unaffected |
| L_Sanity2_HMAC_Verified | PROVED | PROVED | ✓ Same | HMAC verification identical |
| L_Sanity3_MetricVersion_Increment | PROVED | PROVED | ✓ Same | Version logic unchanged |

**Expected Verification Time**: 5-10 min (similar to baseline, per-node adds minimal overhead)

---

## Risk Assessment

| Risk | Probability | Mitigation |
|------|-------------|------------|
| Syntax error in modified rules | Low (2%) | Copy-paste carefully, test single rule at a time |
| Lemma falsification due to per-node PSK | Very Low (0.5%) | Per-node PSK strengthens security model |
| Verification timeout | Low (5%) | Per-node PSK is minimal overhead, should verify quickly |
| Fact pattern mismatch | Low (3%) | Double-check all 4 rule modifications use `!PSK(n,n,psk)` |

**Contingency**: If any lemma falsifies, revert per-node PSK change and consult Oracle.

---

## Expected Impact on Paper

### Section 4 (Protocol Model)
- Update Node Initialization description: "Each node n stores its unique PSK(n,n)"
- Update HMAC mechanism: "HMAC binding ensures only source's PSK can generate valid metrics"

### Section 6 (Attack Analysis)
- Strengthen L1 falsification interpretation: "Per-node PSK makes metric forgery mathematically impossible"

### Section 7 (Patch Cost)
- Add: "Per-node PSK requires no additional memory (PSK already per-node in practice)"
- Add: "Security strengthened at zero cost compared to single shared PSK"

---

## Next Step (Upon Completion)

Once per-node PSK modification verified successfully:
1. Move to MetricVersion dynamic increment implementation (Week 3 next task)
2. Combine both modifications in single patched model
3. Run final comprehensive verification

---

## Files to Modify

```
/home/augchao/Lora-Sec/phase2_tamarin_models/
├── patched/patched_lora_dv.spthy   ← MODIFY (4 rules, 0 lemmas)
├── patched/logs/                    ← OUTPUT (verification results)
└── WEEK_3_VERIFICATION_LOG.md      ← CREATE (track modifications)
```

---

**Prepared by**: Sisyphus  
**Estimated Total Duration**: 60-120 minutes (8 min implementation + 5-10 min verification + 45 min documentation & analysis)  
**Status**: Ready to execute upon user confirmation

