#!/bin/bash
# E7 Black Hole Demo — Flash all 6 nodes (EoRa PI / ESP32-S3)
#
# Usage:
#   bash flash_all.sh                          # interactive: prompts for each port
#   bash flash_all.sh node_a /dev/ttyACM0      # flash one specific node
#   bash flash_all.sh --all                    # flash all, auto-detect ports
#
# Board: esp32:esp32:esp32s3  CDCOnBoot=cdc  (Ebyte EoRa PI)
#
# Node mapping:
#   node_a           — Sender    (0xAA)
#   node_r1          — Relay R1  (0x11)
#   node_r2          — Relay R2  (0x22)
#   node_r3          — Relay R3  (0x33)
#   node_b           — Gateway   (0xBB)
#   node_c_blackhole — Attacker  (0xCC)  ← flash last

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BOARD="esp32:esp32:esp32s3"
BOARD_OPTS="CDCOnBoot=cdc"
FQBN="${BOARD}:${BOARD_OPTS}"
BAUD=921600
MONITOR_BAUD=115200

NODES=(node_a node_r1 node_r2 node_r3 node_b node_c_blackhole)
NODE_IDS=(0xAA 0x11 0x22 0x33 0xBB 0xCC)
NODE_NAMES=("Sender" "Relay R1" "Relay R2" "Relay R3" "Gateway" "ATTACKER")

RED='\033[0;31m'; GRN='\033[0;32m'; YLW='\033[1;33m'; NC='\033[0m'

# Ensure arduino-cli and esp32 core available
check_env() {
  if ! command -v arduino-cli &>/dev/null; then
    echo "arduino-cli not found. Install: curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
    exit 1
  fi
  if ! arduino-cli board listall 2>/dev/null | grep -q "esp32s3"; then
    echo "esp32 core not installed. Run:"
    echo "  arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"
    echo "  arduino-cli core update-index && arduino-cli core install esp32:esp32"
    exit 1
  fi
}

compile_node() {
  local node=$1
  local sketch_dir="${SCRIPT_DIR}/${node}"
  echo -e "${YLW}[COMPILE]${NC} ${node}..."
  arduino-cli compile \
    --fqbn "${FQBN}" \
    --build-property "build.extra_flags=-DARDUINO_USB_CDC_ON_BOOT=1" \
    "${sketch_dir}" 2>&1 | tail -5
  echo -e "${GRN}[OK]${NC} ${node} compiled."
}

flash_node() {
  local node=$1
  local port=$2
  local sketch_dir="${SCRIPT_DIR}/${node}"
  echo -e "${YLW}[FLASH]${NC} ${node} → ${port}..."
  # EoRa PI needs manual boot mode: hold BOOT, press RST, release BOOT
  # esptool handles this automatically if CDC is available
  arduino-cli upload \
    --fqbn "${FQBN}" \
    --port "${port}" \
    "${sketch_dir}" 2>&1 | tail -5
  echo -e "${GRN}[OK]${NC} ${node} flashed."
}

monitor_node() {
  local port=$1
  local logfile=$2
  echo -e "${YLW}[MONITOR]${NC} ${port} → ${logfile} (Ctrl+C to stop)"
  mkdir -p "${SCRIPT_DIR}/results"
  arduino-cli monitor \
    --port "${port}" \
    --config "baudrate=${MONITOR_BAUD}" \
    2>/dev/null | tee "${logfile}"
}

# ---- Single node mode ----
if [[ $# -eq 2 && $1 != "--all" ]]; then
  NODE=$1; PORT=$2
  check_env
  compile_node "${NODE}"
  flash_node "${NODE}" "${PORT}"
  echo ""
  echo "Flash complete. To monitor:"
  echo "  arduino-cli monitor --port ${PORT} --config baudrate=${MONITOR_BAUD}"
  echo "Or use collect_logs.sh to capture all nodes simultaneously."
  exit 0
fi

# ---- Interactive / full mode ----
check_env

echo "================================"
echo " E7 Black Hole Demo — Flash All"
echo "================================"
echo ""
echo "Topology: A(0xAA) → R1 → R2 → R3 → B(0xBB)"
echo "Attacker: C(0xCC) — insider, holds NETWORK_PSK"
echo ""
echo -e "${RED}WARNING:${NC} Flash node_c_blackhole LAST."
echo "         Once attacker is live, all honest nodes will route to 0xCC."
echo ""

# Step 1: compile all
echo "--- Compiling all nodes ---"
for node in "${NODES[@]}"; do
  compile_node "${node}"
done
echo ""

# Step 2: flash each node interactively
echo "--- Flashing nodes ---"
echo "Connect each board via USB when prompted."
echo ""

for i in "${!NODES[@]}"; do
  node="${NODES[$i]}"
  nid="${NODE_IDS[$i]}"
  nname="${NODE_NAMES[$i]}"

  if [[ "${node}" == "node_c_blackhole" ]]; then
    echo -e "${RED}>>> ATTACKER NODE <<<${NC}"
    echo "Flash this LAST. The attacker will immediately broadcast signed Hellos."
  fi

  echo -n "Connect ${node} (${nname} ${nid}) then press Enter (or type port, e.g. /dev/ttyACM0): "
  read -r input
  if [[ -z "${input}" ]]; then
    # Auto-detect: find newly connected ESP32 port
    PORT=$(ls /dev/ttyACM* /dev/ttyUSB* /dev/cu.usbmodem* 2>/dev/null | head -1)
    if [[ -z "${PORT}" ]]; then
      echo "No port found. Specify manually: bash flash_all.sh ${node} /dev/ttyACMx"
      exit 1
    fi
    echo "Auto-detected: ${PORT}"
  else
    PORT="${input}"
  fi

  flash_node "${node}" "${PORT}"
  echo ""
done

echo "================================"
echo " All nodes flashed successfully"
echo "================================"
echo ""
echo "Next: run collect_logs.sh to capture Serial output from all nodes."
echo "Expected results:"
echo "  - node_a Serial: [ROUTE] NH=0xCC POISONED! then [TX] DATA via=0xCC with no ACKs"
echo "  - node_b Serial: silence after Hello — no [RX] DATA lines"
echo "  - node_c Serial: [BLACKHOLE] DROP data src=0xAA (total_dropped=N)"
echo ""
echo "Finding: HMAC patch (SECURITY_MODE=1) does NOT prevent insider Black Hole."
echo "         Authentication ≠ Availability."
