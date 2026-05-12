#!/usr/bin/env python3
"""
E14 RSSI Localization — Collect RSSI from 5 observer nodes, run multilateration.

Usage:
  python3 collect_rssi.py /dev/ttyACM0 /dev/ttyACM1 /dev/ttyACM2 /dev/ttyACM3 /dev/ttyACM4

Reads RSSI_REPORT CSV lines from each serial port:
  RSSI_REPORT,0xAA,<seq>,<rssi_dBm>,<pos_x_cm>,<pos_y_cm>

After collecting N_SAMPLES_PER_SEQ measurements for each seq number,
runs weighted least-squares multilateration and prints estimated (x, y).

Calibration mode:
  CALIB_MODE=1 python3 collect_rssi.py ...
  Place attacker at CALIB_TRUE_POS_CM = (x, y) and measure PL_RSSI_REF, PL_N.
"""

import serial
import threading
import time
import math
import sys
import os
from collections import defaultdict

# Path loss model (calibrate with known-distance measurements)
PL_RSSI_REF = float(os.environ.get("PL_RSSI_REF", "-40.0"))   # dBm at d_ref
PL_N        = float(os.environ.get("PL_N",        "2.0"))      # path loss exponent
PL_D_REF    = float(os.environ.get("PL_D_REF",    "0.1"))      # reference dist (m)

BAUD         = 115200
N_OBSERVERS  = 5
CALIB_MODE   = int(os.environ.get("CALIB_MODE", "0"))
CALIB_POS    = (float(os.environ.get("CALIB_X", "100")),
                float(os.environ.get("CALIB_Y", "100")))   # cm

# Accumulated RSSI: {seq -> {node_id -> (rssi, pos_x, pos_y)}}
rssi_db = defaultdict(dict)
lock    = threading.Lock()


def rssi_to_dist_m(rssi_dbm: float) -> float:
    """Convert RSSI to distance using log-distance path loss model."""
    return PL_D_REF * 10 ** ((PL_RSSI_REF - rssi_dbm) / (10.0 * PL_N))


def multilaterate(measurements: dict) -> tuple:
    """
    Weighted least-squares multilateration.
    measurements: {node_id: (rssi_dbm, pos_x_cm, pos_y_cm)}
    Returns: (x_est_cm, y_est_cm, error_m)
    """
    nodes = list(measurements.values())
    if len(nodes) < 3:
        return None, None, None

    # Convert to meters
    positions = [(px / 100.0, py / 100.0) for (_, px, py) in nodes]
    distances = [rssi_to_dist_m(rssi) for (rssi, _, _) in nodes]
    weights   = [1.0 / max(d, 0.01) for d in distances]  # weight by 1/d

    # Iterative least squares (Gauss-Newton, 10 iterations)
    # Start at centroid of observer positions
    x = sum(p[0] for p in positions) / len(positions)
    y = sum(p[1] for p in positions) / len(positions)

    for _ in range(20):
        A_rows, b_rows = [], []
        for i, ((xi, yi), di, wi) in enumerate(zip(positions, distances, weights)):
            d_est = math.sqrt((x - xi)**2 + (y - yi)**2) + 1e-6
            A_rows.append([wi * (x - xi) / d_est, wi * (y - yi) / d_est])
            b_rows.append(wi * (d_est - di))

        # Normal equations
        AtA = [[sum(r[c]*r[d] for r in A_rows) for d in range(2)] for c in range(2)]
        Atb = [sum(A_rows[i][c] * b_rows[i] for i in range(len(b_rows))) for c in range(2)]

        det = AtA[0][0]*AtA[1][1] - AtA[0][1]*AtA[1][0]
        if abs(det) < 1e-12:
            break
        dx = -(AtA[1][1]*Atb[0] - AtA[0][1]*Atb[1]) / det
        dy = -(AtA[0][0]*Atb[1] - AtA[1][0]*Atb[0]) / det
        x += dx; y += dy
        if abs(dx) < 1e-5 and abs(dy) < 1e-5:
            break

    # Residual error
    residual = sum(
        w * abs(math.sqrt((x-xi)**2 + (y-yi)**2) - di)
        for (xi, yi), di, w in zip(positions, distances, weights)
    ) / sum(weights)

    return x * 100, y * 100, residual   # back to cm, residual in m


def calibrate(measurements: dict):
    """Print calibration info if true position known."""
    x_est, y_est, err = multilaterate(measurements)
    if x_est is None:
        return
    dx = x_est - CALIB_POS[0]
    dy = y_est - CALIB_POS[1]
    error_cm = math.sqrt(dx**2 + dy**2)
    print(f"[CALIB] true=({CALIB_POS[0]:.0f},{CALIB_POS[1]:.0f})cm "
          f"est=({x_est:.1f},{y_est:.1f})cm error={error_cm:.1f}cm")
    # Compute PL_N from measurements
    for node_id, (rssi, px, py) in measurements.items():
        true_d = math.sqrt((CALIB_POS[0]/100 - px/100)**2 + (CALIB_POS[1]/100 - py/100)**2)
        if true_d < 0.01: continue
        n_est = (PL_RSSI_REF - rssi) / (10 * math.log10(true_d / PL_D_REF + 1e-9))
        print(f"  node={node_id}: rssi={rssi:.1f}dBm d_true={true_d*100:.1f}cm n_est={n_est:.2f}")


def reader_thread(port: str):
    try:
        s = serial.Serial(port, BAUD, timeout=1)
        print(f"[SERIAL] Connected: {port}")
    except Exception as e:
        print(f"[ERR] Cannot open {port}: {e}")
        return

    while True:
        try:
            line = s.readline().decode(errors='replace').strip()
            if not line.startswith("RSSI_REPORT,"):
                if line: print(f"  [{port}] {line}")
                continue
            parts = line.split(",")
            if len(parts) < 6:
                continue
            _, node_id, seq_str, rssi_str, px_str, py_str = parts[:6]
            seq   = int(seq_str)
            rssi  = float(rssi_str)
            pos_x = float(px_str)
            pos_y = float(py_str)

            with lock:
                rssi_db[seq][node_id] = (rssi, pos_x, pos_y)
                meas = dict(rssi_db[seq])

            if len(meas) >= N_OBSERVERS:
                x_est, y_est, err = multilaterate(meas)
                if x_est is not None:
                    d_str = " ".join(f"{nid}:{rssi_to_dist_m(r)*100:.0f}cm"
                                     for nid, (r, _, _) in meas.items())
                    print(f"[LOC] seq={seq} est=({x_est:.1f},{y_est:.1f})cm "
                          f"residual={err*100:.1f}cm  [{d_str}]")
                    if CALIB_MODE:
                        calibrate(meas)
        except Exception:
            pass


def main():
    if len(sys.argv) < 2:
        print(f"Usage: python3 {sys.argv[0]} /dev/ttyACM0 [/dev/ttyACM1 ...]")
        print(f"  PL_RSSI_REF={PL_RSSI_REF} PL_N={PL_N} PL_D_REF={PL_D_REF}")
        sys.exit(1)

    ports = sys.argv[1:]
    print(f"=== E14 RSSI Localization ===")
    print(f"Path loss: RSSI_ref={PL_RSSI_REF}dBm n={PL_N} d_ref={PL_D_REF}m")
    print(f"Observers: {len(ports)} ports")
    print(f"Grid: A(0,0) B(200,0) C(100,100) D(0,200) E(200,200) cm")
    print()
    print("Output: [LOC] seq=N est=(x,y)cm residual=Rcm")
    print("Press Ctrl+C to stop.")
    print()

    threads = [threading.Thread(target=reader_thread, args=(p,), daemon=True)
               for p in ports]
    for t in threads:
        t.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n--- done ---")
        # Summary
        all_estimates = []
        with lock:
            for seq, meas in rssi_db.items():
                if len(meas) >= N_OBSERVERS:
                    x, y, e = multilaterate(meas)
                    if x is not None:
                        all_estimates.append((x, y, e))
        if all_estimates:
            xs = [e[0] for e in all_estimates]
            ys = [e[1] for e in all_estimates]
            print(f"Estimates: {len(all_estimates)}")
            print(f"Mean position: ({sum(xs)/len(xs):.1f}, {sum(ys)/len(ys):.1f}) cm")
            print(f"Std X: {(sum((x-sum(xs)/len(xs))**2 for x in xs)/len(xs))**0.5:.1f} cm")
            print(f"Std Y: {(sum((y-sum(ys)/len(ys))**2 for y in ys)/len(ys))**0.5:.1f} cm")


if __name__ == "__main__":
    main()
