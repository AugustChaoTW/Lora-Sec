#!/bin/bash
# Tamarin Lemma Integration Helper
# Purpose: Automate manual addition of executability + sanity lemmas
# Usage: ./TAMARIN_LEMMA_INTEGRATION.sh [baseline|patched|both]

set -e

PROJECT_ROOT="/home/augchao/Lora-Sec"
TAMARIN_DIR="$PROJECT_ROOT/phase2_tamarin_models"

BASELINE_FILE="$TAMARIN_DIR/baseline/baseline_lora_dv.spthy"
PATCHED_FILE="$TAMARIN_DIR/patched/patched_lora_dv.spthy"

echo "=================================================="
echo "Tamarin Lemma Integration Helper"
echo "=================================================="

if [ ! -d "$TAMARIN_DIR" ]; then
    echo "❌ Tamarin directory not found: $TAMARIN_DIR"
    exit 1
fi

# Baseline Executability Lemmas
BASELINE_LEMMAS='

// Week 2 Executability Lemmas (auto-added by script)
lemma L_Ex1_Node_Can_Init:
  "∃ n #i. NodeInit(n) @ i"

lemma L_Ex2_Hello_Can_Broadcast:
  "∃ n seq #i. HelloSent(n, '"'"'0'"'"', seq) @ i"

lemma L_Ex3_Route_Can_Update:
  "∃ dst src metric seq nh #i. RouteUpdate(dst, src, metric, seq) @ i"

lemma L_Ex4_Data_Can_Forward:
  "∃ f s d nh #i. Forward(f, s, d, nh) @ i"'

# Patched Sanity Lemmas
PATCHED_LEMMAS='

// Week 2 Sanity Lemmas (auto-added by script)
lemma L_Sanity1_Honest_Route_Exists:
  "∃ dst src metric nh seq ver #i. 
    RouteAccepted(dst, src, metric, nh, seq, ver) @ i & 
    HonestNode(dst) @ i & HonestNode(src) @ i"

lemma L_Sanity2_HMAC_Verified:
  "∃ dst src metric seq ver #i. 
    HMACVerified(dst, src, metric, seq) @ i"

lemma L_Sanity3_MetricVersion_Increment_Happens:
  "∃ n ver #i. MetricVersionIncrement(n, ver) @ i"'

# Function to add lemmas to file
add_baseline_lemmas() {
    if grep -q "L_Ex1_Node_Can_Init" "$BASELINE_FILE"; then
        echo "⚠️  Baseline lemmas already present, skipping..."
        return
    fi
    
    echo "Adding 4 executability lemmas to baseline..."
    
    # Append before the final closing (remove last line, add lemmas, add closing back)
    head -n -1 "$BASELINE_FILE" > "$BASELINE_FILE.tmp"
    echo "$BASELINE_LEMMAS" >> "$BASELINE_FILE.tmp"
    echo "end" >> "$BASELINE_FILE.tmp"
    mv "$BASELINE_FILE.tmp" "$BASELINE_FILE"
    
    echo "✅ Baseline lemmas added"
}

add_patched_lemmas() {
    if grep -q "L_Sanity1_Honest_Route_Exists" "$PATCHED_FILE"; then
        echo "⚠️  Patched lemmas already present, skipping..."
        return
    fi
    
    echo "Adding 3 sanity lemmas to patched..."
    
    head -n -1 "$PATCHED_FILE" > "$PATCHED_FILE.tmp"
    echo "$PATCHED_LEMMAS" >> "$PATCHED_FILE.tmp"
    echo "end" >> "$PATCHED_FILE.tmp"
    mv "$PATCHED_FILE.tmp" "$PATCHED_FILE"
    
    echo "✅ Patched lemmas added"
}

verify_baseline() {
    echo ""
    echo "Verifying baseline model..."
    cd "$TAMARIN_DIR/baseline"
    
    if ! timeout 300 tamarin-prover baseline_lora_dv.spthy --prove > logs/verify_exec.log 2>&1; then
        echo "⚠️  Verification timed out or failed (check logs/verify_exec.log)"
        return 1
    fi
    
    if grep -q "VERIFIED" logs/verify_exec.log; then
        echo "✅ Baseline verification complete"
        grep "VERIFIED\|FALSIFIED" logs/verify_exec.log | head -10
    else
        echo "⚠️  No verification results found in log"
    fi
}

verify_patched() {
    echo ""
    echo "Verifying patched model..."
    cd "$TAMARIN_DIR/patched"
    
    if ! timeout 300 tamarin-prover patched_lora_dv.spthy --prove > logs/verify_sanity.log 2>&1; then
        echo "⚠️  Verification timed out or failed (check logs/verify_sanity.log)"
        return 1
    fi
    
    if grep -q "VERIFIED" logs/verify_sanity.log; then
        echo "✅ Patched verification complete"
        grep "VERIFIED\|FALSIFIED" logs/verify_sanity.log | head -10
    else
        echo "⚠️  No verification results found in log"
    fi
}

create_verification_report() {
    echo ""
    echo "Creating verification report..."
    
    cat > "$TAMARIN_DIR/WEEK_2_TAMARIN_VERIFICATION.md" << 'EOF'
# Week 2 Tamarin Verification Report

**Date**: 2026-03-29 (auto-generated)  
**Status**: In Progress

## Baseline Model: Executability Lemmas

| Lemma | Status | Time | Notes |
|-------|--------|------|-------|
| L_Ex1_Node_Can_Init | ⏳ Running | — | Node initialization possible |
| L_Ex2_Hello_Can_Broadcast | ⏳ Running | — | Hello message broadcast works |
| L_Ex3_Route_Can_Update | ⏳ Running | — | Route updates can occur |
| L_Ex4_Data_Can_Forward | ⏳ Running | — | Data packet forwarding possible |

**Overall**: Verification in progress...

## Patched Model: Sanity Lemmas

| Lemma | Status | Time | Notes |
|-------|--------|------|-------|
| L_Sanity1_Honest_Route_Exists | ⏳ Running | — | Honest routes can be established |
| L_Sanity2_HMAC_Verified | ⏳ Running | — | HMAC verification succeeds |
| L_Sanity3_MetricVersion_Increment_Happens | ⏳ Running | — | Version increment logic works |

**Overall**: Verification in progress...

---

**Note**: This is an auto-generated skeleton. Results will be populated after verification completes.

See logs/ directory for detailed Tamarin output:
- `baseline/logs/verify_exec.log` — Executability verification
- `patched/logs/verify_sanity.log` — Sanity verification
EOF
    
    echo "✅ Report created: WEEK_2_TAMARIN_VERIFICATION.md"
}

# Main dispatch
case "${1:-both}" in
    baseline)
        add_baseline_lemmas
        verify_baseline
        ;;
    patched)
        add_patched_lemmas
        verify_patched
        ;;
    both)
        add_baseline_lemmas
        add_patched_lemmas
        create_verification_report
        
        echo ""
        echo "=================================================="
        echo "Lemmas added. Ready for verification."
        echo "=================================================="
        echo ""
        echo "To verify manually:"
        echo "  cd $TAMARIN_DIR/baseline"
        echo "  tamarin-prover baseline_lora_dv.spthy --prove"
        echo ""
        echo "  cd $TAMARIN_DIR/patched"
        echo "  tamarin-prover patched_lora_dv.spthy --prove"
        echo ""
        ;;
    *)
        echo "Usage: $0 [baseline|patched|both]"
        exit 1
        ;;
esac

echo ""
echo "✅ Done"
