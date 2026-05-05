#!/usr/bin/env python3
"""
E5a result parser — reads serial output and extracts JSON for paper update.

Usage:
    # Live capture (replace port as needed):
    python3 parse_e5a.py --port /dev/cu.usbmodem134201

    # From saved log file:
    python3 parse_e5a.py --file e5a_result.log

Output:
    - Console summary
    - e5a_result.json  (for records)
    - Paper update instructions
"""

import argparse, json, re, sys
from datetime import datetime

PAPER_CLAIM_MS = 0.3   # mbedTLS 2.28 benchmark claim in paper §6.1
LORA_AIRTIME_US = 165000


def parse_json_from_log(text: str) -> dict:
    m = re.search(r'JSON_BEGIN\s*(\{.*?\})\s*JSON_END', text, re.DOTALL)
    if not m:
        raise ValueError("JSON_BEGIN/JSON_END block not found in log")
    return json.loads(m.group(1))


def capture_from_serial(port: str, baud: int = 115200) -> str:
    import serial, time
    print(f"Opening {port} @ {baud} baud — press reset on board if needed...")
    lines = []
    with serial.Serial(port, baud, timeout=2) as s:
        done = False
        while not done:
            line = s.readline().decode("utf-8", errors="replace").rstrip()
            if line:
                print(line)
                lines.append(line)
            if "JSON_END" in line:
                done = True
            if "Done. Reset" in line:
                done = True
    return "\n".join(lines)


def report(d: dict):
    print("\n" + "=" * 60)
    print("E5a RESULT SUMMARY")
    print("=" * 60)
    print(f"  Chip       : {d['chip']} rev{d['chip_rev']} @ {d['cpu_mhz']} MHz")
    print(f"  SDK        : {d['sdk']}")
    print(f"  Iterations : {d['n_iter']:,}")
    print(f"  Payload    : {d['payload_bytes']} B  Tag: {d['tag_bytes']} B  Key: {d['key_bytes']} B")
    print()
    print(f"  mean  : {d['mean_us']:.1f} µs  ({d['mean_ms']:.3f} ms)")
    print(f"  std   : {d['std_us']:.1f} µs")
    print(f"  p50   : {d['p50_us']:.1f} µs")
    print(f"  p95   : {d['p95_us']:.1f} µs")
    print(f"  p99   : {d['p99_us']:.1f} µs")
    print(f"  max   : {d['max_us']:.1f} µs")
    print()
    print(f"  Paper claim (§6.1) : {PAPER_CLAIM_MS * 1000:.0f} µs ({PAPER_CLAIM_MS} ms)")

    measured_ms = d["mean_ms"]
    delta_pct = (measured_ms - PAPER_CLAIM_MS) / PAPER_CLAIM_MS * 100
    verdict = "CONFIRMED" if abs(delta_pct) < 30 else "DIFFERS"
    print(f"  Measured           : {d['mean_us']:.1f} µs ({measured_ms:.3f} ms)  [{delta_pct:+.1f}%] → {verdict}")
    print(f"  LoRa air-time ref  : {LORA_AIRTIME_US / 1000:.0f} ms")
    print(f"  Overhead           : {d['overhead_pct']:.3f}% of LoRa air-time")

    print()
    print("=" * 60)
    print("PAPER UPDATE (IOTJ-Aug.tex §6.1 line ~705)")
    print("=" * 60)
    chip = d["chip"]
    cpu  = d["cpu_mhz"]
    n    = d["n_iter"]
    mn   = d["mean_us"]
    sd   = d["std_us"]
    print(f"""
OLD: "0.3~ms (mbedTLS~2.28 on ESP32-S3 @ 240~MHz), measured over 10,000 iterations"

NEW: "{mn/1000:.2f}~ms ({mn:.0f}~\\mu s $\\pm$ {sd:.0f}~\\mu s, {n:,}~iterations, {chip} @ {cpu}~MHz)"

Also update avoid_overclaiming in 1_contribution.yaml C3:
  FROM: "Do not claim hardware measurement — ESP32 timing is from literature + mbedTLS benchmarks"
  TO:   "Timing measured on real {chip} @ {cpu} MHz hardware, {n:,} iterations"

R2 status: in_progress → CLOSED
C3 confidence: 0.92 → 0.98
""")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", help="Serial port (live capture)")
    ap.add_argument("--file", help="Saved log file")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    if args.file:
        text = open(args.file).read()
    elif args.port:
        text = capture_from_serial(args.port, args.baud)
    else:
        print("Reading stdin...")
        text = sys.stdin.read()

    d = parse_json_from_log(text)
    d["captured_at"] = datetime.now().isoformat()
    report(d)

    out = "e5a_result.json"
    with open(out, "w") as f:
        json.dump(d, f, indent=2)
    print(f"\nResult saved to {out}")


if __name__ == "__main__":
    main()
