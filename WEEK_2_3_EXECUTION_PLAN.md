# Week 2-3 Execution Plan

**Date**: 2026-03-29  
**Duration**: 12 days (Week 2 Day 2 ~ Week 3 Day 6)  
**Status**: Ready to execute  
**Owner**: User (with optional Sisyphus delegation)

---

## 📋 Overview

This plan outlines the critical path for Week 2-3 with parallel tracks:
- **Track A (ns-3)**: Day 2-3 quick validation, then decision point
- **Track B (Tamarin)**: Day 8-10 lemma verification
- **Track C (Paper)**: Day 11 Related Work integration

**Critical Milestone**: Day 5 (Wednesday) — Go/No-Go decision based on ns-3 PDR

---

## 🚀 WEEK 2 DAY 2 (Monday) — ns-3 Quick Verification

### Morning: Preparation (15 min)
```bash
cd /home/augchao/Lora-Sec
./DAY_2_3_EXECUTION_GUIDE.sh
# Verifies Docker image exists
# Displays execution plan
```

### Day 2: Quick Test (30-60 min with GPU)
```bash
# Navigate to phase4
cd /home/augchao/Lora-Sec/phase4_ns3_simulation

# Run 27-scenario quick test
docker run --gpus all \
  -v $(pwd):/workspace \
  -v $(pwd)/results:/results \
  -e CUDA_VISIBLE_DEVICES=0 \
  lora-mesh-experiments:latest \
  bash -c "
    /usr/local/bin/compile-lora-mesh.sh
    cd /workspace
    python3 scratch/lora-mesh-experiment-gpu-parallel.py \
      --topologies Linear Tree Grid \
      --attacks None Spoofing Replay \
      --patches 0 1 \
      --seeds 1 \
      --workers 10
  "
```

### Success Criteria
✅ All 27 scenarios complete  
✅ No compilation errors  
✅ PDR > 0% for at least one topology  
✅ Results saved as JSON files

### If Fails
→ Check DOCKER_SETUP_GUIDE.md troubleshooting  
→ Review Docker logs  
→ Consult Oracle (if needed)

---

## 📊 WEEK 2 DAY 3 (Tuesday) — Analysis & Decision Prep

### Morning: Result Analysis (30 min)
```bash
cd /home/augchao/Lora-Sec/phase4_ns3_simulation

# Run analysis script
python3 analyze_pdr.py

# Check output
ls -lh results/EXPERIMENTAL_RESULTS.json
cat results/EXPERIMENTAL_RESULTS.json | python3 -m json.tool | head -50
```

### Expected Output
```json
{
  "summary": {
    "total_scenarios": 27,
    "successful": 27,
    "pdr_baseline": {
      "Linear_None": 0.85,
      "Tree_None": 0.78,
      "Grid_None": 0.82
    },
    "pdr_patched": {...}
  }
}
```

### Success Indicators
- ✅ PDR baseline (patch=0): Should show PDR > 0%
- ✅ PDR patched (patch=1): Should show PDR > baseline (fix improves performance)
- ✅ All three topologies have results
- ✅ Patch effect visible (patched > baseline)

---

## 🎯 WEEK 2 DAY 5 (Thursday) — Milestone Decision

### Critical Review Meeting (60 min)
Evaluate:
1. **PDR Results**: Does fix work? (PDR > 0%)
2. **Statistical Validity**: Are results consistent across seeds?
3. **Patch Effectiveness**: Does patch improve/stabilize PDR?
4. **Risk Assessment**: Any unexpected issues?

### Decision Options

**Option A: GO** (Proceed to 360-scenario suite)
```
✅ Decision: Proceed to Week 5-6 full 360-scenario run
📅 Timeline: Push full suite to Week 5 (6+ days away)
📋 Action: Update WEEK_1_DECISION.md → "PROCEED_FULL_SUITE"
```

**Option B: INVESTIGATE** (Repeat Day 2-3 with more seeds)
```
⏸️  Decision: Run 27 scenarios with 5 seeds (better statistics)
📅 Timeline: Add 2-3 days to debug/validate
📋 Action: Update WEEK_1_DECISION.md → "INVESTIGATE_MORE_SEEDS"
```

**Option C: HALT** (Rollback and troubleshoot)
```
❌ Decision: Stop, investigate root cause
📅 Timeline: Week 3 focused on ns-3 diagnosis
📋 Action: Update WEEK_1_DECISION.md → "HALT_INVESTIGATE"
```

---

## 🧮 WEEK 2 DAY 8 (Tuesday) — Tamarin Lemma Integration

### Pre-work: Verification (5 min)
Check that Tamarin is installed and working:
```bash
which tamarin-prover
tamarin-prover --version
```

### Integration: Baseline Lemmas (30 min)

**Option 1: Automatic (via script)**
```bash
cd /home/augchao/Lora-Sec
./TAMARIN_LEMMA_INTEGRATION.sh baseline
# Adds 4 executability lemmas
# Runs verification
# Shows results
```

**Option 2: Manual (if script fails)**
```bash
cd /home/augchao/Lora-Sec/phase2_tamarin_models/baseline

# Edit baseline_lora_dv.spthy
# Add these 4 lemmas before "end" statement:
#   lemma L_Ex1_Node_Can_Init: ...
#   lemma L_Ex2_Hello_Can_Broadcast: ...
#   lemma L_Ex3_Route_Can_Update: ...
#   lemma L_Ex4_Data_Can_Forward: ...

# Run verification
tamarin-prover baseline_lora_dv.spthy --prove > logs/verify_exec.log 2>&1
tail -50 logs/verify_exec.log
```

### Success Criteria
✅ 4 lemmas added to baseline_lora_dv.spthy  
✅ All 4 lemmas VERIFIED (not FALSIFIED)  
✅ Verification completes in <5 min  
✅ logs/verify_exec.log saved

---

## 🔐 WEEK 2 DAY 9 (Wednesday) — Tamarin Sanity Lemmas

### Integration: Patched Lemmas (30 min)

**Option 1: Automatic (via script)**
```bash
cd /home/augchao/Lora-Sec
./TAMARIN_LEMMA_INTEGRATION.sh patched
# Adds 3 sanity lemmas
# Runs verification
# Shows results
```

**Option 2: Manual**
```bash
cd /home/augchao/Lora-Sec/phase2_tamarin_models/patched

# Edit patched_lora_dv.spthy
# Add these 3 lemmas before "end" statement:
#   lemma L_Sanity1_Honest_Route_Exists: ...
#   lemma L_Sanity2_HMAC_Verified: ...
#   lemma L_Sanity3_MetricVersion_Increment_Happens: ...

# Run verification
tamarin-prover patched_lora_dv.spthy --prove > logs/verify_sanity.log 2>&1
tail -50 logs/verify_sanity.log
```

### Success Criteria
✅ 3 lemmas added to patched_lora_dv.spthy  
✅ All 3 lemmas VERIFIED  
✅ Verification completes in <5 min  
✅ logs/verify_sanity.log saved

---

## 📄 WEEK 2 DAY 10 (Thursday) — Verification Report

### Generate Report (15 min)
```bash
cd /home/augchao/Lora-Sec/phase2_tamarin_models

# Compile results from both logs
grep "VERIFIED\|FALSIFIED" baseline/logs/verify_exec.log > /tmp/baseline_results.txt
grep "VERIFIED\|FALSIFIED" patched/logs/verify_sanity.log > /tmp/patched_results.txt

# Create verification report
cat > WEEK_2_TAMARIN_VERIFICATION.md << 'EOF'
# Week 2 Tamarin Verification Report

## Baseline Model: Executability Lemmas
[Insert results from baseline_results.txt]

## Patched Model: Sanity Lemmas
[Insert results from patched_results.txt]

## Summary
Week 2 Tamarin tasks complete. Ready for Week 3 PSK + MetricVersion implementation.
EOF

# Update progress
echo "✅ Week 2 Tamarin verification complete" >> PROJECT_PROGRESS_TRACKING.md
```

---

## 📖 WEEK 2 DAY 11 (Friday) — Paper Integration

### Related Work Integration (60 min)

**Step 1: Merge 4 parts (30 min)**
```bash
cd /home/augchao/Lora-Sec

# Edit phase5_paper/DRAFT_v1.md
# After introduction, insert:
#   ## 2. Related Work
#   [Content of RELATED_WORK_PART1_LORAWAN.md]
#   [Content of RELATED_WORK_PART2_MESH.md]
#   [Content of RELATED_WORK_PART3_FORMAL.md]
#   [Content of RELATED_WORK_PART4_BREAKING.md]

# Update cross-references in introduction
```

**Step 2: Citation verification (15 min)**
```bash
# Check all 29 papers are cited
./crane_check_citations DRAFT_v1.md

# Expected: "Found 29 papers, all cited"
```

**Step 3: Grammar check (15 min)**
```bash
# Install/use grammar checker
# or use online service like Grammarly
```

### Success Criteria
✅ All 4 Related Work parts integrated  
✅ All 29 papers cited (check_citations returns 0 errors)  
✅ No forward references to undefined sections  
✅ Word count: 14-16 pages expected

---

## 📋 WEEK 3 DAY 1 (Monday) — Week 3 Planning

### Review & Planning (120 min)

**Tasks**:
1. Review Week 2 results
2. Prioritize Tamarin fixes (PSK vs MetricVersion)
3. Create Week 3-4 detailed plan
4. Identify resource requirements

**Outcomes**:
- WEEK_3_DETAILED_PLAN.md created
- PSK modification design document
- MetricVersion increment logic design
- ns-3 360-scenario execution schedule

---

## 📊 Summary Table

| Day | Track | Task | Duration | Status |
|-----|-------|------|----------|--------|
| D2 | ns-3 | Run 27 scenarios | 1 hr | ⏳ Ready |
| D3 | ns-3 | Analyze PDR results | 0.5 hr | ⏳ Ready |
| D5 | Management | **Go/No-Go Decision** | 1 hr | 🎯 Critical |
| D8 | Tamarin | Add baseline lemmas | 0.5 hr | ⏳ Ready |
| D9 | Tamarin | Add patched lemmas | 0.5 hr | ⏳ Ready |
| D10 | Tamarin | Generate report | 0.25 hr | ⏳ Ready |
| D11 | Paper | Integrate Related Work | 1 hr | ⏳ Ready |
| W3D1 | Planning | Week 3-4 detailed plan | 2 hr | ⏳ Ready |

**Total Execution Time**: ~8 hours (spread over 12 days)

---

## 🎯 Success Metrics

### ns-3 Track
✅ 27 scenarios complete  
✅ PDR > 0% for all topologies  
✅ Results saved as valid JSON  
✅ Analysis script runs without errors

### Tamarin Track
✅ 7 lemmas added (4 + 3)  
✅ All lemmas VERIFIED  
✅ Verification logs saved  
✅ Report generated

### Paper Track
✅ 4 Related Work parts integrated  
✅ 29 papers all cited  
✅ Grammar checked  
✅ 14-16 pages achieved

### Overall
✅ All 14 todo items completed  
✅ Week 2 cumulative progress: 15-20% of 14-week plan  
✅ On track for TDSC submission deadline

---

## 🛑 Blockers & Fallbacks

### Blocker 1: Docker/GPU Issues (ns-3)
**Fallback**: Run on CPU (10-20x slower, takes 5-10 hours)  
**Recovery**: See DOCKER_SETUP_GUIDE.md troubleshooting

### Blocker 2: Tamarin Installation (Lemma Verification)
**Fallback**: Skip automated verification, manual inspection of .spthy syntax  
**Recovery**: Install Tamarin: https://tamarin-prover.github.io/

### Blocker 3: PDR = 0% (ns-3 Still Broken)
**Fallback**: Investigate lora-mesh-sim.cc L283-344 again  
**Recovery**: Consult Oracle for deeper analysis

### Blocker 4: Lemma FALSIFIED (Tamarin Logic Error)
**Fallback**: Revise lemma definitions based on falsification trace  
**Recovery**: Week 3 includes lemma debugging time

---

## 📞 Support & Escalation

**Tier 1 (Documentation)**
- Docker issues → DOCKER_SETUP_GUIDE.md
- Compilation → COMPILATION_DETAILS.md
- Experiments → RUNNING_EXPERIMENTS.md

**Tier 2 (Scripts)**
- Execution → DAY_2_3_EXECUTION_GUIDE.sh
- Lemma integration → TAMARIN_LEMMA_INTEGRATION.sh

**Tier 3 (Specialist)**
- ns-3 diagnosis → Oracle consultation
- Tamarin proof issues → Metis (proof strategy)

---

## ✨ Ready to Execute

All preparation complete. You can now:

1. **Run Day 2-3 ns-3 verification** — Execute anytime
2. **Run Day 8-10 Tamarin lemmas** — Execute on Day 8
3. **Run Day 11 paper integration** — Execute on Day 11

No blocking dependencies. Parallel execution encouraged.

**Next step**: Confirm Day 2-3 ns-3 execution start time.

---

**Prepared by**: Claude Code (Sisyphus)  
**Date**: 2026-03-29  
**Status**: Ready for user execution
