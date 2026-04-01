# Docker Setup for LoRa Mesh ns-3 Experiments

**Status**: ✅ Ready for Phase 4 experiments  
**Last Updated**: 2026-03-29  
**Version**: 1.0  

---

## 📚 Documentation Index

This guide contains complete Docker setup instructions for continuing LoRa Mesh research. **Read in order**:

1. **This file** — Quick start and overview
2. **DOCKER_SETUP_GUIDE.md** — Detailed Docker configuration and troubleshooting
3. **RUNNING_EXPERIMENTS.md** — How to execute experiments
4. **COMPILATION_DETAILS.md** — Technical details for developers
5. **QUICK_START.sh** — Executable script for common tasks

---

## 🚀 Fast Start (2 minutes)

```bash
cd /home/augchao/Lora-Sec/phase4_ns3_simulation

# 1. Build Docker image (one-time, 35-40 min)
docker build -f Dockerfile.lora-mesh -t lora-mesh-experiments:latest .

# 2. Test compilation
docker run --rm -v $(pwd):/workspace lora-mesh-experiments:latest \
  /usr/local/bin/compile-lora-mesh.sh
# Expected output: "✓ Binary ready: /workspace/build/lora-mesh-sim"

# 3. Run quick experiment (27 scenarios, ~1 hour with GPU)
docker run --gpus all \
  -v $(pwd):/workspace \
  -v $(pwd)/results:/results \
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

---

## 🎯 What's Been Fixed

### Problem
- Previous Docker image failed to compile `lora-mesh-sim`
- Error: `libns3-network.so.41: cannot open shared object file`
- Root cause: Missing source files in compilation, incorrect ns-3 library setup

### Solution
✅ **Dockerfile.lora-mesh** — New optimized image with:
- ns-3.40 built from source
- Smart compilation script that detects source changes
- Direct ns-3 library linking (no pkg-config)
- Includes Python analysis tools

✅ **Updated Compilation** — Now compiles all required files:
- `scratch/lora-mesh-sim.cc` (main)
- `src/routing/loramesh-routing.cc` (custom routing)
- `src/attack/attacker-node.cc` (attack models)

✅ **Smart Caching** — Binary only recompiles if source changed

---

## 📋 Documentation Files

| File | Purpose | Read When |
|------|---------|-----------|
| **DOCKER_SETUP_GUIDE.md** | Complete Docker reference | First time setup |
| **RUNNING_EXPERIMENTS.md** | Experiment execution guide | Before running experiments |
| **COMPILATION_DETAILS.md** | Technical architecture | Maintaining the build system |
| **QUICK_START.sh** | CLI tool | For automated builds/tests |
| **Dockerfile.lora-mesh** | Image definition | Modifying build configuration |

---

## ✅ Verification Checklist

After setting up Docker:

- [ ] Run `docker images | grep lora-mesh-experiments` — Image exists
- [ ] Run compilation test — Binary created successfully
- [ ] Check build directory — `build/lora-mesh-sim` exists and is executable
- [ ] Run quick experiment — Generates JSON results

---

## 🔧 One-Time Setup

```bash
# Navigate to phase 4
cd /home/augchao/Lora-Sec/phase4_ns3_simulation

# Build image (includes ns-3, Python tools, compilation setup)
# This takes 35-40 minutes
docker build -f Dockerfile.lora-mesh -t lora-mesh-experiments:latest .

# Verify image
docker images | grep lora-mesh-experiments
# Should show: lora-mesh-experiments | latest | ... | 15GB (approx)

# Test compilation (should complete in <5 seconds)
docker run --rm -v $(pwd):/workspace lora-mesh-experiments:latest \
  /usr/local/bin/compile-lora-mesh.sh

# Verify binary
ls -lh build/lora-mesh-sim
# Should show: -rwxr-xr-x ... 95K lora-mesh-sim
```

---

## 🎓 Understanding the Architecture

### Image Contents

```
Docker Image (lora-mesh-experiments:latest)
├── Base OS: Ubuntu 24.04 + CUDA 12.8
├── ns-3.40: Network simulator (pre-built)
├── Python 3.12: Experiment orchestration
├── Tools: numpy, pandas, matplotlib, scipy, etc.
└── compile-lora-mesh.sh: Smart compilation script
```

### Compilation Flow

```
Source Files (on host)
  ├── scratch/lora-mesh-sim.cc
  ├── src/routing/loramesh-routing.cc
  └── src/attack/attacker-node.cc
            ↓
        compile-lora-mesh.sh (inside Docker)
            ↓
    g++ -std=c++17 -O2 ... -lns3.40-core-optimized ...
            ↓
    build/lora-mesh-sim (binary)
            ↓
    Mounted back to host
```

### Experiment Workflow

```
Docker Container
  1. Auto-compile (if needed): /usr/local/bin/compile-lora-mesh.sh
  2. Run orchestration: python3 scratch/lora-mesh-experiment-gpu-parallel.py
  3. Save results: results/*.json
         ↓
    Mounted to: /home/augchao/Lora-Sec/phase4_ns3_simulation/results
```

---

## 💡 Key Concepts

### Smart Recompilation
- Binary only recompiles if source is newer
- Saves time when running multiple experiments
- Edit source locally → changes auto-visible in container

### GPU Support
- `--gpus all` flag enables NVIDIA GPU acceleration
- Requires NVIDIA Container Toolkit on host
- Falls back to CPU if GPU unavailable

### Volume Mounts
- `-v $(pwd):/workspace` → Source code visible in container
- Changes made locally instantly visible inside
- No need to rebuild image for code changes

---

## 🚀 Next Steps: Running Experiments

### For Day 2-3 verification (quick test)
```bash
./QUICK_START.sh run "Linear" "Spoofing" "0 1" "1" "10"
# 18 scenarios, ~1 hour
```

### For Week 5-6 full suite
```bash
./QUICK_START.sh run  # Uses defaults (360 scenarios, 8-12 hours)
# Or manually:
python3 scratch/lora-mesh-experiment-gpu-parallel.py \
  --topologies Linear Tree Grid \
  --attacks None Spoofing Replay Selective \
  --patches 0 1 \
  --seeds 20 \
  --workers 10
```

---

## 📞 Quick Troubleshooting

| Issue | Solution |
|-------|----------|
| "Image not found" | Run `docker build -f Dockerfile.lora-mesh ...` |
| "Compilation failed" | Check `/tmp/compile.log` inside container |
| "GPU not available" | Install NVIDIA Container Toolkit, then restart Docker |
| "Results directory empty" | Check Python script logs, verify `--workers` setting |
| "Out of disk space" | Run `docker system prune -a` |

---

## 📊 Performance Expectations

| Task | Time | Notes |
|------|------|-------|
| Build image | 35-40 min | One-time, uses cache after |
| Compile binary | <5 sec | Incremental if source unchanged |
| 27 scenarios (1 seed) | 30-60 min | With 10 GPU workers |
| 360 scenarios (20 seeds) | 8-12 hours | Full statistical suite |

---

## 🔐 Important Notes

- **Don't commit `build/` directory** — Always regenerate in Docker
- **Source files are your truth** — Keep `scratch/` and `src/` under version control
- **Results are ephemeral** — Save important result analysis to version control
- **Docker image is large** — ~15GB disk space required

---

## 📖 Full Documentation

For complete details, see:
- **DOCKER_SETUP_GUIDE.md** — Comprehensive Docker reference
- **RUNNING_EXPERIMENTS.md** — Detailed experiment instructions
- **COMPILATION_DETAILS.md** — Build system architecture

---

## ✨ Ready to Proceed

After following the "Fast Start" section above, you should have:
- ✅ Docker image built and verified
- ✅ Compilation working (binary created)
- ✅ Quick experiment tested (results generated)

You're now ready for Phase 4 experiments (Week 2-5)!

```bash
cd /home/augchao/Lora-Sec/phase4_ns3_simulation
# Next: See RUNNING_EXPERIMENTS.md for detailed guidance
```

---

**Questions?** Check the full documentation files listed above.  
**Ready to start?** Run your first experiment:
```bash
./QUICK_START.sh test  # Verify everything is setup
./QUICK_START.sh run   # Run quick experiment
```
