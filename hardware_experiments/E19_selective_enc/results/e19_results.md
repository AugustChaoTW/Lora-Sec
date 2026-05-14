# E19 — Selective Encryption + FC-HMAC: Hardware Results

**Date**: 2026-05-14  
**Hardware**: 2× Ebyte EoRa PI (ESP32-S3 + SX1262)  
**Duration**: 120 seconds  
**Frequency**: 922.0 MHz (separate from E16/E18 at 923.0 MHz)

---

## Configuration

| Parameter | Value |
|-----------|-------|
| LoRa SF | 9 |
| LoRa BW | 125 kHz |
| LoRa CR | 4/7 |
| TX Power | 2 dBm |
| Sync Word | 0x34 |
| Fragment payload | 233 B |
| Fragment header | 7 B (magic 2B + frag_id 2B + total_frags 2B + flags 1B) |
| HMAC size | 8 B |
| Total packet | 248 B |
| Image size | 2046 B (synthetic JPEG, 200×150 equiv) |
| Fragments/image | 9 |
| Encryption mode | DC_ONLY (HMAC-SHA256 stream cipher on bytes at 64B boundaries) |
| Encryption key | AES-128 session key (pre-shared) |

---

## Results (120-second run, 8 complete images)

| Metric | Value |
|--------|-------|
| Images transmitted | 8 |
| Images received (complete) | 8 |
| Fragments sent | 72 |
| Fragments received | 72 |
| Fragments verified (HMAC OK) | 72 |
| Chain ok = 100% | 8/8 images |
| Chain broken | 0/8 images |
| Packet delivery ratio (PDR) | 100% |
| FC-HMAC verification rate | 100% |

---

## Performance Metrics

| Metric | Value |
|--------|-------|
| Airtime per 248B fragment | 1691890 μs (~1.692 s) |
| Airtime per 197B fragment (last) | 1347878 μs (~1.348 s) |
| Total airtime per image (9 frags) | 14882 ms (~14.9 s) |
| HMAC CPU time per image | ~950 μs (all 9 frags combined) |
| Selective enc CPU per image | ~2807 μs (DC_ONLY, 9 HMAC-PRF calls) |
| HMAC overhead per packet | 8 B / 241 B = **3.3%** |
| HMAC overhead per image | 72 B / 2046 B = **3.5%** |
| DC_ONLY encrypted bytes | 496 / 2046 = **24.2%** |
| RSSI at receiver | −29 dBm (consistent) |

---

## Overhead Summary

```
Per-packet:  [233B payload | 8B HMAC] = 8/241 = 3.3% HMAC overhead
Per-image:   9 frags × 8B HMAC / 2046B = 3.5%
Enc bytes:   ~496/2046 = 24.2% (DC-coefficient approximation: every 64th byte, 16B block)
```

---

## Serial Log Sample (image seq=6, perfect chain)

```
[TX] frag=1/9 pkt=248B air=1691750us hmac=3B31591E73C35B27
[RX] [NEW_IMG] total_frags=9
[RX] [OK] frag=1/9 plen=233 rssi=-29.0 hmac=OK
[TX] frag=2/9 pkt=248B air=1691881us hmac=E6C997CE03111DCA
[RX] [OK] frag=2/9 plen=233 rssi=-29.0 hmac=OK
...
[TX] frag=9/9 pkt=197B air=1347878us hmac=5E01974CCCB03ABD
[RX] [OK] frag=9/9 plen=182 rssi=-29.0 hmac=OK
[RX] [IMG_DONE] seq=6 recv=9/9 verified=9 failed=0 chain_ok=100% chain_broken=NO
[RX] [OVERHEAD] hmac_bytes=72 / img_payload_bytes~=1638 = 3.3%
[TX] [DONE] img=6 frags=9 total_air=14882ms hmac_cpu=950us enc_cpu=2808us overhead=3.5%
[TX] [ENC] mode=DC_ONLY enc_bytes~=496/2046 (24%)
```

---

## Implementation Notes

**Selective encryption**: Hardware AES-CTR (mbedTLS) crashes ESP32-S3 Arduino framework due to hardware AES accelerator semaphore initialization issue. Replaced with HMAC-SHA256-based stream cipher (software-only): keystream = HMAC(K, nonce ‖ block_position), XOR applied to DC-block bytes. Functional equivalence preserved; AES-CTR benchmark done separately.

**RF interference**: Boards 1–4 (E16–E18 firmware) transmit on 923.0 MHz causing ~50% CRC collision rate when E19 used same frequency. Fixed by moving E19 to 922.0 MHz.

**FreeRTOS assert fix**: Original `mbedtls_aes_crypt_ctr()` triggered `xQueueGenericSend queue.c:937` assert in ESP32-S3 Arduino hardware AES driver. Fixed by removing mbedTLS AES dependency.

**Interrupt-driven TX**: Replaced blocking `radio->transmit()` with `startTransmit()` + DIO1 ISR to avoid the FreeRTOS queue assert.

---

## Contribution to Paper D

- **RQ1 (airtime)**: 9 × 1.692s = 15.23s per 2046B image at SF9/BW125
- **RQ2 (HMAC overhead)**: 3.3% per packet, 3.5% per image — confirms design target <5%
- **RQ3 (FC-HMAC correctness)**: 72/72 frags verified across 8 images, chain_ok=100% at PDR=100%
- **RQ4 (selective enc fraction)**: DC_ONLY encrypts 24.2% of image bytes
- **CPU overhead**: HMAC 950μs + enc 2807μs per image ≈ 3.76ms total crypto overhead
