# Running Experiments with Docker

**For**: LoRa Mesh ns-3 Phase 4 research  
**Quick Reference**: 5-minute setup, then hands-off execution

---

## 🚀 Fast Start (Copy & Paste)

```bash
cd /home/augchao/Lora-Sec/phase4_ns3_simulation

# 1. Build image (first time: 30-40 min)
docker build -f Dockerfile.lora-mesh -t lora-mesh-experiments:latest .

# 2. Run quick test (27 scenarios, 30-60 min)
docker run --gpus all \
  -v $(pwd):/workspace \
  -v $(pwd)/results:/results \
  -it lora-mesh-experiments:latest \
  bash -c "
    /usr/local/bin/compile-lora-mesh.sh
    cd /workspace
    python3 scratch/lora-mesh-experiment-gpu-parallel.py \
      --topologies Linear Tree Grid \
      --attacks None Spoofing Replay Selective \
      --patches 0 1 \
      --seeds 1 \
      --workers 10
  "
```

---

## 📊 Experiment Sizes

### Day 2-3: Quick Validation (27 scenarios, ~1 hour)
```bash
# Linear, Tree, Grid topologies
# 3 attacks: None, Spoofing, Replay
# 2 patches: baseline (0), patched (1)
# 1 seed (fast)
# Total: 3 × 3 × 2 × 1 = 18 scenarios + 9 no-attack = 27

python3 scratch/lora-mesh-experiment-gpu-parallel.py \
  --topologies Linear Tree Grid \
  --attacks None Spoofing Replay \
  --patches 0 1 \
  --seeds 1 \
  --workers 10
```

### Week 5-6: Full Suite (360 scenarios, 8-12 hours)
```bash
# Same topologies & attacks
# 20 seeds (statistical significance)
# Total: 3 × 3 × 2 × 20 = 360

python3 scratch/lora-mesh-experiment-gpu-parallel.py \
  --topologies Linear Tree Grid \
  --attacks None Spoofing Replay Selective \
  --patches 0 1 \
  --seeds 20 \
  --workers 10
```

### Custom: Any combination
```bash
python3 scratch/lora-mesh-experiment-gpu-parallel.py \
  --topologies Linear \
  --attacks Spoofing \
  --patches 0 \
  --seeds 5 \
  --workers 8
```

---

## 🔧 Setup Checklist

| Step | Command | Expected Output | Status |
|------|---------|-----------------|--------|
| GPU Check | `nvidia-smi` | NVIDIA GPU listed | ☐ |
| Docker Check | `docker --version` | Docker 20.10+ | ☐ |
| Build Image | `docker build ...` | `Successfully tagged` | ☐ |
| Test Compile | `docker run ... /usr/local/bin/compile-lora-mesh.sh` | `✓ Binary ready` | ☐ |
| Run Test | `python3 ... --seeds 1` | Results in `results/` | ☐ |

---

## 📂 Experiment Flow

```
Input
  ├─ scratch/lora-mesh-sim.cc (simulation binary)
  ├─ scratch/lora-mesh-experiment-gpu-parallel.py (orchestration)
  └─ src/ (routing, attack modules)
       │
       ▼
Docker Container (compile → run 10 parallel workers)
       │
       ▼
Output → results/
  ├─ {topology}_{attack}_patch={0|1}_seed={N}.json
  ├─ EXPERIMENTAL_RESULTS.json (summary)
  └─ EXPERIMENTAL_RESULTS.csv (tables)

Analysis
  └─ python3 analyze_pdr.py
```

---

## 📈 Monitoring Progress

### Terminal 1: Watch file count
```bash
watch -n 5 'ls -1 results/*.json | wc -l'
# Updates every 5s, shows running count
```

### Terminal 2: Monitor GPU
```bash
docker run --gpus all --rm nvidia/cuda:12.8.0-devel-ubuntu24.04 watch -n 1 nvidia-smi
```

### Terminal 3: Tail logs
```bash
tail -f results/*.log 2>/dev/null || echo "Waiting for logs..."
```

---

## 🎯 Key Experiment Strategies

### Strategy A: Fast Validation (Day 2-3)
- **Goal**: Verify fix works
- **Parameters**: 1 seed, 10 workers, 30-60 min runtime
- **Decision**: Proceed to full suite if PDR > 0%

### Strategy B: Statistical Significance (Week 5-6)
- **Goal**: Publication-ready results
- **Parameters**: 20 seeds, 10 workers, 8-12 hour runtime
- **Output**: Mean ± std for each configuration

### Strategy C: Targeted Investigation
- **Goal**: Debug specific attack
- **Parameters**: Single topology/attack combo, 5-10 seeds
- **Speed**: 20-30 min

---

## 🔄 Updating Code

**Workflow**: Edit locally → changes auto-visible in container

```bash
# On HOST (local machine)
vim /home/augchao/Lora-Sec/phase4_ns3_simulation/scratch/lora-mesh-sim.cc

# In CONTAINER (same session)
/usr/local/bin/compile-lora-mesh.sh  # Detects change, auto-recompiles
python3 scratch/lora-mesh-experiment-gpu-parallel.py --seeds 1  # Test fix
```

---

## 🐛 Troubleshooting

| Issue | Diagnosis | Fix |
|-------|-----------|-----|
| "GPU not detected" | `nvidia-smi` fails | Install NVIDIA Container Toolkit |
| "Compilation failed" | Check `/tmp/compile.log` | Verify `src/` files exist |
| "Results empty" | `ls results/` is empty | Check Python script logs |
| "Out of GPU memory" | `nvidia-smi` shows full | Reduce `--workers` flag |
| "Disk full" | `df -h /workspace` | Cleanup old `results/` |

---

## 📝 Quick Commands Reference

```bash
# Start fresh
docker run --gpus all -v $(pwd):/workspace -it lora-mesh-experiments:latest

# Compile only
docker run --rm -v $(pwd):/workspace lora-mesh-experiments:latest \
  /usr/local/bin/compile-lora-mesh.sh

# Run single topology
python3 scratch/lora-mesh-experiment-gpu-parallel.py \
  --topologies Linear --seeds 1

# Analyze results
python3 analyze_pdr.py

# Interactive container (for debugging)
docker run --gpus all -v $(pwd):/workspace -it lora-mesh-experiments:latest bash
```

---

## ✅ Verification Checklist (Post-Experiment)

- [ ] Results directory contains `.json` files
- [ ] `EXPERIMENTAL_RESULTS.json` has `summary` field
- [ ] PDR values are between 0-100%
- [ ] Patch (1) results show lower attack success than baseline (0)
- [ ] `analyze_pdr.py` runs without errors

---

**Status**: ✅ Ready for Phase 4 Week 2-5  
**Last Updated**: 2026-03-29
