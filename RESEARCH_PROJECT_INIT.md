# 🔬 LoRa Mesh Security Research Project Initialization

**Initialized**: 2026-03-29  
**Status**: ✅ Research project structure ready  
**Target Venue**: IEEE Transactions on Dependable and Secure Computing (TDSC)  
**Submission Target**: Week 13-14 (~July 7, 2026)  

---

## 📁 Project Structure

```
Lora-Sec/
├── OPTIMIZATION_MASTERPLAN.md       # 14-week complete execution plan
├── DRAFT_v1.md                       # Paper draft (current)
├── PHASE5_PAPER_INTEGRATION.md       # Chapter planning
├── IEEE_SUBMISSION_CHECKLIST.md      # Submission requirements
├── PROJECT_STATUS.md                 # Project tracking
│
├── phase2_tamarin_models/
│   ├── baseline/baseline_lora_dv.spthy
│   ├── patched/patched_lora_dv.spthy
│   ├── logs/
│   └── VERIFICATION_SUMMARY.md
│
├── phase4_ns3_simulation/
│   ├── scratch/
│   │   ├── lora-mesh-experiment-gpu-parallel.py
│   │   └── lora-mesh-sim.cc
│   ├── results/
│   │   └── (360 scenario results)
│   ├── Dockerfile.gpu
│   └── docker-run-gpu.sh
│
├── references/                       # Literature management
│   ├── papers/                       # Paper metadata (YAML)
│   ├── pdfs/                         # Paper PDFs
│   └── bibliography.bib              # BibTeX export
│
├── .github/
│   ├── ISSUE_TEMPLATE/
│   │   └── research-task.yml         # Structured task template
│   └── workflows/                    # (GitHub Actions for CI/CD)
│
└── [Supporting directories]
    ├── figures/                      # Generated figures
    ├── tables/                       # Generated tables
    ├── artifacts/                    # Tamarin/ns-3 artifacts
    └── analysis/                     # Statistical analysis scripts
```

---

## 🏷️ GitHub Labels (Created)

### Phase Labels
- `phase:phase-1` — Literature Review
- `phase:phase-2` — Threat Modeling
- `phase:phase-3` — Formal Verification (Tamarin)
- `phase:phase-4` — Experimental Validation (ns-3)
- `phase:phase-5` — Paper Integration
- `phase:phase-6` — Artifact & Submission

### Type Labels
- `type:research-design` — Design/planning
- `type:implementation` — Code/model changes
- `type:experiment` — Simulation/experimental work
- `type:analysis` — Data analysis
- `type:documentation` — Documentation

### Priority Labels
- `priority:P0` — **Blocking** (must complete first)
- `priority:P1` — **High** (needed soon)
- `priority:P2` — **Medium** (important but not urgent)
- `priority:P3` — **Low** (nice-to-have)

---

## 📋 GitHub Milestones (Created)

1. **Phase 1: Literature Review** — Complete survey, screen candidates
2. **Phase 2: Threat Modeling** — Formalize threat model & protocol spec
3. **Phase 3: Formal Verification** — Develop & verify Tamarin models
4. **Phase 4: Experimental Validation** — ns-3 simulations & data collection
5. **Phase 5: Paper Integration** — Write complete draft with evidence
6. **Phase 6: Artifact & Submission** — Package artifact & submit to TDSC

---

## 📚 Research Task Template

Use **`.github/ISSUE_TEMPLATE/research-task.yml`** to create structured tasks.

**Template includes**:
- Phase selection (Phase 1-6)
- Task type (research-design, implementation, experiment, analysis, documentation)
- Priority (P0-P3)
- Objective (clear, measurable goal)
- Approach (step-by-step methodology)
- Success criteria (how to verify completion)
- Dependencies (blocking tasks)
- Estimated effort (time estimate)

**Create a task**:
```bash
gh issue create --template research-task.yml
```

---

## 🔄 Execution Roadmap (14 Weeks)

See **`OPTIMIZATION_MASTERPLAN.md`** for complete week-by-week plan.

### Quick Summary
- **Week 1-2**: Foundation + Triage (fix ns-3 timing, classify Tamarin issues)
- **Week 3-6**: Parallel Deepening (Tamarin models, ns-3 full factorial, Literature)
- **Week 7-8**: Paper Integration (claims locked, complete draft v2)
- **Week 9-10**: Internal Review (polish & fix)
- **Week 11-13**: Artifact + Submission (prepare & submit)
- **Week 14**: Target submission to TDSC

---

## 🛠️ Key Tools & Agents

### Allocated Agents (Sisyphus-Junior instances)

| Agent | Responsibility | Weeks | Track |
|-------|-----------------|-------|-------|
| **[deep]** | Tamarin model deepening | 2-6 | Formal Verification |
| **[ultrabrain]** | Statistical analysis + claims integration | 5-8 | Analysis |
| **[artistry]** | Paper writing + figures | 3-10 | Writing |
| **librarian** | Literature discovery & annotation | 1-4 | Literature |
| **explore** | Code pattern discovery (on-demand) | 1, 11-12 | Utility |

### Commands & Skills

- **`gh` CLI** — GitHub operations (issues, PRs, labels, milestones)
- **`crane` YAML** — Paper metadata, reference management, task tracking
- **`bash`** — ns-3 simulation, Tamarin verification, data analysis
- **`lsp_*`** — Code navigation and refactoring (Tamarin, ns-3)
- **`ast_grep`** — Pattern matching across codebase

---

## ✅ Verification Checklist

Before each phase milestone, verify:

### Week 1 Completion
- [ ] ns-3 timing fix: PDR > 0% for baseline
- [ ] Tamarin triage: 3 issues classified with effort estimates
- [ ] Literature: 30+ candidates found

### Week 6 Completion (Evidence Locked)
- [ ] Tamarin: All models final, all proofs complete
- [ ] ns-3: All 360 experiments done, statistics computed
- [ ] Paper: 6/8 sections drafted
- [ ] Evidence: Every claim has supporting data

### Week 8 Completion (Full Draft v2)
- [ ] All chapters filled
- [ ] All figures/tables integrated
- [ ] No `{{PLACEHOLDER}}` tags remain
- [ ] LaTeX compiles cleanly

### Week 13 Completion (Pre-Submission)
- [ ] Artifact: Docker builds and runs successfully
- [ ] Paper: `crane_evaluate_q1_standards` score ≥ 80/100
- [ ] Claims: `crane_check_citations` all valid
- [ ] Submission package: Ready for TDSC

---

## 📊 Success Metrics

| Metric | Current | Target (TDSC) | Deadline |
|--------|---------|---------------|----------|
| Paper pages | 8 | 14 ± 0.5 | W14 |
| Figures | 0 | 7-8 | W8 |
| Tables | 8 | 8-10 | W8 |
| References | 0 | 20+ | W6 |
| Tamarin lines | 87+94 | 180+200 | W6 |
| Tamarin lemmas | 5 | 10+ | W6 |
| ns-3 scenarios | 27 | 360 | W6 |
| Readiness (self-eval) | 3.5/10 | 8-9/10 | W13 |

---

## 🚀 Next Steps

1. **Review `OPTIMIZATION_MASTERPLAN.md`** — understand the full 14-week plan
2. **Confirm 3 blocking questions** — see masterplan end section
3. **Start Week 1, Day 1** — execute ns-3 fix + Tamarin triage in parallel
4. **Track progress via GitHub Issues** — use research-task template

---

## 📞 Questions?

Refer to **`OPTIMIZATION_MASTERPLAN.md`** section "用户确认问题" for clarifications needed before Week 1.

---

**Ready to begin Week 1? Let's ship this paper.** 🚀
