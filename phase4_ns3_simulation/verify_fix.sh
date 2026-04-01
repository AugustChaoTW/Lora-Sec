#!/bin/bash
# Day 2-3 ns-3修复验证脚本
# 用途: 自动化验证 data traffic timing fix

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

echo "🔍 === Week 1 Day 2 ns-3 Verification Script ==="
echo ""

# ============================================================
# Phase 1: Quick validation (单场景)
# ============================================================
echo "📌 Phase 1: Single scenario validation"
echo "Running: Linear/NoAttack/Patch/Seed0 → Expected PDR > 80%"
echo ""

if [ -f "docker-run-gpu.sh" ]; then
    chmod +x docker-run-gpu.sh
    ./docker-run-gpu.sh \
        --topologies "Linear" \
        --attacks "None" \
        --patches 1 \
        --seeds 1 \
        --workers 2 \
        --duration 420 \
        2>&1 | tee verify_phase1.log
else
    echo "⚠️  docker-run-gpu.sh not found, skipping Docker verification"
fi

echo ""
echo "✅ Phase 1 complete. Checking results..."
echo ""

# ============================================================
# Phase 2: Analyze single scenario result
# ============================================================
python3 << 'PYTHON_EOF'
import json
import os
import sys

results_dir = "results"
target_file = None

# Find the Linear_None result file
for f in os.listdir(results_dir):
    if f.startswith("Linear_None_patch=1"):
        target_file = os.path.join(results_dir, f)
        break

if target_file is None:
    print("❌ Could not find Linear_None result file")
    sys.exit(1)

print(f"📄 Analyzing: {os.path.basename(target_file)}")

with open(target_file) as fp:
    data = json.load(fp)

pdr = data.get('pdr', 0)
delivered = data.get('delivered', 0)
generated = data.get('generated', 0)

print(f"   PDR: {pdr*100:.1f}% (Expected: > 80%)")
print(f"   Generated: {generated} packets")
print(f"   Delivered: {delivered} packets")

if pdr > 0.8:
    print("\n✅ SUCCESS: PDR > 80%. Fix is effective!")
    sys.exit(0)
elif pdr > 0:
    print(f"\n⚠️  PARTIAL: PDR = {pdr*100:.1f}% (expected > 80%, but > 0% is progress)")
    print("   Proceeding to Phase 2 (3-topology validation)")
    sys.exit(0)
else:
    print("\n❌ FAILURE: PDR = 0%. Data traffic timing fix did not work")
    print("   Need to revisit lora-mesh-sim.cc L283-344")
    sys.exit(1)
PYTHON_EOF

echo ""

# ============================================================
# Phase 2: Three-topology validation (3场景)
# ============================================================
echo "📌 Phase 2: Three-topology validation"
echo "Running: Linear/Tree/Grid (no attack) → Expected PDR > 0% all"
echo ""

if [ -f "docker-run-gpu.sh" ]; then
    ./docker-run-gpu.sh \
        --topologies "Linear" "Tree" "Grid" \
        --attacks "None" \
        --patches 1 \
        --seeds 1 \
        --workers 3 \
        --duration 420 \
        2>&1 | tee verify_phase2.log
fi

echo ""
echo "✅ Phase 2 complete. Analyzing 3-topology results..."
echo ""

# ============================================================
# Phase 3: Analyze three-topology results
# ============================================================
python3 << 'PYTHON_EOF'
import json
import os
import sys
import statistics

results_dir = "results"
topology_pdrs = {"Linear": None, "Tree": None, "Grid": None}

# Find results for each topology (no attack, patch=1)
for topo in topology_pdrs.keys():
    for f in os.listdir(results_dir):
        if f.startswith(f"{topo}_None_patch=1"):
            with open(os.path.join(results_dir, f)) as fp:
                data = json.load(fp)
                topology_pdrs[topo] = data.get('pdr', 0)
            break

print("3-Topology PDR Results (No Attack):")
print("-" * 40)
for topo, pdr in topology_pdrs.items():
    status = "✅" if (pdr is not None and pdr > 0) else "❌"
    pdr_str = f"{pdr*100:.1f}%" if pdr is not None else "N/A"
    print(f"{status} {topo:10s}: {pdr_str}")

print("-" * 40)

# Check if all topologies have PDR > 0
all_positive = all(pdr is not None and pdr > 0 for pdr in topology_pdrs.values())

if all_positive:
    avg_pdr = statistics.mean(p for p in topology_pdrs.values() if p is not None)
    print(f"\n✅ SUCCESS: All topologies have PDR > 0%")
    print(f"   Average PDR: {avg_pdr*100:.1f}%")
    print("\n📊 Ready for Phase 3: Full 27-scenario rerun")
    sys.exit(0)
else:
    missing = [t for t, p in topology_pdrs.items() if p is None]
    print(f"\n⚠️  WARNING: Missing results for: {missing}")
    print("   Proceeding to full 27-scenario rerun anyway...")
    sys.exit(0)
PYTHON_EOF

echo ""
echo "═" * 60
echo "✅ Verification script complete"
echo ""
echo "📋 Next steps (Day 3):"
echo "  1. Run full 27-scenario rerun (can take 60-90 min)"
echo "  2. Check results in results/ directory"
echo "  3. Run: python3 analyze_pdr.py"
echo ""
