#!/bin/bash
# E20 Run Script — capture serial logs from 3 nodes simultaneously
# Usage: ./run_e20.sh <sender_port> <relay_port> <receiver_port> <scenario> [duration_sec]
# Example: ./run_e20.sh /dev/cu.usbmodem1134201 /dev/cu.usbmodem1134401 /dev/cu.usbmodem1134601 drop10 120
#
# Scenarios: honest | drop10 | drop20 | bitflip | replay
# Duration: default 120s

set -e

SENDER_PORT="${1:-/dev/cu.usbmodem1134201}"
RELAY_PORT="${2:-/dev/cu.usbmodem1134401}"
RECEIVER_PORT="${3:-/dev/cu.usbmodem1134601}"
SCENARIO="${4:-honest}"
DURATION="${5:-120}"

BASE="$HOME/Work/Lora-Sec/hardware_experiments/E20_attack_validation"
RESULTS="$BASE/results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
LOG_DIR="$RESULTS/${TIMESTAMP}_${SCENARIO}"

mkdir -p "$LOG_DIR"

echo "=== E20 Run: $SCENARIO ($DURATION s) ==="
echo "Logs → $LOG_DIR"
echo "Press Ctrl+C to stop early"
echo ""

# Read serial from all 3 ports simultaneously
python3 - <<PYEOF &
import serial, threading, time, sys, os

ports = {
    "SENDER":   ("$SENDER_PORT",   "$LOG_DIR/sender.log"),
    "RELAY":    ("$RELAY_PORT",    "$LOG_DIR/relay.log"),
    "RECEIVER": ("$RECEIVER_PORT", "$LOG_DIR/receiver.log"),
}

stop_event = threading.Event()

def read_port(name, port, logfile):
    try:
        with serial.Serial(port, 115200, timeout=1) as s, open(logfile, 'w') as f:
            print(f"[{name}] opened {port}")
            while not stop_event.is_set():
                line = s.readline().decode('utf-8', errors='replace').rstrip()
                if line:
                    ts = time.strftime('%H:%M:%S')
                    out = f"[{ts}][{name}] {line}"
                    print(out, flush=True)
                    f.write(out + '\n')
                    f.flush()
    except Exception as e:
        print(f"[{name}] ERROR: {e}", file=sys.stderr)

threads = []
for name, (port, logfile) in ports.items():
    t = threading.Thread(target=read_port, args=(name, port, logfile), daemon=True)
    t.start()
    threads.append(t)

time.sleep($DURATION)
stop_event.set()
print(f"\n[DONE] {$DURATION}s elapsed — stopping")
PYEOF

SERIAL_PID=$!
wait $SERIAL_PID 2>/dev/null || true

echo ""
echo "=== Parsing results ==="
python3 "$BASE/parse_e20.py" "$LOG_DIR" "$SCENARIO"
