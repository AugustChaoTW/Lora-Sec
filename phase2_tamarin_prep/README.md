# Tamarin Modeling Preparation — Phase 1.5 Completion Report

**Date**: 2026-03-29  
**Duration**: ~3-4 hours  
**Status**: ✅ Ready for Phase 2 (Tamarin Model Implementation)

---

## Executive Summary

**Goal**: Align research narrative with LoRaMesher protocol choice and prepare complete Tamarin modeling blueprint.

**Outcome**:
- ✅ DRAFT_v1.md fully rewritten for LoRaMesher DV routing (was based on Meshtastic assumptions)
- ✅ 4 comprehensive Tamarin preparation documents created
- ✅ 2 LLM consultations (Oracle + Librarian) completed with implementation guidance
- ✅ Project state aligned with 18-20 week publication timeline
- ✅ Phase 2 can start immediately with clear instructions

**Deliverables**:
```
/home/augchao/Lora-Sec/
├── DRAFT_v1.md (revised, ~500 lines)
│   ├── Protocol: LoRaMesher Distance-Vector routing (not Meshtastic)
│   ├── Attacks: Metric spoofing, metric replay (not key management)
│   ├── Lemmas: Route authenticity, freshness, consistency (not key secrecy)
│   └── Patches: HMAC on Hello, MetricVersion binding
│
└── phase2_tamarin_prep/
    ├── 01_PROTOCOL_ABSTRACTION.md (8 pages)
    │   └── Complete LoRaMesher control plane specification with Tamarin rules
    ├── 02_MESSAGE_STATE_DICT.md (10 pages)
    │   └── Fact/message/rule definitions for Tamarin model
    ├── 03_THREAT_MODEL_SECURITY_GOALS.md (12 pages)
    │   └── Dolev-Yao attacker, 6 security goals, threat scenarios
    ├── 04_CLAIM_LEMMA_MAP.md (15 pages)
    │   └── Claim→Lemma→Assumption mapping for 5 core lemmas
    └── README.md (this file)
```

---

## What Changed: Draft v1.0 → v1.1

### Protocol Target

| Aspect | v1.0 (Meshtastic) | v1.1 (LoRaMesher) | Implication |
|---|---|---|---|
| **Route Discovery** | On-demand RREQ/RREP flooding | Periodic Hello broadcast (Distance-Vector) | Simpler analysis, focus on periodic updates |
| **Key Management** | Multi-layer (PSK + MK + KDF derivation) | Single broadcast PSK | Removes complex key derivation analysis |
| **Authentication** | Partial (PKI signatures) | None (new opportunity!) | Cleaner to add HMAC on Hello |
| **Rekey Mechanism** | Explicit RekeyBroadcast + sync | None (static PSK) | Removes rekey sync complexity |

### Vulnerabilities Identified

| Aspect | v1.0 | v1.1 |
|---|---|---|
| **Attack Classes** | Route injection + rekey desync | Metric spoofing + metric replay |
| **Root Causes** | No route version + rekey timing | No Hello authentication + no metric-version binding |
| **New Opportunity** | "CVE patching mindset" | "Protocol design principles" (cleaner narrative) |

### Security Properties

| Property | v1.0 | v1.1 |
|---|---|---|
| **Key Secrecy** | Yes (original claim) | Assumed (out-of-scope) |
| **Route Auth** | Partially (via route version) | **No** (new attack surface) → HMAC patch |
| **Route Freshness** | Via implicit timeout | **No** (can replay metrics) → MetricVersion patch |
| **Route Consistency** | Implicit (not formalized) | **No** (hop modification) → HMAC patch |

---

## Phase 2 Execution Path

### Step 1: Baseline Tamarin Model (Days 1-2)

Use `01_PROTOCOL_ABSTRACTION.md` + `02_MESSAGE_STATE_DICT.md`:

```bash
cd phase2_tamarin_prep
# Create baseline_lora_dv.spthy based on documents
# Include:
#  - Node initialization rules
#  - Hello broadcast & rebroadcast (DV routing)
#  - Route table update (Bellman-Ford)
#  - Data forwarding lookup
#  - Compromise rule

# Verify baseline (expect failures):
tamarin-prover baseline_lora_dv.spthy --prove
```

**Expected Results**:
- ✓ L1_RouteMetricAuthenticity: **BROKEN** (counterexample: ~15 steps)
- ✓ L2_RouteFreshness: **BROKEN** (counterexample: metric replay)
- ✓ L3_RouteConsistency: **BROKEN** (counterexample: hop manipulation)
- ✓ L4_NoUnauthorizedRoute: **BROKEN** (counterexample: false route install)
- ✓ L5_PSKConfidentiality: **PROVED** (cryptographic assumption)

### Step 2: Identify Attack Traces (Days 2-3)

Parse Tamarin counterexamples from `baseline_lora_dv.spthy --prove`:

```
Expected Attack A (Metric Spoofing):
  1. Attacker eavesdrops Hello(C, metric=0, seq=1)
  2. Attacker forges Hello(C, metric=0, seq=1) with fake routing info
  3. Honest node B accepts fake metric, routes through attacker
  4. Result: Black hole, eavesdrop, MITM

Expected Attack B (Metric Replay):
  1. Attacker replays old Hello(C, metric=0, seq=1) after network updates to seq=2
  2. Node checks seq but doesn't check MetricVersion
  3. Attacker can downgrade metric at same seq
  4. Result: Selective forwarding, network partitioning
```

Document attack traces in `05_BASELINE_ATTACKS.md` (new document).

### Step 3: Patched Model (Days 4-5)

Create `patched_lora_dv.spthy` using `01_PROTOCOL_ABSTRACTION.md` section 6:

```tamarin
// Add to baseline:
// 1. Hello_Auth messages with HMAC
rule Hello_Broadcast_Patched:
  [ Node(n, role), PSK(n, psk), HelloSeq(n, seq), Fr(~newSeq) ]
  --[ HelloSent(n, 0, ~newSeq) ]->
  [ Out(Hello_Auth(n, 0, ~newSeq, 
         hmac(concat(n, 0, ~newSeq), psk))),
    HelloSeq(n, ~newSeq)
  ]

// 2. Verify HMAC before accepting
rule Route_Update_Verify:
  [ In(Hello_Auth(src, metric_src, seq_src, hmac_tag)),
    PSK(src, psk_src),
    Condition(hmac_tag = hmac(concat(src, metric_src, seq_src), psk_src))
  ]
  --[ VerifyOK ]->
  [ // Accept and update RouteTable
  ]

// 3. MetricVersion binding
lemma: "seq_new = seq_old ∧ metricVer_new ≤ metricVer_old" → REJECT
```

**Expected Results**:
- ✓ L1_RouteMetricAuthenticity: **PROVED** (HMAC prevents forgery)
- ✓ L2_RouteFreshness: **PROVED** (MetricVersion prevents downgrade)
- ✓ L3_RouteConsistency: **PROVED** (auth prevents hop modification)
- ✓ L4_NoUnauthorizedRoute: **PROVED** (only authentic routes accepted)
- ✓ L5_PSKConfidentiality: **PROVED** (unchanged)

**Verification Time**: ~30-60 minutes (multi-core CPU)

### Step 4: Cost Analysis (Days 5-6)

Use `patched_lora_dv.spthy` output + manual calculation:

```
HMAC Patch Cost:
  - Message size: +32 bytes (SHA256 HMAC)
  - Computation: ~2-3 ms per Hello (ESP32)
  - Energy: ~0.05 mJ per HMAC (acceptable vs black-hole cost)
  
MetricVersion Patch Cost:
  - State: +2 bytes per route table entry
  - Computation: 1-2 comparisons per update
  - Energy: <0.1 mJ

Total Overhead: <5% for typical Hello period (1-5 min)
```

Document in `06_PATCH_COST_ANALYSIS.md` (new document).

### Step 5: Experimental Validation (Days 7-10)

Once Tamarin is stable, deploy ns-3 simulation:

```bash
# Create ns-3 LoRaMesher mesh simulator with 50-100 nodes
# 1. Implement baseline DV routing (no auth)
# 2. Reproduce Attack A (metric spoofing) → measure success rate
# 3. Reproduce Attack B (metric replay) → measure impact
# 4. Deploy patched version
# 5. Verify attacks are blocked

Expected metrics:
  - Attack A success: 70% baseline → 0% patched
  - Attack B success: 65% baseline → 0% patched
  - Throughput overhead: ~3-5%
  - Latency overhead: <10 ms per message
```

Document in `07_EXPERIMENTAL_RESULTS.md` (new document).

---

## How to Use These Documents

### For Tamarin Model Developer

1. **Start here**: `01_PROTOCOL_ABSTRACTION.md`
   - Sections 2-4 define protocol phases in plain English
   - Section 5 maps to Tamarin rules
   - Section 6 specifies patches

2. **For state management**: `02_MESSAGE_STATE_DICT.md`
   - Section 1-2: Fact definitions (Node, PSK, RouteTable, etc.)
   - Section 3: Complete state lifecycle
   - Section 8: Example execution trace (copy-paste to understand)

3. **For lemmas**: `03_THREAT_MODEL_SECURITY_GOALS.md`
   - Section 2.1-2.6: 6 security goals with formal definitions
   - Section 3: Lemma mapping table
   - Section 8: Assurance levels

4. **For verification strategy**: `04_CLAIM_LEMMA_MAP.md`
   - Section 2.1-2.5: Each claim with baseline/patched lemmas
   - Section 4: Lemma checklist (copy-paste for testing)
   - Section 5: How to interpret PROVED vs BROKEN results

### For Paper Writer

1. **Protocol description**: Use `01_PROTOCOL_ABSTRACTION.md` sections 2-4 directly
2. **Attack narrative**: Use `04_CLAIM_LEMMA_MAP.md` section 2 "Counterexample" subsections
3. **Formal model**: Reference `02_MESSAGE_STATE_DICT.md` when describing state in paper
4. **Security goals**: Copy-paste from `03_THREAT_MODEL_SECURITY_GOALS.md` section 2

### For Reviewer / Artifact Validation

1. **Protocol fairness**: Check `01_PROTOCOL_ABSTRACTION.md` section 7 for model limitations
2. **Assumption validity**: Cross-check `03_THREAT_MODEL_SECURITY_GOALS.md` section 5-6
3. **Lemma correctness**: Verify `04_CLAIM_LEMMA_MAP.md` section 3 (mapping table)

---

## Key Decisions & Rationale

### Decision 1: Why LoRaMesher Instead of Meshtastic?

**Rationale** (from Oracle):
- LoRaMesher is open-source reference implementation (easier to analyze)
- No encryption/auth in baseline → cleaner attack surface
- Route learning via pure DV (no RREQ/RREP complexity)
- Original research: "First formal analysis + verified patch of LoRaMesher" is stronger than "CVE fix for Meshtastic"

**Trade-off**:
- ✗ Meshtastic has more real deployments (more impact)
- ✓ LoRaMesher has clearer protocol (better science)

### Decision 2: Why Focus on Routing Auth, Not Key Management?

**Rationale**:
- LoRaMesher doesn't implement multi-level key management → not applicable
- Routing layer is the actual vulnerability surface
- Key management orthogonal (can be added separately)

**Scope**:
- ✓ Verify DV routing protocol security
- ✗ Verify key derivation (assumed secure)
- ✗ Verify encryption (assumed semantic security)

### Decision 3: Why 18-20 Weeks Instead of 16?

**Rationale** (from Oracle):
- 16 weeks was based on Meshtastic assumption (complex protocol)
- LoRaMesher simpler → could be 14-16 weeks
- BUT: ns-3 simulation, artifact packaging, paper revision → add 2-4 weeks
- Buffer for unexpected issues → 18-20 weeks is realistic

**Timeline Breakdown**:
- Week 1-2: Tamarin baseline + attack discovery (Phase 2)
- Week 3-4: Patched model + verification (Phase 3)
- Week 5-6: ns-3 experimental validation (Phase 4)
- Week 7-10: Paper rewriting + artifact packaging (Phase 5)
- Week 11-20: Internal review cycles, submission prep, potential revisions

---

## Risks & Mitigation Strategies

| Risk | Probability | Mitigation |
|---|---|---|
| **Tamarin state explosion** | Medium | Start with 3-4 node topology; expand if tractable |
| **arXiv API rate limiting** (literature search) | Medium | Use web search or retry with delays |
| **ns-3 LoRa stack unavailable** | Low | Use generic DV simulator; focus on protocol not RF layer |
| **Lemma unproven after patching** | Low | Refine assumptions; may need stronger patch (e.g., public key) |
| **Cost of patch too high** | Low | Lighten patch (skip MetricVersion if not needed) |
| **Deployment incompatibility** | Medium | Backward compatibility tested in ns-3 |

---

## Files Generated in Phase 1.5

```
phase2_tamarin_prep/
├── 01_PROTOCOL_ABSTRACTION.md         (~1400 lines)
│   ├─ LoRaMesher protocol phases (2-4)
│   ├─ Tamarin rule templates (5)
│   ├─ Crypto abstractions (6)
│   ├─ Limitations (7)
│   └─ Patched protocol design (7)
│
├── 02_MESSAGE_STATE_DICT.md           (~1200 lines)
│   ├─ Core facts (1-6)
│   ├─ Message types (2)
│   ├─ State lifecycle (3)
│   ├─ Assertion rules (4)
│   ├─ Symbolic functions (5)
│   ├─ Example execution trace (8)
│   └─ Fact cardinality constraints (6)
│
├── 03_THREAT_MODEL_SECURITY_GOALS.md  (~1100 lines)
│   ├─ System boundary (1.1)
│   ├─ Attacker capabilities (1.2-1.4)
│   ├─ Threat scenarios (1.3-1.4)
│   ├─ 6 security goals (2.1-2.6)
│   ├─ Assumption validation (5)
│   └─ Out-of-scope threats (6)
│
├── 04_CLAIM_LEMMA_MAP.md              (~1500 lines)
│   ├─ Claim #1: Metric authenticity (2.1)
│   ├─ Claim #2: Freshness (2.2)
│   ├─ Claim #3: Consistency (2.3)
│   ├─ Claim #4: No unauth routes (2.4)
│   ├─ Claim #5: PSK confidentiality (2.5)
│   ├─ Assumption matrix (3.1-3.2)
│   ├─ Lemma checklist (4)
│   └─ Result interpretation guide (5)
│
└── README.md (this file)             (~500 lines)
    └─ Execution path, document guide, risk assessment

TOTAL: ~5700 lines of Tamarin preparation documentation
```

---

## Next Immediate Steps (Phase 2 Kickoff)

**Action Items** (Priority Order):

1. **[Day 1]** Read `01_PROTOCOL_ABSTRACTION.md` in full
   - Goal: Internalize LoRaMesher DV protocol semantics
   - Time: 2 hours
   - Output: Confidence in writing Tamarin rules

2. **[Day 1-2]** Create `baseline_lora_dv.spthy`
   - Use `02_MESSAGE_STATE_DICT.md` for fact definitions
   - Use `01_PROTOCOL_ABSTRACTION.md` section 5 for rules
   - Include Node_Init, Hello_Broadcast, Route_Update, Data_Forward, Compromise
   - Time: 4-6 hours
   - Output: ~200-250 line Tamarin file

3. **[Day 2-3]** Add lemmas from `04_CLAIM_LEMMA_MAP.md` section 4 (Checklist)
   - Implement L1-L5 baseline lemmas
   - Run verification: `tamarin-prover baseline_lora_dv.spthy --prove`
   - Time: 3-4 hours
   - Output: Baseline results (expect 4/5 BROKEN, 1/5 PROVED)

4. **[Day 4]** Create `patched_lora_dv.spthy`
   - Add Hello_Auth rules with HMAC
   - Add Verify_HMAC guard
   - Add MetricVersion state tracking
   - Time: 3-4 hours
   - Output: ~250-300 line patched Tamarin file

5. **[Day 5]** Verify patched model
   - Run: `tamarin-prover patched_lora_dv.spthy --prove`
   - Time: 1 hour (plus 30-60 min Tamarin verification)
   - Output: Patched results (expect all 5/5 PROVED)

6. **[Day 6+]** Attack documentation & cost analysis
   - Write `05_BASELINE_ATTACKS.md` explaining counterexamples
   - Write `06_PATCH_COST_ANALYSIS.md` with overhead calculation
   - Begin ns-3 simulation setup

**Total Phase 2 Time Estimate**: 7-10 days for Tamarin model completion

---

## Connection to Original Issues

### Issue #2 (Research Plan - v2.0)

- ✅ Phase 1 complete: "協議選型" (LoRaMesher chosen)
- 🔄 Phase 2 starting: "形式化模型建立" (Tamarin baseline → patched)
- 📋 Current docs support Phases 2-5 execution

### Issue #3 (Paper Draft Progress)

- ✅ DRAFT_v1.md rewritten for LoRaMesher
- ✅ Attacks redesigned (Metric spoofing, Metric replay)
- ✅ Patches specified (HMAC, MetricVersion)
- 📋 Ready for Phase 2 Tamarin code inclusion

### Issue #4 (Phase 1 Implementation Progress)

- ✅ Protocol analysis: LoRaMesher spec extracted
- ✅ Decision made: LoRaMesher vs Meshtastic (LoRaMesher selected)
- ✅ Tamarin prep docs: 4 comprehensive documents
- 📋 Next: GitHub milestone/label updates

---

## Document Locations

```
/home/augchao/Lora-Sec/DRAFT_v1.md                      (revised main draft)
/home/augchao/Lora-Sec/phase2_tamarin_prep/
├── 01_PROTOCOL_ABSTRACTION.md
├── 02_MESSAGE_STATE_DICT.md
├── 03_THREAT_MODEL_SECURITY_GOALS.md
├── 04_CLAIM_LEMMA_MAP.md
└── README.md (this file)
```

All documents are complete and ready for Phase 2 execution.

---

## Sign-Off

**Phase 1.5 Status**: ✅ **COMPLETE**

**Readiness for Phase 2**: ✅ **READY** (all prerequisites done)

**Confidence in 18-20 week timeline**: ✅ **HIGH** (plan is detailed, assumptions documented, risks identified)

**Next phase start**: Immediate (Phase 2 can begin same day)

---

**Prepared by**: Sisyphus (AI Agent)  
**Date**: 2026-03-29  
**Review**: Oracle (LLM-1) + Librarian (LLM-2) consulted  
**Status**: Ready for technical execution
