# Compilation Details & Architecture

**For**: Developers maintaining the build system  
**Scope**: Docker image, compilation flags, dependencies

---

## 📦 Dependency Tree

```
lora-mesh-sim (binary)
├── Standard Library
│   ├── <fstream>, <sstream>, <iomanip>
│   ├── <vector>, <map>, <set>
│   └── <algorithm>, <random>, <optional>
│
├── ns-3 Core (pkg-config ns3-core)
│   ├── ns3/core-module.h (Simulator, RNG, Time)
│   ├── ns3/command-line.h (CommandLine parsing)
│   └── ns3/network-module.h (NodeContainer)
│
└── Custom Modules (src/)
    ├── src/common/sim-types.h (data structures, no deps)
    ├── src/routing/loramesh-routing.{h,cc} (Bellman-Ford)
    └── src/attack/attacker-node.{h,cc} (attack models)
```

---

## 🔨 Compilation Process

### Step 1: Environment Setup (Dockerfile)

```dockerfile
# Install minimal tools
apt-get install build-essential cmake python3 pkg-config

# Download ns-3.40
wget https://www.nsnam.org/release/ns-allinone-3.40.tar.bz2

# Build ns-3.40
./ns3 configure --build-profile=optimized --disable-tests
./ns3 build

# Set environment
export LD_LIBRARY_PATH=/opt/ns-allinone-3.40/ns-3.40/build/lib
export PKG_CONFIG_PATH=/opt/ns-allinone-3.40/ns-3.40/build/lib/pkgconfig
```

### Step 2: Check Dependencies (at runtime)

```bash
pkg-config --cflags --libs ns3-core ns3-network
# Output: -I/opt/ns-allinone-3.40/ns-3.40/build/include -L/opt/ns-allinone-3.40/ns-3.40/build/lib -lns3.40-core-default -lns3.40-network-default
```

### Step 3: Compile Binary (in compile-lora-mesh.sh)

```bash
g++ -std=c++17 -O2 \
    -I${NS3_PATH}/build/include \
    -I/workspace/src \
    -L${NS3_PATH}/build/lib \
    /workspace/scratch/lora-mesh-sim.cc \
    /workspace/src/routing/loramesh-routing.cc \
    /workspace/src/attack/attacker-node.cc \
    -o /workspace/build/lora-mesh-sim \
    -Wl,-rpath=${NS3_PATH}/build/lib \
    $(pkg-config --cflags --libs ns3-core ns3-network)
```

**Flags Explained**:
- `-std=c++17` — Modern C++ standard
- `-O2` — Optimization level (balance speed/build time)
- `-I${NS3_PATH}/build/include` — ns-3 headers
- `-I/workspace/src` — Custom headers
- `-L${NS3_PATH}/build/lib` — Library search path
- `-Wl,-rpath=...` — Embed runtime library path (portability)
- `$(pkg-config ...)` — Link with ns3-core, ns3-network

### Step 4: Smart Caching

```bash
# Timestamp check: recompile only if source newer than binary
if [ "$source" -nt "$binary" ]; then
    g++ ... # recompile
else
    echo "Using cached binary"
fi
```

---

## 🎯 Why This Architecture

### Problem: Previous failures

1. **Incomplete .pc files** — Dockerfile only created ns3-core.pc, ns3-network.pc
2. **Wrong ns-3 modules** — Tried linking internet, mobility, applications (not used)
3. **Missing source files** — Only compiled main file, forgot routing/attack .cc

### Solution: Simplified design

✅ **ns-3 modules** — Use only what's needed (core, network)  
✅ **Custom compilation** — Explicit list of source files  
✅ **Smart caching** — Timestamp-based recompilation  
✅ **Portable runtime** — `-Wl,-rpath=` embeds library paths  

---

## 📊 Build Performance

| Stage | Duration | Cached? |
|-------|----------|---------|
| System packages (apt) | 2-3 min | 30 days |
| ns-3 download | 1-2 min | Always |
| ns-3 build | 20-30 min | Every build |
| Dockerfile finish | 1 min | Yes |
| **Total first build** | **~35 min** | — |
| **Total incremental** | **<1 sec** | Only `compile-lora-mesh.sh` |

---

## 🔄 Maintenance: What to Update

### Adding new source files

```bash
# 1. Add to compile-lora-mesh.sh SOURCES array
SOURCES=(
    /workspace/scratch/lora-mesh-sim.cc
    /workspace/src/routing/loramesh-routing.cc
    /workspace/src/attack/attacker-node.cc
    /workspace/src/new-module/new-module.cc  # NEW
)

# 2. Rebuild doesn't need Dockerfile change (auto-detects timestamp)
```

### Upgrading ns-3 version

```dockerfile
# In Dockerfile.lora-mesh, change L34:
RUN wget -q https://www.nsnam.org/release/ns-allinone-3.41.tar.bz2
```

### Adding Python packages

```dockerfile
# In Dockerfile.lora-mesh, update pip install L61-68:
RUN pip3 install --no-cache-dir --break-system-packages \
    numpy pandas scipy matplotlib tqdm psutil pyyaml \
    new-package  # NEW
```

---

## 🧪 Verification

### Quick test after build

```bash
# Test ns-3 is available
docker run --rm lora-mesh-experiments:latest bash -c \
  "pkg-config --modversion ns3-core"
# Expected: 3.40

# Test compilation
docker run --rm -v $(pwd):/workspace lora-mesh-experiments:latest \
  /usr/local/bin/compile-lora-mesh.sh
# Expected: ✓ Binary ready

# Test binary runs
docker run --rm -v $(pwd):/workspace lora-mesh-experiments:latest \
  ./build/lora-mesh-sim --help
# Expected: command-line help output
```

---

## 📈 Scaling: Running Many Experiments

### Current approach: Python multiprocessing

```python
# scratch/lora-mesh-experiment-gpu-parallel.py
with ProcessPoolExecutor(max_workers=10) as executor:
    futures = [executor.submit(run_scenario, ...) for ...]
    results = [f.result() for f in as_completed(futures)]
```

### If 360 scenarios is too slow

**Option A**: More workers (default 10, max ~20)
```bash
--workers 20  # Uses more CPU, faster
```

**Option B**: Distributed experiments (future)
```bash
# Run on multiple machines in parallel
# Aggregate results with analyze_pdr.py
```

---

## 🔐 Security Considerations

### Container isolation

- Build happens in isolated container
- No host access to /opt/ns-allinone-3.40 
- Source code mounted read-write for development

### GPU access

- Requires NVIDIA Container Toolkit
- `--gpus all` enables GPU but isolates to container

---

## 📚 File Reference

| File | Purpose | Editable? |
|------|---------|-----------|
| `Dockerfile.lora-mesh` | Image definition | Rarely (upgrades) |
| `compile-lora-mesh.sh` | Compilation script (in Dockerfile) | When adding files |
| `scratch/lora-mesh-sim.cc` | Main simulation | Always |
| `src/routing/loramesh-routing.cc` | Routing implementation | Always |
| `src/attack/attacker-node.cc` | Attack models | Always |
| `scratch/lora-mesh-experiment-gpu-parallel.py` | Experiment orchestration | Rarely |

---

**Status**: ✅ Production-ready  
**Last Updated**: 2026-03-29  
**Maintainer**: LoRa Mesh Research Team
