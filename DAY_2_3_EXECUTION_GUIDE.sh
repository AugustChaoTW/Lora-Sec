#!/bin/bash
# Day 2-3 ns-3 Quick Verification Guide
# Purpose: Execute 27-scenario test to verify PDR fix
# Expected Runtime: 30-60 minutes with GPU
# Expected Outcome: PDR > 0% for all topologies

set -e

PROJECT_ROOT="/home/augchao/Lora-Sec"
PHASE4_DIR="$PROJECT_ROOT/phase4_ns3_simulation"
DOCKER_IMAGE="lora-mesh-experiments:latest"

echo "==============================================="
echo "Week 2 Day 2-3: ns-3 Quick Verification"
echo "==============================================="
echo ""
echo "This script will:"
echo "1. Verify Docker image is built"
echo "2. Run 27-scenario quick test"
echo "3. Analyze PDR results"
echo ""

# Check Docker image
if ! docker images --quiet "$DOCKER_IMAGE" &>/dev/null; then
    echo "❌ Docker image not found: $DOCKER_IMAGE"
    echo "Build it first:"
    echo "  cd $PHASE4_DIR"
    echo "  docker build -f Dockerfile.lora-mesh -t $DOCKER_IMAGE ."
    exit 1
fi

echo "✅ Docker image found"

# Run verification
echo ""
echo "Running 27-scenario quick test..."
echo "Topologies: Linear, Tree, Grid"
echo "Attacks: None, Spoofing, Replay"
echo "Patches: 0 (baseline), 1 (patched)"
echo "Seeds: 1 (single seed for speed)"
echo ""

cd "$PHASE4_DIR"

docker run --gpus all \
  -v "$(pwd):/workspace" \
  -v "$(pwd)/results:/results" \
  -e CUDA_VISIBLE_DEVICES=0 \
  -e PYTHONUNBUFFERED=1 \
  --name lora-verify-day2 \
  "$DOCKER_IMAGE" \
  bash -c "
    set -e
    echo '[Day 2-3] Compiling lora-mesh-sim...'
    /usr/local/bin/compile-lora-mesh.sh
    
    echo '[Day 2-3] Running 27-scenario test...'
    cd /workspace
    python3 scratch/lora-mesh-experiment-gpu-parallel.py \
      --topologies Linear Tree Grid \
      --attacks None Spoofing Replay \
      --patches 0 1 \
      --seeds 1 \
      --workers 10 \
      --duration 420
    
    echo '[Day 2-3] Running analysis...'
    python3 analyze_pdr.py
  "

# Check results
echo ""
echo "==============================================="
echo "✅ Day 2-3 Verification Complete"
echo "==============================================="
echo ""
echo "Results saved to: $(pwd)/results/"
echo ""
echo "Checking results..."

result_count=$(find results -name "*.json" -type f | wc -l)
echo "Found $result_count result files (expected ~27)"

if [ "$result_count" -ge 27 ]; then
    echo "✅ All 27 scenarios completed"
else
    echo "⚠️  Only $result_count scenarios completed (expected 27)"
fi

echo ""
echo "Next steps:"
echo "1. Review results in results/ directory"
echo "2. Check analyze_pdr.py output for statistics"
echo "3. Verify PDR > 0% for all topologies"
echo "4. If successful: proceed to Day 5 milestone decision"
echo "5. If failed: review Docker logs and DOCKER_SETUP_GUIDE.md"
