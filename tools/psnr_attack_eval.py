#!/usr/bin/env python3
"""
E19-PSNR: Visual security evaluation for stride-based selective encryption.

Simulates attacker who has ciphertext but not key; measures PSNR/SSIM of
reconstructed image vs original for ENC_NONE / ENC_DC_ONLY / ENC_FULL.

Python port of node_sender.ino selective_enc() using same SESSION_KEY and logic.

Usage:
    python3 psnr_attack_eval.py [image.jpg ...]
    python3 psnr_attack_eval.py  # uses default test_200x150.jpg
"""

import sys
import hashlib
import hmac
import struct
import math
import pathlib
import numpy as np
from PIL import Image

SESSION_KEY = bytes([
    0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
])

ENC_NONE    = 0
ENC_DC_ONLY = 2
ENC_FULL    = 1

STRIDE = 64
BLOCK  = 16


def hmac_prf_keystream(nonce: int, pos: int) -> bytes:
    prf_in = struct.pack(">II", nonce, pos)
    return hmac.new(SESSION_KEY, prf_in, hashlib.sha256).digest()  # 32 bytes


def selective_enc(data: bytearray, mode: int, nonce: int) -> bytearray:
    out = bytearray(data)
    if mode == ENC_NONE:
        return out
    if mode == ENC_DC_ONLY:
        for i in range(0, len(out), STRIDE):
            ks = hmac_prf_keystream(nonce, i)
            blk = min(BLOCK, len(out) - i)
            for j in range(blk):
                out[i + j] ^= ks[j]
    elif mode == ENC_FULL:
        for i in range(0, len(out), 32):
            ks = hmac_prf_keystream(nonce, i)
            blk = min(32, len(out) - i)
            for j in range(blk):
                out[i + j] ^= ks[j]
    return out


def psnr(orig: np.ndarray, recon: np.ndarray) -> float:
    mse = np.mean((orig.astype(float) - recon.astype(float)) ** 2)
    if mse == 0:
        return float('inf')
    return 20 * math.log10(255.0 / math.sqrt(mse))


def ssim_channel(a: np.ndarray, b: np.ndarray) -> float:
    a, b = a.astype(float), b.astype(float)
    mu_a, mu_b = a.mean(), b.mean()
    sig_a = a.std()
    sig_b = b.std()
    sig_ab = ((a - mu_a) * (b - mu_b)).mean()
    C1, C2 = (0.01 * 255) ** 2, (0.03 * 255) ** 2
    return (2 * mu_a * mu_b + C1) * (2 * sig_ab + C2) / \
           ((mu_a**2 + mu_b**2 + C1) * (sig_a**2 + sig_b**2 + C2))


def ssim(orig: np.ndarray, recon: np.ndarray) -> float:
    if orig.ndim == 2:
        return ssim_channel(orig, recon)
    return float(np.mean([ssim_channel(orig[:,:,c], recon[:,:,c])
                           for c in range(orig.shape[2])]))


def attacker_reconstruct(orig_img: Image.Image, enc_bytes: bytearray) -> tuple:
    """
    Two attacker models:
    A1 (naive): feed encrypted bytes directly to JPEG decoder.
    A2 (format-aware): knows JPEG SOI/EOI markers and AC byte positions are
       plaintext (only stride-encrypted positions are ciphertext); tries to
       restore JPEG structure by zeroing encrypted positions before decode.
       This models an attacker who knows the encryption pattern (Kerckhoffs).
    Returns: (psnr_a1, ssim_a1, ok_a1), (psnr_a2, ssim_a2, ok_a2)
    """
    import io
    orig_arr = np.array(orig_img)

    def try_decode(buf):
        try:
            img = Image.open(io.BytesIO(bytes(buf))).convert("RGB")
            arr = np.array(img)
            if arr.shape != orig_arr.shape:
                img = img.resize(orig_img.size, Image.LANCZOS)
                arr = np.array(img)
            return psnr(orig_arr, arr), ssim(orig_arr, arr), True
        except Exception:
            return 0.0, 0.0, False

    # A1: naive
    r_a1 = try_decode(enc_bytes)

    # A2: format-aware — zero out encrypted positions (attacker knows STRIDE/BLOCK)
    # XOR with 0 doesn't help; attacker zeros the unknown ciphertext bytes,
    # hoping partial JPEG data from plaintext AC bytes allows partial decode.
    zeroed = bytearray(enc_bytes)
    for i in range(0, len(zeroed), STRIDE):
        blk = min(BLOCK, len(zeroed) - i)
        for j in range(blk):
            zeroed[i + j] = 0x00
    r_a2 = try_decode(zeroed)

    return r_a1, r_a2


def eval_image(img_path: pathlib.Path) -> dict:
    img = Image.open(img_path).convert("RGB")
    orig_arr = np.array(img)

    raw = bytearray(img_path.read_bytes())
    nonce = 0  # image_seq=0 matches sender default

    results = {}
    for mode_name, mode in [("ENC_NONE", ENC_NONE),
                             ("ENC_DC_ONLY", ENC_DC_ONLY),
                             ("ENC_FULL", ENC_FULL)]:
        enc_bytes = selective_enc(bytearray(raw), mode, nonce)
        enc_count = sum(1 for a, b in zip(raw, enc_bytes) if a != b)

        (p1, s1, ok1), (p2, s2, ok2) = attacker_reconstruct(img, enc_bytes)

        results[mode_name] = {
            "enc_bytes": enc_count,
            "enc_pct": enc_count / len(raw) * 100,
            # A1: naive attacker
            "psnr_a1": p1, "ssim_a1": s1, "decode_a1": ok1,
            # A2: format-aware attacker (knows encryption positions)
            "psnr_a2": p2, "ssim_a2": s2, "decode_a2": ok2,
        }

    return results


def security_verdict(r: dict, model: str) -> str:
    p = r[f"psnr_{model}"]
    ok = r[f"decode_{model}"]
    if not ok or p == 0:
        return "SECURE — decode fails"
    if p < 12:
        return f"SECURE — PSNR={p:.1f}dB (<12dB)"
    if p < 20:
        return f"MARGINAL — PSNR={p:.1f}dB (visually degraded)"
    return f"WEAK — PSNR={p:.1f}dB (recoverable)"


def print_results(img_path: pathlib.Path, results: dict):
    raw_size = len(img_path.read_bytes())
    print(f"\n{'='*75}")
    print(f"Image: {img_path.name}  ({raw_size} bytes)")
    print(f"{'='*75}")
    print(f"{'Mode':<14} {'Enc%':>6}  {'A1 PSNR':>9} {'A1 dec':>7}  {'A2 PSNR':>9} {'A2 dec':>7}")
    print(f"{'':14} {'':6}  {'(naive)':>9} {'':>7}  {'(fmt-aware)':>9}")
    print(f"{'-'*75}")
    for mode, r in results.items():
        p1 = f"{r['psnr_a1']:.1f}" if r['decode_a1'] else "n/a"
        p2 = f"{r['psnr_a2']:.1f}" if r['decode_a2'] else "n/a"
        d1 = "OK" if r['decode_a1'] else "FAIL"
        d2 = "OK" if r['decode_a2'] else "FAIL"
        print(f"{mode:<14} {r['enc_pct']:>5.1f}%  {p1:>9} {d1:>7}  {p2:>9} {d2:>7}")
    print()
    dc = results.get("ENC_DC_ONLY")
    if dc:
        print(f"DC_ONLY A1 (naive):      {security_verdict(dc, 'a1')}")
        print(f"DC_ONLY A2 (fmt-aware):  {security_verdict(dc, 'a2')}")


def main():
    default = pathlib.Path(__file__).parent.parent / \
              "hardware_experiments/E19_selective_enc/test_images/test_200x150.jpg"
    paths = [pathlib.Path(p) for p in sys.argv[1:]] if len(sys.argv) > 1 else [default]

    for p in paths:
        if not p.exists():
            print(f"[SKIP] not found: {p}")
            continue
        results = eval_image(p)
        print_results(p, results)

    # Summary table for paper
    print("\n--- Paper D Table (E19-PSNR eval) ---")
    print(f"{'Mode':<14} {'Enc%':>6}  {'A1 PSNR':>9} {'A1 verdict':<26}  {'A2 PSNR':>9} {'A2 verdict'}")
    print(f"{'-'*90}")
    for p in paths:
        if not p.exists():
            continue
        results = eval_image(p)
        for mode, r in results.items():
            p1 = f"{r['psnr_a1']:.1f}dB" if r['decode_a1'] else "n/a"
            p2 = f"{r['psnr_a2']:.1f}dB" if r['decode_a2'] else "n/a"
            v1 = security_verdict(r, 'a1')
            v2 = security_verdict(r, 'a2')
            print(f"{mode:<14} {r['enc_pct']:>5.1f}%  {p1:>9} {v1:<26}  {p2:>9} {v2}")


if __name__ == "__main__":
    main()
