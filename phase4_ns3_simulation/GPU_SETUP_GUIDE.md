# Phase 4: GPU Environment Setup & Execution Guide

**Date**: 2026-03-29  
**Status**: ✅ Ready for GPU execution  
**Hardware**: NVIDIA GB10 + 20 CPU cores  
**Expected Speedup**: 6-12× (parallel multiprocess execution)

---

## 🚀 Quick Start (5 minutes)

```bash
# Navigate to project root
cd /home/augchao/Lora-Sec

# Make docker runner executable
chmod +x phase4_ns3_simulation/docker-run-gpu.sh

# Run GPU-accelerated experiments
./phase4_ns3_simulation/docker-run-gpu.sh
```

**Expected Output**:
- Docker image built (if needed)
- 27 experiment scenarios run in parallel (10 workers)
- Results saved to `phase4_ns3_simulation/results/`
- Total execution time: **1-2 hours** (vs 11-19 hours sequential)

---

## 📋 System Requirements

### Hardware
- ✅ NVIDIA GPU (GB10 detected on this system)
- ✅ 20 CPU cores (will use 10 for parallel execution)
- ✅ 8+ GB RAM

### Software
- ✅ Docker (with NVIDIA runtime support)
- ✅ nvidia-docker (or docker with `--gpus` flag)
- ✅ NVIDIA driver (v580.126.09 detected)
- ✅ CUDA 12.8 (available in Docker image)

### Verification

```bash
# Check NVIDIA driver
nvidia-smi

# Check Docker GPU support
docker run --rm --gpus all nvidia/cuda:12.8.0-base-ubuntu24.04 nvidia-smi

# Check ns-3 availability
which ns3
pkg-config --cflags ns3-core
```

---

## 🐳 Docker Environment Setup

### Automatic Setup (Recommended)

The `docker-run-gpu.sh` script handles everything:

```bash
./phase4_ns3_simulation/docker-run-gpu.sh
```

**What it does**:
1. ✅ Checks GPU availability
2. ✅ Builds Docker image if needed (Dockerfile.gpu)
3. ✅ Launches container with `--gpus all`
4. ✅ Mounts project directory to `/workspace`
5. ✅ Runs parallel experiment executor
6. ✅ Saves results to `phase4_ns3_simulation/results/`

### Manual Docker Execution

If you prefer to run Docker manually:

```bash
# Build image
docker build -f phase4_ns3_simulation/Dockerfile.gpu \
             -t lora-mesh-ns3-gpu:latest \
             phase4_ns3_simulation/

# Run with GPU support
docker run --rm \
    --gpus all \
    -v /home/augchao/Lora-Sec:/workspace \
    -e CUDA_VISIBLE_DEVICES=0 \
    lora-mesh-ns3-gpu:latest \
    python3 phase4_ns3_simulation/scratch/lora-mesh-experiment-gpu-parallel.py
```

---

## 🔧 Experiment Execution

### Script: `lora-mesh-experiment-gpu-parallel.py`

**Features**:
- ✅ Multiprocessing with 10 parallel workers
- ✅ Automatic GPU detection
- ✅ CUDA compilation flags for optimization
- ✅ JSON result output per experiment
- ✅ Summary statistics and reporting

**Configuration** (can be modified in script):

```python
num_workers = 10          # Parallel processes (adjust as needed)
timeout_per_sim = 120     # 2 minutes per simulation
total_duration = 300      # 5-minute simulations
hello_period = 10         # 10-second Hello broadcast period
sample_period = 10        # 10-second sampling interval
```

### Experiment Scenarios

**Total**: 27 experiments (3 topologies × 3 attacks × 3 runs)

| Topology | Attacks | Runs | Total |
|----------|---------|------|-------|
| Linear | Spoofing, Replay, Selective | 3 | 9 |
| Tree | Spoofing, Replay, Selective | 3 | 9 |
| Grid | Spoofing, Replay, Selective | 3 | 9 |
| **TOTAL** | **9 scenarios** | **3** | **27** |

**Expected Results**:

| Scenario | Baseline Success | Time per Run |
|----------|-----------------|--------------|
| Attack Spoofing | 70-80% | ~30-45s |
| Attack Replay | 60-75% | ~30-45s |
| Attack Selective | 65-80% | ~30-45s |

---

## 📊 Results Format

### Output Structure

```
phase4_ns3_simulation/results/
├── Linear_Spoofing_patch=0_run=0.json
├── Linear_Spoofing_patch=0_run=1.json
├── Linear_Spoofing_patch=0_run=2.json
├── Linear_Replay_patch=0_run=0.json
├── ... (27 JSON files)
└── EXPERIMENTAL_RESULTS.json          # Aggregated summary
```

### JSON Result Format

```json
{
  "scenario": {
    "topology": "Linear",
    "attack_type": "Spoofing",
    "run_id": 0,
    "use_patch": false
  },
  "success_rate": 75.3,
  "avg_latency": 45.2,
  "throughput": 95.4,
  "pdr": 94.8,
  "completion_time": 42.5,
  "error": null
}
```

### Summary Statistics

The script generates a summary table:

```
Topology   Attack        Success Rate      Latency (ms)  PDR (%)
-------------------------------------------------------------------
Linear     Replay        72.3% ± 5.2%      45.2 ms       94.8%
Linear     Selective     78.1% ± 3.8%      46.1 ms       91.2%
Linear     Spoofing      75.6% ± 4.5%      44.9 ms       93.5%
Tree       Replay        68.5% ± 6.1%      52.3 ms       89.4%
... (9 total scenarios)
```

---

## ⚡ Performance Optimization

### GPU Acceleration

The Dockerfile includes CUDA 12.8 support:
- ✅ CUDA development tools for compilation
- ✅ NVIDIA CUDA runtime (12.8)
- ✅ Optimization flags: `-O3 -DCUDA_ENABLED` (if CUDA detected)

### CPU Parallelization

The Python script uses `multiprocessing.Pool`:
- ✅ 10 worker processes (adjustable)
- ✅ Independent experiment execution
- ✅ Automatic load balancing

### Expected Execution Time

| Configuration | Time | Speedup |
|---------------|------|---------|
| Sequential (1 worker) | 11-19h | 1× |
| CPU Parallel (10 workers) | 1.5-2h | **6-12×** |
| GPU + Parallel | 1-1.5h | **8-15×** (if CUDA optimizes) |

---

## 🔍 Troubleshooting

### Issue: Docker GPU not recognized

**Solution**:
```bash
# Verify NVIDIA Docker support
docker run --rm --gpus all nvidia/cuda:12.8.0-base-ubuntu24.04 nvidia-smi

# If fails, install nvidia-docker
sudo apt-get install nvidia-docker2
sudo systemctl restart docker
```

### Issue: Out of Memory

**Solution**:
```bash
# Reduce number of parallel workers
# Edit docker-run-gpu.sh and change:
# num_workers=5  (instead of 10)

# Or limit Docker memory:
docker run --rm --gpus all -m 4g <image>
```

### Issue: Simulation timeout

**Solution**:
```bash
# Increase timeout in Python script
timeout_per_sim = 180  # 3 minutes instead of 2

# Or increase simulation efficiency flags
--duration=300 --helloPeriod=20 --samplePeriod=20
```

---

## 📈 Next Steps

### After Experiments Complete

1. **Verify Results**:
   ```bash
   ls -lah phase4_ns3_simulation/results/
   cat phase4_ns3_simulation/results/EXPERIMENTAL_RESULTS.json | python3 -m json.tool
   ```

2. **Analyze Results**:
   - Compare attack success rates (baseline vs scenarios)
   - Check latency and PDR variations
   - Verify convergence to expected values (70-80% spoofing, 60-75% replay)

3. **Generate Figures** (Phase 5):
   - Attack success rate bar charts
   - Latency/throughput box plots
   - Topology comparison matrices

4. **Document Findings** (Phase 5):
   - Create `EXPERIMENTAL_RESULTS_BASELINE.md`
   - Integrate results into paper (Section 8)
   - Prepare for IEEE submission

---

## 🛠️ Advanced Configuration

### Custom Experiment Parameters

Edit `lora-mesh-experiment-gpu-parallel.py`:

```python
# Line 233-245: Adjust experiment scenarios
topologies = ["Linear", "Tree", "Grid", "Star"]  # Add custom topologies
attack_types = ["Spoofing", "Replay", "Selective", "Delay"]  # Add custom attacks
runs = [0, 1, 2, 3, 4]  # Increase statistical samples

# Line 55: Adjust parallel workers
self.num_workers = min(15, cpu_count() - 2)  # Use more cores

# Line 170: Adjust simulation duration
"--duration=600",  # Longer simulation (10 min)
```

### Custom Docker Image

Extend `Dockerfile.gpu` for additional tools:

```dockerfile
# Add after ns-3 installation
RUN pip3 install --no-cache-dir \
    plotly \
    seaborn \
    jupyter
```

Then rebuild:
```bash
docker build -f phase4_ns3_simulation/Dockerfile.gpu \
             -t lora-mesh-ns3-gpu:custom \
             phase4_ns3_simulation/
```

---

## 📞 Support

**Issues or Questions**:
1. Check logs: `phase4_ns3_simulation/results/*.log` (if generated)
2. Review error messages from Docker output
3. Verify GPU/CUDA installation: `nvidia-smi`
4. Check ns-3 installation: `pkg-config --list-all | grep ns3`

---

**Status**: ✅ **READY FOR EXECUTION**

Expected start time: Immediately  
Expected completion: **1-2 hours** (vs 11-19 hours)  
Result quality: **Full** (all 27 experiments with detailed metrics)

🚀 **Run command**: `./phase4_ns3_simulation/docker-run-gpu.sh`
