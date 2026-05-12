#!/bin/bash
# E7 — Collect Serial logs from all nodes simultaneously
#
# Usage:
#   bash collect_logs.sh /dev/ttyACM0 /dev/ttyACM1 /dev/ttyACM2 /dev/ttyACM3 /dev/ttyACM4 /dev/ttyACM5
#
# Ports order: node_a  node_r1  node_r2  node_r3  node_b  node_c_blackhole
#
# Logs saved to: results/e7_<node>_<timestamp>.log
# Combined log:  results/e7_combined_<timestamp>.log
#
# Duration: 120 seconds (covers ~12 Hello cycles, ~24 Data packets from node_a)
# Stop early: Ctrl+C (logs are flushed continuously via tee)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RESULTS_DIR="${SCRIPT_DIR}/results"
BAUD=115200
DURATION=${DURATION:-120}   # seconds; override with: DURATION=60 bash collect_logs.sh ...

mkdir -p "${RESULTS_DIR}"
TS=$(date +%Y%m%d_%H%M%S)

PORTS=("$@")
NODES=(node_a node_r1 node_r2 node_r3 node_b node_c_blackhole)
PREFIXES=("[A(0xAA)]" "[R1(0x11)]" "[R2(0x22)]" "[R3(0x33)]" "[B(0xBB)]" "[C(0xCC)]")

if [[ ${#PORTS[@]} -ne 6 ]]; then
  echo "Usage: bash collect_logs.sh <port_a> <port_r1> <port_r2> <port_r3> <port_b> <port_c>"
  echo "Example: bash collect_logs.sh /dev/ttyACM0 /dev/ttyACM1 /dev/ttyACM2 /dev/ttyACM3 /dev/ttyACM4 /dev/ttyACM5"
  exit 1
fi

COMBINED="${RESULTS_DIR}/e7_combined_${TS}.log"
PIDS=()

echo "=== E7 Black Hole Demo — Log Collection ==="
echo "Duration: ${DURATION}s"
echo "Combined: ${COMBINED}"
echo ""

for i in "${!PORTS[@]}"; do
  port="${PORTS[$i]}"
  node="${NODES[$i]}"
  prefix="${PREFIXES[$i]}"
  logfile="${RESULTS_DIR}/e7_${node}_${TS}.log"

  echo "  ${node} @ ${port} → ${logfile}"

  # Read serial, prefix each line with node ID, tee to individual + combined log
  (
    stty -F "${port}" "${BAUD}" raw -echo 2>/dev/null || true
    while IFS= read -r line; do
      ts=$(date +%H:%M:%S.%3N)
      echo "${prefix} ${ts} ${line}"
    done < "${port}"
  ) | tee -a "${logfile}" >> "${COMBINED}" &
  PIDS+=($!)
done

echo ""
echo "Collecting for ${DURATION}s... (Ctrl+C to stop early)"
sleep "${DURATION}"

# Kill all background readers
for pid in "${PIDS[@]}"; do
  kill "${pid}" 2>/dev/null || true
done
wait 2>/dev/null || true

echo ""
echo "=== Collection complete ==="
echo ""

# Quick summary
echo "--- Summary ---"
BLACKHOLE_COUNT=$(grep -c "BLACKHOLE.*DROP" "${COMBINED}" 2>/dev/null || echo 0)
HMAC_FAIL_COUNT=$(grep -c "HMAC FAILED" "${COMBINED}" 2>/dev/null || echo 0)
POISONED_COUNT=$(grep -c "POISONED\|0xCC" "${COMBINED}" 2>/dev/null || echo 0)
GW_RX_TOTAL=$(grep "RX.*DATA.*total=" "${COMBINED}" 2>/dev/null | tail -1 | grep -oP 'total=\K[0-9]+' || echo 0)
NODE_A_SENT=$(grep "\[A(0xAA)\].*\[TX\] DATA" "${COMBINED}" 2>/dev/null | wc -l || echo 0)

echo "  Gateway packets received : ${GW_RX_TOTAL}"
echo "  Node A packets sent      : ${NODE_A_SENT}"
echo "  Black hole drops (0xCC)  : ${BLACKHOLE_COUNT}"
echo "  HMAC FAILED drops        : ${HMAC_FAIL_COUNT}"
echo "  Route poisoning events   : ${POISONED_COUNT}"
echo ""

if [[ "${BLACKHOLE_COUNT}" -gt 0 && "${HMAC_FAIL_COUNT}" -eq 0 ]]; then
  echo "RESULT: Black Hole SUCCEEDED — HMAC patch bypassed by insider."
  echo "        Authentication ≠ Availability confirmed."
elif [[ "${HMAC_FAIL_COUNT}" -gt 0 && "${BLACKHOLE_COUNT}" -eq 0 ]]; then
  echo "RESULT: HMAC patch blocked attack (wrong firmware? attacker should use sign_hello)."
else
  echo "RESULT: Mixed — check combined log for details."
fi

echo ""
echo "Run parse_e7.py for full PDR analysis:"
echo "  python3 parse_e7.py ${COMBINED}"
