# Docker Setup Guide for LoRa Mesh ns-3 Experiments

**Date Created**: 2026-03-29  
**Version**: 1.0  
**Purpose**: Consistent, reproducible environment for LoRa Mesh security research  
**Target Users**: Researchers continuing Phase 4 experiments

---

## 📋 Quick Start

```bash
# 1. Navigate to phase 4 directory
cd /home/augchao/Lora-Sec/phase4_ns3_simulation

# 2. Build Docker image (one-time, ~30-40 minutes)
docker build -f Dockerfile.lora-mesh -t lora-mesh-experiments:latest .

# 3. Run interactive container (all source code accessible)
docker run --gpus all \
  -v $(pwd):/workspace \
  -v $(pwd)/results:/results \
  -it lora-mesh-experiments:latest

# 4. Inside container: experiments run automatically via Python
# For manual execution:
./build/lora-mesh-sim [arguments]
```

---

## 🏗️ What's in the Docker Image

### Base Environment
- **OS**: Ubuntu 24.04 LTS
- **CUDA**: 12.8.0 (devel, for GPU support)
- **Compiler**: g++ (C++17 standard)

### Frameworks & Libraries
| Component | Version | Purpose |
|-----------|---------|---------|
| ns-3 | 3.40 | Network simulator core |
| ns3-core | 3.40 | Fundamental ns-3 classes |
| ns3-network | 3.40 | Network protocol stack |
| Python | 3.12+ | Experiment orchestration |

### Pre-installed Python Packages
- `numpy`, `pandas`, `scipy` — Data analysis
- `matplotlib` — Visualization
- `tqdm` — Progress tracking
- `psutil` — System monitoring
- `pyyaml` — Configuration files

### Custom Build
- **lora-mesh-sim** — Compiled binary (auto-compiled on first run)
  - Source: `scratch/lora-mesh-sim.cc`
  - Dependencies: `src/routing/loramesh-routing.*`, `src/attack/attacker-node.*`
  - Output: `build/lora-mesh-sim`

---

## 🔨 Build Instructions

### Prerequisites
- **Docker**: 20.10+ with GPU support
- **NVIDIA Container Toolkit**: For GPU access
- **Disk Space**: ~15GB free (ns-3 source + build)
- **Build Time**: 30-40 minutes on typical hardware

### Building the Image

```bash
cd /home/augchao/Lora-Sec/phase4_ns3_simulation

# Full rebuild (clean, always rebuilds)
docker build --no-cache -f Dockerfile.lora-mesh -t lora-mesh-experiments:latest .

# Incremental build (uses cache layers)
docker build -f Dockerfile.lora-mesh -t lora-mesh-experiments:latest .

# With specific ns-3 version (edit Dockerfile first)
docker build -f Dockerfile.lora-mesh -t lora-mesh-experiments:ns3.41 .
```

### Verifying the Build

```bash
# Check image exists
docker images | grep lora-mesh-experiments

# Test basic ns-3 installation
docker run --rm lora-mesh-experiments:latest bash -c \
  "pkg-config --modversion ns3-core && pkg-config --modversion ns3-network"

# Expected output:
# 3.40
# 3.40
```

---

## 🚀 Running Experiments

### Interactive Mode (for testing)

```bash
# Start container, get bash shell
docker run --gpus all \
  -v $(pwd):/workspace \
  -v $(pwd)/results:/results \
  -it lora-mesh-experiments:latest

# Inside container:
root@container:~# /usr/local/bin/compile-lora-mesh.sh
root@container:~# python3 scratch/lora-mesh-experiment-gpu-parallel.py --help
root@container:~# python3 scratch/lora-mesh-experiment-gpu-parallel.py \
    --topologies Linear Tree Grid \
    --attacks None Spoofing Replay Selective \
    --patches 0 1 \
    --seeds 1 \
    --workers 8
```

### Batch Mode (for full experiments)

```bash
# Run experiments without interactive shell
docker run --gpus all \
  --rm \
  -v $(pwd):/workspace \
  -v $(pwd)/results:/results \
  -e CUDA_VISIBLE_DEVICES=0 \
  lora-mesh-experiments:latest \
  bash -c "cd /workspace && python3 scratch/lora-mesh-experiment-gpu-parallel.py \
    --topologies Linear Tree Grid \
    --attacks None Spoofing Replay Selective \
    --patches 0 1 \
    --seeds 20 \
    --workers 10"
```

### Mounted Volumes Explained

| Mount | Host Path | Container Path | Purpose |
|-------|-----------|-----------------|---------|
| Source | `$(pwd)` | `/workspace` | Project code (ns-3 sim + Python scripts) |
| Results | `$(pwd)/results` | `/results` | Experiment output (JSON, CSV, logs) |

### GPU Access

```bash
# Check GPU is available inside container
docker run --gpus all lora-mesh-experiments:latest nvidia-smi

# Limit to specific GPU (e.g., GPU 1)
docker run --gpus '"device=1"' lora-mesh-experiments:latest nvidia-smi

# CPU-only mode (no GPU)
docker run lora-mesh-experiments:latest bash
```

---

## 🔄 Updating Experiment Code

**Workflow**: Edit locally, changes auto-reflected in container

```bash
# On HOST machine:
vim /home/augchao/Lora-Sec/phase4_ns3_simulation/scratch/lora-mesh-sim.cc

# In CONTAINER (mounted volume sees changes):
cd /workspace
/usr/local/bin/compile-lora-mesh.sh  # Recompiles automatically if source changed
./build/lora-mesh-sim [args]
```

**Smart Recompilation**:
- If `scratch/lora-mesh-sim.cc` is **newer** than `build/lora-mesh-sim` → recompile
- Otherwise → use cached binary (saves time)

---

## 📊 Running the Full Experiment Suite

### Day 2-3 Verification (27 scenarios)

```bash
cd /home/augchao/Lora-Sec/phase4_ns3_simulation

# Modified verify_fix.sh to use new Docker image
cat > verify_fix_docker.sh << 'VERIFY_SCRIPT'
#!/bin/bash

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
      --seeds 3 \
      --workers 10 \
      --duration 420
    python3 analyze_pdr.py
  "
VERIFY_SCRIPT

chmod +x verify_fix_docker.sh
./verify_fix_docker.sh
```

### Full 360-Scenario Run (Week 5-6)

```bash
# 3 topologies × 3 attacks × 2 patches × 20 seeds = 360 scenarios
# Expected runtime: 8-12 hours with 10 GPU workers

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
      --attacks None Spoofing Replay Selective \
      --patches 0 1 \
      --seeds 20 \
      --workers 10
    python3 analyze_pdr.py
  "
```

---

## 🐛 Troubleshooting

### Problem: GPU not detected

```bash
# Check NVIDIA drivers on host
nvidia-smi

# Check NVIDIA Container Toolkit
docker run --rm --gpus all nvidia/cuda:12.8.0-devel-ubuntu24.04 nvidia-smi

# If fails: install container toolkit
# Ubuntu: sudo apt install nvidia-docker2
# Then restart Docker: sudo systemctl restart docker
```

### Problem: Compilation fails (missing ns-3 headers)

```bash
# Inside container, check ns-3 installation
pkg-config --cflags --libs ns3-core ns3-network

# Should output flags like:
# -I/opt/ns-allinone-3.40/ns-3.40/build/include -L... -lns3.40-core-default ...

# If fails: rebuild Docker image
docker build --no-cache -f Dockerfile.lora-mesh -t lora-mesh-experiments:latest .
```

### Problem: Out of disk space

```bash
# Check Docker usage
docker system df

# Clean unused images/layers
docker system prune -a

# Remove old image versions
docker rmi lora-mesh-experiments:old-version
```

### Problem: Results directory not writable

```bash
# Inside container, results may be owned by root
# On HOST, fix permissions:
sudo chown -R $(whoami):$(whoami) $(pwd)/results

# Or mount with different user:
docker run --user $(id -u):$(id -g) ... lora-mesh-experiments:latest
```

---

## 📈 Performance Tuning

### CPU Workers

```bash
# Check available cores
nproc  # Host machine
docker run lora-mesh-experiments:latest nproc  # Inside container

# Adjust --workers flag (default: 10)
# More workers = faster, but uses more CPU/memory
python3 scratch/lora-mesh-experiment-gpu-parallel.py --workers 20
```

### GPU Memory

```bash
# Monitor GPU during run
docker run --gpus all lora-mesh-experiments:latest watch -n 1 nvidia-smi

# If out of memory: reduce workers or scenario size
python3 scratch/lora-mesh-experiment-gpu-parallel.py --workers 5
```

### Build Cache Management

```bash
# Force full rebuild (clears all cache layers)
docker build --no-cache -f Dockerfile.lora-mesh -t lora-mesh-experiments:latest .

# Incremental build (uses cache, faster for small changes)
docker build -f Dockerfile.lora-mesh -t lora-mesh-experiments:latest .
```

---

## 🔗 Environment Variables

### Inside Container

| Variable | Default | Purpose |
|----------|---------|---------|
| `NS3_PATH` | `/opt/ns-allinone-3.40/ns-3.40` | ns-3 installation directory |
| `LD_LIBRARY_PATH` | Includes `$NS3_PATH/build/lib` | Dynamic linker path |
| `PKG_CONFIG_PATH` | Includes `$NS3_PATH/build/lib/pkgconfig` | pkg-config search path |
| `CUDA_VISIBLE_DEVICES` | `0` | GPU device selection |
| `PYTHONUNBUFFERED` | `1` | Unbuffered Python output |

### Customizing at Runtime

```bash
# Change GPU
docker run --gpus all \
  -e CUDA_VISIBLE_DEVICES=1 \
  lora-mesh-experiments:latest

# Custom compilation flags (edit Dockerfile or rebuild)
# Rebuild with optimizations: replace -O2 with -O3 in Dockerfile
```

---

## 📝 Continuing Research: Step-by-Step

### Setup (One-time)

```bash
# 1. Clone/navigate to project
cd /home/augchao/Lora-Sec

# 2. Build Docker image
cd phase4_ns3_simulation
docker build -f Dockerfile.lora-mesh -t lora-mesh-experiments:latest .

# 3. Verify build
docker run --rm lora-mesh-experiments:latest bash -c \
  "pkg-config --modversion ns3-core && echo 'ns-3 ready'"
```

### Daily Workflow

```bash
# Terminal 1: Start experiment
cd /home/augchao/Lora-Sec/phase4_ns3_simulation
docker run --gpus all \
  -v $(pwd):/workspace \
  -v $(pwd)/results:/results \
  --name lora-exp-run-1 \
  lora-mesh-experiments:latest \
  bash -c "
    /usr/local/bin/compile-lora-mesh.sh
    python3 scratch/lora-mesh-experiment-gpu-parallel.py --workers 10
  "

# Terminal 2: Monitor results
cd /home/augchao/Lora-Sec/phase4_ns3_simulation
watch -n 5 'ls -lh results/ | tail -20'

# Terminal 3: Analyze in real-time (after some results)
python3 analyze_pdr.py
```

### Code Changes Workflow

```bash
# 1. Edit code locally (on host)
vim /home/augchao/Lora-Sec/phase4_ns3_simulation/scratch/lora-mesh-sim.cc

# 2. Re-run in container (auto-recompiles if changed)
docker run --gpus all \
  -v $(pwd):/workspace \
  -v $(pwd)/results:/results \
  lora-mesh-experiments:latest \
  bash -c "
    /usr/local/bin/compile-lora-mesh.sh  # Detects changes, recompiles
    python3 scratch/lora-mesh-experiment-gpu-parallel.py
  "
```

---

## 🎯 Key Takeaways

### Why This Docker Setup

✅ **Reproducibility**: Same ns-3 version, same libraries, same OS everywhere  
✅ **Isolation**: No conflicts with host Python, system libraries  
✅ **GPU Support**: CUDA 12.8 built-in, works with `--gpus all`  
✅ **Smart Caching**: Binary only recompiles when source changes  
✅ **Easy Updates**: Edit code locally, changes visible in container  

### When to Rebuild Image

- **Need ns-3 updates**: Edit Dockerfile, rebuild
- **Add Python packages**: Edit `pip install` line, rebuild
- **System library changes**: Modify `apt-get install`, rebuild

### When to Restart Container

- **Source code changes**: Container auto-detects and recompiles
- **Need fresh Python environment**: `exit` and `docker run` again
- **GPU driver updates**: Restart Docker daemon

---

## 📞 Getting Help

### Check ns-3 installation in container

```bash
docker run -it lora-mesh-experiments:latest bash

# Inside:
root@container:~# pkg-config --list-all | grep ns3
root@container:~# ls -la /opt/ns-allinone-3.40/ns-3.40/build/lib | head
```

### Check build logs

```bash
# Re-run with verbose output
docker run -it lora-mesh-experiments:latest bash -c \
  "/usr/local/bin/compile-lora-mesh.sh && \
   ./build/lora-mesh-sim --help"
```

### Check Python environment

```bash
docker run lora-mesh-experiments:latest python3 -c \
  "import numpy, pandas, matplotlib; print('All packages OK')"
```

---

## 📚 References

- [ns-3 Official Documentation](https://www.nsnam.org/documentation/)
- [NVIDIA Container Toolkit](https://github.com/NVIDIA/nvidia-docker)
- [Docker Best Practices](https://docs.docker.com/develop/dev-best-practices/)

---

**Last Updated**: 2026-03-29  
**Maintainer**: LoRa Mesh Security Research Team  
**Status**: ✅ Ready for Phase 4 experiments (Week 2-5)
