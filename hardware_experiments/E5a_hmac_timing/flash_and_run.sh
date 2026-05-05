#!/bin/bash
# E5a Flash & Run — Heltec WiFi LoRa 32 V3 on augustmacbook-pro
# Usage: bash flash_and_run.sh [--port /dev/cu.usbmodem134201]

set -e

PORT="${1:-/dev/cu.usbmodem134201}"
SKETCH_DIR="$(dirname "$0")/hmac_benchmark"
BOARD="Heltec-esp32:esp32:heltec_wifi_lora_32_V3"
BAUD=921600
MONITOR_BAUD=115200

echo "=== E5a: HMAC Benchmark Flash & Run ==="
echo "Port : $PORT"
echo "Board: $BOARD"
echo ""

# --- Step 1: Install arduino-cli if missing ---
if ! command -v arduino-cli &>/dev/null; then
  echo "[1/5] Installing arduino-cli..."
  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
  export PATH="$HOME/bin:$PATH"
else
  echo "[1/5] arduino-cli found: $(arduino-cli version)"
fi

# --- Step 2: Install Heltec ESP32 board package ---
echo "[2/5] Checking Heltec board package..."
arduino-cli config init --overwrite 2>/dev/null || true
arduino-cli config add board_manager.additional_urls \
  https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series/releases/download/0.0.9/package_heltec_esp32_index.json \
  2>/dev/null || true

arduino-cli core update-index
arduino-cli core install "Heltec-esp32:esp32" 2>/dev/null || true

# Fallback: use official esp32 core if Heltec not available
if ! arduino-cli board listall | grep -q "heltec_wifi_lora_32_V3" 2>/dev/null; then
  echo "  Heltec core not found, using official esp32 core..."
  arduino-cli config add board_manager.additional_urls \
    https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json \
    2>/dev/null || true
  arduino-cli core update-index
  arduino-cli core install "esp32:esp32"
  BOARD="esp32:esp32:heltec_wifi_lora_32_V3"
fi

# --- Step 3: Compile ---
echo "[3/5] Compiling sketch..."
arduino-cli compile \
  --fqbn "$BOARD" \
  --build-property "build.extra_flags=-DARDUINO_USB_CDC_ON_BOOT=1" \
  "$SKETCH_DIR"

# --- Step 4: Flash ---
echo "[4/5] Flashing to $PORT @ $BAUD baud..."
arduino-cli upload \
  --fqbn "$BOARD" \
  --port "$PORT" \
  --input-dir "$SKETCH_DIR/build" \
  "$SKETCH_DIR"

echo ""
echo "[5/5] Monitoring serial output (Ctrl+C to stop)..."
echo "      Results will appear after ~30 seconds"
echo ""
# Capture output and parse
arduino-cli monitor \
  --port "$PORT" \
  --config "baudrate=$MONITOR_BAUD" \
  2>/dev/null | tee /tmp/e5a_raw.log | grep -E "mean|std|p50|p95|JSON|Result|===|Chip|CPU|Done"
