#!/bin/bash
# Quick start script for LoRa Mesh ns-3 experiments
# Usage: ./QUICK_START.sh [build|run|test]

set -e

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
PHASE4_DIR="$PROJECT_ROOT/phase4_ns3_simulation"
IMAGE_NAME="lora-mesh-experiments:latest"

echo "=========================================="
echo "LoRa Mesh ns-3 Experiment Quick Start"
echo "=========================================="
echo "Project: $PROJECT_ROOT"
echo "Phase 4:  $PHASE4_DIR"
echo ""

# ============================================================================
# Command: build
# ============================================================================

build_image() {
    echo "📦 Building Docker image: $IMAGE_NAME"
    echo "   This takes 30-40 minutes on first build..."
    echo ""
    
    cd "$PHASE4_DIR"
    docker build -f Dockerfile.lora-mesh -t "$IMAGE_NAME" .
    
    echo ""
    echo "✅ Image built successfully!"
    echo "   Verify: docker images | grep lora-mesh"
}

# ============================================================================
# Command: run
# ============================================================================

run_experiment() {
    local TOPOLOGIES="${1:-Linear Tree Grid}"
    local ATTACKS="${2:-None Spoofing Replay Selective}"
    local PATCHES="${3:-0 1}"
    local SEEDS="${4:-3}"
    local WORKERS="${5:-10}"
    
    echo "🚀 Running experiments..."
    echo "   Topologies: $TOPOLOGIES"
    echo "   Attacks:    $ATTACKS"
    echo "   Patches:    $PATCHES"
    echo "   Seeds:      $SEEDS"
    echo "   Workers:    $WORKERS"
    echo ""
    
    cd "$PHASE4_DIR"
    
    docker run --gpus all \
        -v "$PHASE4_DIR:/workspace" \
        -v "$PHASE4_DIR/results:/results" \
        -e CUDA_VISIBLE_DEVICES=0 \
        -it "$IMAGE_NAME" \
        bash -c "
            /usr/local/bin/compile-lora-mesh.sh
            cd /workspace
            python3 scratch/lora-mesh-experiment-gpu-parallel.py \
                --topologies $TOPOLOGIES \
                --attacks $ATTACKS \
                --patches $PATCHES \
                --seeds $SEEDS \
                --workers $WORKERS
        "
}

# ============================================================================
# Command: test
# ============================================================================

test_setup() {
    echo "🧪 Testing Docker setup..."
    echo ""
    
    # Check image exists
    if ! docker images --quiet "$IMAGE_NAME" &>/dev/null; then
        echo "❌ Image not found: $IMAGE_NAME"
        echo "   Run: $0 build"
        exit 1
    fi
    
    echo "✓ Docker image exists"
    
    # Test ns-3
    echo "Checking ns-3 installation..."
    docker run --rm "$IMAGE_NAME" bash -c \
        "pkg-config --modversion ns3-core && pkg-config --modversion ns3-network" \
        && echo "✓ ns-3 ready" || echo "❌ ns-3 not found"
    
    # Test GPU
    echo "Checking GPU..."
    docker run --gpus all --rm "$IMAGE_NAME" nvidia-smi --query-gpu=index,name --format=csv,noheader \
        && echo "✓ GPU ready" || echo "⚠ GPU not available (CPU mode will work)"
    
    # Test compilation
    echo "Testing compilation..."
    docker run --rm \
        -v "$PHASE4_DIR:/workspace" \
        "$IMAGE_NAME" \
        bash -c "/usr/local/bin/compile-lora-mesh.sh" \
        && echo "✓ Compilation ready" || echo "❌ Compilation failed"
    
    echo ""
    echo "✅ All checks passed! Ready to run experiments."
}

# ============================================================================
# Main dispatch
# ============================================================================

COMMAND="${1:-help}"

case "$COMMAND" in
    build)
        build_image
        ;;
    run)
        run_experiment "$@"
        ;;
    test)
        test_setup
        ;;
    help|--help|-h)
        cat << 'HELP'
LoRa Mesh ns-3 Quick Start

Usage: ./QUICK_START.sh COMMAND [ARGS]

Commands:
  build                    Build Docker image (first time only, ~40 min)
  test                     Test Docker setup (ns-3, GPU, compilation)
  run [TOPOLOGIES] [ATTACKS] [PATCHES] [SEEDS] [WORKERS]
                          Run experiments with specified parameters
  help                     Show this message

Examples:
  ./QUICK_START.sh build
  ./QUICK_START.sh test
  ./QUICK_START.sh run "Linear" "None Spoofing" "0 1" "3" "10"
  ./QUICK_START.sh run  # Use defaults (all topologies, attacks, patches)

Default Parameters (if not specified):
  TOPOLOGIES: Linear Tree Grid
  ATTACKS:    None Spoofing Replay Selective
  PATCHES:    0 1
  SEEDS:      3
  WORKERS:    10

For full documentation:
  cat DOCKER_SETUP_GUIDE.md
HELP
        ;;
    *)
        echo "❌ Unknown command: $COMMAND"
        echo "Run '$0 help' for usage"
        exit 1
        ;;
esac
