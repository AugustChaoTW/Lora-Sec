#!/bin/bash
# GPU-enabled Docker runner for LoRaMesher ns-3 experiments

set -e

PROJECT_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
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

# Run Docker container with GPU support and Python parallel script
echo "[INFO] Launching Docker container with GPU support..."
docker run --rm \
    $GPU_FLAG \
    -v "$PROJECT_ROOT:/workspace" \
    -v "$RESULTS_DIR:/results" \
    -e CUDA_VISIBLE_DEVICES=0 \
    -e PYTHONUNBUFFERED=1 \
    -w /workspace \
    "$IMAGE_NAME" \
    python3 phase4_ns3_simulation/scratch/lora-mesh-experiment-gpu-parallel.py

echo ""
echo "=========================================="
echo "Experiments completed!"
echo "Results saved to: $RESULTS_DIR"
echo "=========================================="
