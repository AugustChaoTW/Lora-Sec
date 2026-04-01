#!/bin/bash
# GPU-enabled Docker runner for LoRaMesher ns-3 experiments

set -e

PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
PHASE4_DIR="$PROJECT_ROOT/phase4_ns3_simulation"
RESULTS_DIR="$PHASE4_DIR/results"

echo "=========================================="
echo "LoRaMesher ns-3 GPU Experiment Runner"
echo "=========================================="
echo "Project root: $PROJECT_ROOT"
echo "Phase 4 dir:  $PHASE4_DIR"
echo "Results dir:  $RESULTS_DIR"
echo ""

# Create results directory
mkdir -p "$RESULTS_DIR"

# Check GPU availability
echo "[INFO] Checking GPU availability..."
if command -v nvidia-smi &> /dev/null; then
    echo "[INFO] NVIDIA GPU detected:"
    nvidia-smi --query-gpu=index,name,memory.total --format=csv,noheader || true
    GPU_FLAG="--gpus all"
else
    echo "[WARN] No NVIDIA GPU detected, running on CPU"
    GPU_FLAG=""
fi

# Build Docker image (if not exists)
IMAGE_NAME="lora-mesh-ns3-gpu:latest"
if ! docker images --quiet "$IMAGE_NAME" &> /dev/null; then
    echo "[INFO] Building Docker image: $IMAGE_NAME"
    docker build -f "$PHASE4_DIR/Dockerfile.gpu" -t "$IMAGE_NAME" "$PHASE4_DIR"
else
    echo "[INFO] Using existing Docker image: $IMAGE_NAME"
fi

# Prepare arguments for Python script
PYTHON_ARGS=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --topologies)
            PYTHON_ARGS="$PYTHON_ARGS --topologies ${2//,/ }"
            shift 2
            ;;
        --attacks)
            PYTHON_ARGS="$PYTHON_ARGS --attacks ${2//,/ }"
            shift 2
            ;;
        --patches)
            PYTHON_ARGS="$PYTHON_ARGS --patches $2"
            shift 2
            ;;
        --seeds)
            PYTHON_ARGS="$PYTHON_ARGS --seeds $2"
            shift 2
            ;;
        --workers)
            PYTHON_ARGS="$PYTHON_ARGS --workers $2"
            shift 2
            ;;
        --duration)
            PYTHON_ARGS="$PYTHON_ARGS --duration $2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

# Run Docker container with GPU support
echo "[INFO] Launching Docker container with GPU support..."
echo "[INFO] Will compile lora-mesh-sim inside container..."

COMPILE_AND_RUN=$(cat << 'DOCKER_SCRIPT'
#!/bin/bash
set -e

NS3_PATH=/opt/ns-allinone-3.40/ns-3.40
export LD_LIBRARY_PATH=$NS3_PATH/build/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$NS3_PATH/build/lib/pkgconfig:$PKG_CONFIG_PATH

# Verify ns-3 libraries exist
if [ ! -d "$NS3_PATH/build/lib" ]; then
    echo "[ERROR] ns-3 libraries not found in $NS3_PATH/build/lib"
    exit 1
fi

# Compile lora-mesh-sim if not already compiled
if [ ! -f "/workspace/build/lora-mesh-sim" ] || [ "/workspace/scratch/lora-mesh-sim.cc" -nt "/workspace/build/lora-mesh-sim" ]; then
    echo "[INFO] Compiling lora-mesh-sim..."
    mkdir -p /workspace/build
    cd /workspace
    
    g++ -std=c++17 -O2 \
        -I$NS3_PATH/build/include \
        -I/workspace/src \
        -L$NS3_PATH/build/lib \
        /workspace/scratch/lora-mesh-sim.cc \
        -o /workspace/build/lora-mesh-sim \
        -Wl,-rpath=$NS3_PATH/build/lib \
        $(pkg-config --cflags --libs ns3-core ns3-network 2>/dev/null) \
        2>&1 | tee /tmp/compile.log && tail -20 /tmp/compile.log
    
    if [ ! -f "/workspace/build/lora-mesh-sim" ]; then
        echo "[ERROR] Compilation failed!"
        exit 1
    fi
    echo "[INFO] ✓ lora-mesh-sim compiled successfully"
else
    echo "[INFO] Using cached lora-mesh-sim binary"
fi

# Run experiments
echo "[INFO] Running experiments..."
cd /workspace
python3 phase4_ns3_simulation/scratch/lora-mesh-experiment-gpu-parallel.py $PYTHON_ARGS
DOCKER_SCRIPT
)

docker run --rm \
    $GPU_FLAG \
    -v "$PROJECT_ROOT:/workspace" \
    -v "$RESULTS_DIR:/results" \
    -e CUDA_VISIBLE_DEVICES=0 \
    -e PYTHONUNBUFFERED=1 \
    -w /workspace \
    "$IMAGE_NAME" \
    bash -c "$COMPILE_AND_RUN"

echo ""
echo "=========================================="
echo "Experiments completed!"
echo "Results saved to: $RESULTS_DIR"
echo "=========================================="
