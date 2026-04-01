#!/bin/bash

# LoRa Mesh Formal Verification Script
# Purpose: Run all Tamarin models and generate verification report
# Usage: ./verify.sh [model_name]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="$(dirname "$SCRIPT_DIR")"
MODELS_DIR="$WORK_DIR/models"
VERIFICATION_DIR="$WORK_DIR/verification"

echo "=================================================="
echo "LoRa Mesh Control Plane - Tamarin Verification"
echo "=================================================="
echo ""

# Check if Tamarin is installed
if ! command -v tamarin-prover &> /dev/null; then
    echo "ERROR: tamarin-prover not found."
    echo "Please install Tamarin Prover first:"
    echo "  https://tamarin-prover.github.io/manual/installation.html"
    exit 1
fi

echo "[INFO] Tamarin version: $(tamarin-prover --version)"
echo ""

# Determine which model(s) to verify
if [ -z "$1" ]; then
    MODELS=("baseline" "patched_2node" "patched_metrics_v2")
    echo "[INFO] Running all models (baseline, patched_2node, patched_metrics_v2)"
else
    MODELS=("$1")
    echo "[INFO] Running model: $1"
fi

echo ""

# Run verification for each model
FAILED=0
PASSED=0

for model in "${MODELS[@]}"; do
    echo "=================================================="
    echo "Verifying: $model"
    echo "=================================================="
    
    # Map model name to file
    case $model in
        "baseline")
            MODEL_FILE="$MODELS_DIR/baseline_lora_dv.spthy"
            MODEL_LABEL="Baseline LoRaMesher DV (No Auth)"
            ;;
        "patched_2node")
            MODEL_FILE="$MODELS_DIR/patched_lora_dv_2node.spthy"
            MODEL_LABEL="Patched with HMAC (2-Node Simplified)"
            ;;
        "patched_metrics_v2")
            MODEL_FILE="$MODELS_DIR/patched_lora_dv_with_metrics_v2.spthy"
            MODEL_LABEL="Patched with MetricVersion (v2)"
            ;;
        *)
            echo "[ERROR] Unknown model: $model"
            exit 1
            ;;
    esac
    
    if [ ! -f "$MODEL_FILE" ]; then
        echo "[ERROR] Model file not found: $MODEL_FILE"
        FAILED=$((FAILED + 1))
        continue
    fi
    
    echo "[INFO] Model: $MODEL_LABEL"
    echo "[INFO] File: $MODEL_FILE"
    echo ""
    
    # Run verification
    START_TIME=$(date +%s)
    OUTPUT_FILE="$VERIFICATION_DIR/${model}_verification_run.log"
    
    echo "[RUNNING] tamarin-prover $MODEL_FILE --prove"
    echo "[OUTPUT] $OUTPUT_FILE"
    
    if tamarin-prover "$MODEL_FILE" --prove 2>&1 | tee "$OUTPUT_FILE"; then
        END_TIME=$(date +%s)
        ELAPSED=$((END_TIME - START_TIME))
        
        # Parse results
        VERIFIED=$(grep -c "verified\|PROVED" "$OUTPUT_FILE" || true)
        FALSIFIED=$(grep -c "falsified\|BROKEN" "$OUTPUT_FILE" || true)
        
        echo ""
        echo "[RESULT] ✓ Verification completed in ${ELAPSED}s"
        echo "  - Properties verified: $VERIFIED"
        echo "  - Properties falsified: $FALSIFIED"
        echo ""
        
        PASSED=$((PASSED + 1))
    else
        echo ""
        echo "[ERROR] Verification failed for $model"
        echo ""
        FAILED=$((FAILED + 1))
    fi
done

echo "=================================================="
echo "Summary"
echo "=================================================="
echo "Models passed: $PASSED"
echo "Models failed: $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "✓ All verifications completed successfully!"
    echo ""
    echo "Next steps:"
    echo "1. Review full logs in: $VERIFICATION_DIR/"
    echo "2. Compare results: diff $VERIFICATION_DIR/baseline_verification_run.log $VERIFICATION_DIR/patched_2node_verification_run.log"
    echo "3. See README.md for interpretation"
    exit 0
else
    echo "✗ Some verifications failed. Check logs for details."
    exit 1
fi
