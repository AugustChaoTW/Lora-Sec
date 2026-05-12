#!/usr/bin/env python3
"""
E15: Train LoRaMesher anomaly detection autoencoder offline.
Converts trained weights to C header for TFLite Micro on ESP32-S3.

Architecture:
  Encoder: 5 → 8 → 4 (ReLU)
  Decoder: 4 → 8 → 5 (Linear)
  ~130 parameters = 520 bytes float32

Feature vector (per Hello packet):
  [rssi_norm, hop_count_norm, mv_delta_norm, interval_norm, local_pdr]

Usage:
  # From captured serial log:
  python3 train_autoencoder.py --log results/baseline_serial.log

  # From NS-3 simulation data:
  python3 train_autoencoder.py --ns3 ../../phase4_ns3_simulation/results/

  # Generate synthetic baseline (for testing):
  python3 train_autoencoder.py --synthetic

Output:
  autoencoder_weights.h   — C header with float32 weight arrays
  threshold_report.txt    — reconstruction error statistics
"""

import argparse
import json
import math
import os
import re
import struct
import sys
from typing import List, Tuple

import numpy as np

try:
    import tensorflow as tf
    TF_AVAILABLE = True
except ImportError:
    TF_AVAILABLE = False
    print("[WARN] TensorFlow not available — using numpy-only training")


# --- Feature normalization bounds (from expected LoRaMesher operation) ---
FEAT_BOUNDS = {
    "rssi":      (-120.0, -30.0),    # dBm
    "hop_count": (1.0, 6.0),
    "mv_delta":  (0.0, 10.0),        # MetricVersion delta per window
    "interval":  (5.0, 30.0),        # seconds between Hellos
    "local_pdr": (0.0, 1.0),
}

def normalize(feat: List[float]) -> np.ndarray:
    bounds = list(FEAT_BOUNDS.values())
    out = []
    for i, (lo, hi) in enumerate(bounds):
        out.append((feat[i] - lo) / (hi - lo + 1e-9))
    return np.clip(out, 0, 1).astype(np.float32)


def parse_serial_log(log_path: str) -> List[List[float]]:
    """Parse serial log into feature vectors. Each Hello line contributes one sample."""
    samples = []
    node_state = {}  # node_id -> {last_hello_ms, last_mv, last_pdr}
    hello_re = re.compile(r"\[SNIFF\] Hello src=(0x\w+) metric=(\d+) seq=(\d+) rssi=([-\d.]+)")

    with open(log_path) as f:
        for line in f:
            m = hello_re.search(line)
            if not m:
                continue
            src, metric, seq, rssi_str = m.group(1), int(m.group(2)), int(m.group(3)), float(m.group(4))
            ts_m = re.search(r"(\d{2}:\d{2}:\d{2}\.\d+)", line)
            ts_ms = 0
            if ts_m:
                parts = ts_m.group(1).split(":")
                ts_ms = (int(parts[0])*3600 + int(parts[1])*60 + float(parts[2])) * 1000

            if src not in node_state:
                node_state[src] = {"last_ms": ts_ms, "last_seq": seq, "pdr": 1.0}
                continue

            st = node_state[src]
            interval_s = max((ts_ms - st["last_ms"]) / 1000, 0.1)
            mv_delta   = abs(seq - st["last_seq"])
            hop_count  = metric + 1

            feat = [rssi_str, float(hop_count), float(mv_delta), interval_s, st["pdr"]]
            samples.append(feat)

            st["last_ms"]  = ts_ms
            st["last_seq"] = seq

    print(f"[PARSE] {len(samples)} samples from {log_path}")
    return samples


def generate_synthetic(n_normal: int = 500, n_attack: int = 100) -> Tuple[np.ndarray, np.ndarray]:
    """Generate synthetic normal + attack feature vectors."""
    rng = np.random.default_rng(42)

    # Normal: RSSI ~-65±10, hop 2-4, mv_delta 1, interval 10±2s, pdr 0.85±0.1
    normal = np.column_stack([
        rng.normal(-65, 10,  n_normal),
        rng.uniform(2, 4,    n_normal),
        rng.uniform(0, 2,    n_normal),
        rng.normal(10, 2,    n_normal),
        rng.uniform(0.7, 1,  n_normal),
    ]).astype(np.float32)

    # Attack: flood (interval 0.5s), replay (mv_delta=0), metric spoofing (hop=0)
    attack = np.column_stack([
        rng.normal(-55, 5,   n_attack),
        rng.choice([0, 1],   n_attack),   # metric=0 (spoofed)
        np.zeros(n_attack),               # mv_delta=0 (replay)
        rng.uniform(0.5, 2,  n_attack),   # interval<2s (flood)
        rng.uniform(0, 0.3,  n_attack),
    ]).astype(np.float32)

    return normal, attack


def normalize_batch(X: np.ndarray) -> np.ndarray:
    bounds = list(FEAT_BOUNDS.values())
    out = np.zeros_like(X)
    for i, (lo, hi) in enumerate(bounds):
        out[:, i] = (X[:, i] - lo) / (hi - lo + 1e-9)
    return np.clip(out, 0, 1)


def train_numpy(X_train: np.ndarray, epochs: int = 200, lr: float = 0.01) -> dict:
    """Minimal autoencoder training in pure numpy (no TF dependency)."""
    rng = np.random.default_rng(0)

    def relu(x): return np.maximum(0, x)
    def relu_grad(x): return (x > 0).astype(np.float32)

    # Weights: 5→8→4→8→5
    W1 = rng.normal(0, 0.3, (5, 8)).astype(np.float32)
    b1 = np.zeros(8, dtype=np.float32)
    W2 = rng.normal(0, 0.3, (8, 4)).astype(np.float32)
    b2 = np.zeros(4, dtype=np.float32)
    W3 = rng.normal(0, 0.3, (4, 8)).astype(np.float32)
    b3 = np.zeros(8, dtype=np.float32)
    W4 = rng.normal(0, 0.3, (8, 5)).astype(np.float32)
    b4 = np.zeros(5, dtype=np.float32)

    n = len(X_train)
    for epoch in range(epochs):
        # Forward
        z1 = X_train @ W1 + b1;   a1 = relu(z1)
        z2 = a1 @ W2 + b2;        a2 = relu(z2)
        z3 = a2 @ W3 + b3;        a3 = relu(z3)
        z4 = a3 @ W4 + b4          # linear output
        loss = np.mean((z4 - X_train)**2)

        if epoch % 50 == 0:
            print(f"  epoch {epoch:4d}  MSE={loss:.6f}")

        # Backward
        d4 = 2 * (z4 - X_train) / n
        dW4 = a3.T @ d4;  db4 = d4.sum(0)
        d3 = (d4 @ W4.T) * relu_grad(z3)
        dW3 = a2.T @ d3;  db3 = d3.sum(0)
        d2 = (d3 @ W3.T) * relu_grad(z2)
        dW2 = a1.T @ d2;  db2 = d2.sum(0)
        d1 = (d2 @ W2.T) * relu_grad(z1)
        dW1 = X_train.T @ d1; db1 = d1.sum(0)

        # SGD update
        for W, dW in [(W1,dW1),(W2,dW2),(W3,dW3),(W4,dW4)]:
            W -= lr * dW
        for b, db in [(b1,db1),(b2,db2),(b3,db3),(b4,db4)]:
            b -= lr * db

    return {"W1":W1,"b1":b1,"W2":W2,"b2":b2,"W3":W3,"b3":b3,"W4":W4,"b4":b4}


def compute_threshold(weights: dict, X_normal: np.ndarray,
                      X_attack: np.ndarray) -> float:
    """Compute reconstruction error threshold from normal distribution."""
    def infer(X):
        def relu(x): return np.maximum(0, x)
        a1 = relu(X @ weights["W1"] + weights["b1"])
        a2 = relu(a1 @ weights["W2"] + weights["b2"])
        a3 = relu(a2 @ weights["W3"] + weights["b3"])
        return a3 @ weights["W4"] + weights["b4"]

    recon_normal = np.mean((infer(X_normal) - X_normal)**2, axis=1)
    recon_attack = np.mean((infer(X_attack) - X_attack)**2, axis=1)

    threshold = float(np.percentile(recon_normal, 95))
    tp_rate   = float(np.mean(recon_attack > threshold))
    fp_rate   = float(np.mean(recon_normal > threshold))

    print(f"\n[THRESHOLD] 95th-percentile normal error = {threshold:.6f}")
    print(f"[EVAL] Detection rate (TPR): {tp_rate*100:.1f}%")
    print(f"[EVAL] False positive rate:  {fp_rate*100:.1f}%")
    print(f"[EVAL] Normal  recon error:  mean={recon_normal.mean():.5f} p95={np.percentile(recon_normal,95):.5f}")
    print(f"[EVAL] Attack  recon error:  mean={recon_attack.mean():.5f} p95={np.percentile(recon_attack,95):.5f}")

    return threshold


def weights_to_c_header(weights: dict, threshold: float, out_path: str):
    """Emit C header with float32 weight arrays for TFLite Micro / manual inference."""
    lines = [
        "// autoencoder_weights.h — generated by train_autoencoder.py",
        "// Autoencoder 5→8→4→8→5 for LoRaMesher anomaly detection",
        "// DO NOT EDIT: regenerate with train_autoencoder.py",
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"#define AE_THRESHOLD  {threshold:.6f}f",
        "#define AE_INPUT_DIM   5",
        "#define AE_HIDDEN1     8",
        "#define AE_LATENT      4",
        "#define AE_HIDDEN2     8",
        "#define AE_OUTPUT_DIM  5",
        "",
    ]

    def arr_line(name: str, arr: np.ndarray) -> List[str]:
        flat = arr.flatten().tolist()
        vals = ", ".join(f"{v:.8f}f" for v in flat)
        return [f"static const float {name}[{len(flat)}] = {{", f"  {vals}", "};", ""]

    for key in ["W1","b1","W2","b2","W3","b3","W4","b4"]:
        lines.extend(arr_line(f"ae_{key}", weights[key]))

    with open(out_path, "w") as f:
        f.write("\n".join(lines))

    total_params = sum(weights[k].size for k in weights)
    print(f"\n[OUT] {out_path}  ({total_params} params = {total_params*4} bytes)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--log",       help="serial log file path")
    ap.add_argument("--synthetic", action="store_true", help="use synthetic data")
    ap.add_argument("--epochs",    type=int, default=300)
    ap.add_argument("--lr",        type=float, default=0.005)
    ap.add_argument("--out",       default=os.path.join(os.path.dirname(__file__),
                                   "node_d_autoencoder/autoencoder_weights.h"))
    args = ap.parse_args()

    if args.synthetic or (not args.log):
        print("[DATA] Using synthetic baseline + attack data")
        X_normal_raw, X_attack_raw = generate_synthetic(600, 150)
        X_normal = normalize_batch(X_normal_raw)
        X_attack = normalize_batch(X_attack_raw)
    else:
        raw = parse_serial_log(args.log)
        if len(raw) < 20:
            print("[WARN] < 20 samples in log. Using synthetic fallback.")
            X_normal_raw, X_attack_raw = generate_synthetic()
            X_normal = normalize_batch(X_normal_raw)
            X_attack = normalize_batch(X_attack_raw)
        else:
            X_normal = normalize_batch(np.array(raw, dtype=np.float32))
            _, X_attack_raw = generate_synthetic(0, 100)
            X_attack = normalize_batch(X_attack_raw)

    print(f"[DATA] Normal: {X_normal.shape}  Attack: {X_attack.shape}")
    print(f"[TRAIN] {args.epochs} epochs, lr={args.lr}")
    weights = train_numpy(X_normal, epochs=args.epochs, lr=args.lr)

    threshold = compute_threshold(weights, X_normal, X_attack)
    weights_to_c_header(weights, threshold, args.out)

    print(f"\n[DONE] Flash ESP32-S3 with autoencoder_weights.h included.")
    print(f"       Set AE_THRESHOLD = {threshold:.6f} in node_d_autoencoder.ino")


if __name__ == "__main__":
    main()
