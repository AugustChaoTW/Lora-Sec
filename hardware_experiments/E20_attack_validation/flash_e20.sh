#!/bin/bash
# E20 Flash Script — compile + upload 3 nodes
# Usage: ./flash_e20.sh <sender_port> <relay_port> <receiver_port>
# Example: ./flash_e20.sh /dev/cu.usbmodem1134201 /dev/cu.usbmodem1134401 /dev/cu.usbmodem1134601
#
# RELAY_MODE is set at compile time by editing node_relay.ino:
#   #define RELAY_MODE MODE_HONEST   (baseline)
#   #define RELAY_MODE MODE_DROP     + DROP_RATE_PCT=10 or 20
#   #define RELAY_MODE MODE_BITFLIP
#   #define RELAY_MODE MODE_REPLAY

set -e

ARDUINO="$HOME/bin/arduino-cli"
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc"
BASE="$HOME/Work/Lora-Sec/hardware_experiments/E20_attack_validation"

SENDER_PORT="${1:-/dev/cu.usbmodem1134201}"
RELAY_PORT="${2:-/dev/cu.usbmodem1134401}"
RECEIVER_PORT="${3:-/dev/cu.usbmodem1134601}"

echo "=== E20 Flash ==="
echo "FQBN:     $FQBN"
echo "Sender:   $SENDER_PORT"
echo "Relay:    $RELAY_PORT"
echo "Receiver: $RECEIVER_PORT"
echo ""

compile_and_flash() {
  local sketch_dir="$1"
  local port="$2"
  local name="$3"
  echo "--- Compiling $name ---"
  "$ARDUINO" compile --fqbn "$FQBN" "$sketch_dir" 2>&1
  echo "--- Uploading $name → $port ---"
  "$ARDUINO" upload --fqbn "$FQBN" --port "$port" "$sketch_dir" 2>&1
  echo "--- $name DONE ---"
  echo ""
}

compile_and_flash "$BASE/node_sender"   "$SENDER_PORT"   "node_sender"
compile_and_flash "$BASE/node_relay"    "$RELAY_PORT"    "node_relay"
compile_and_flash "$BASE/node_receiver" "$RECEIVER_PORT" "node_receiver"

echo "=== All 3 nodes flashed ==="
echo "Sender:   $SENDER_PORT (magic 0xE1,0xAA)"
echo "Relay:    $RELAY_PORT  (translates 0xAA→0x9A)"
echo "Receiver: $RECEIVER_PORT (accepts 0xE1,0x9A)"
echo ""
echo "Next: ./run_e20.sh $SENDER_PORT $RELAY_PORT $RECEIVER_PORT <scenario>"
