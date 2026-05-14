# E13 Hardware Experiment Results: KLD + Hamming Distance IDS

**Platform**: Ebyte EoRa PI (ESP32-S3 + SX1262)  
**Nodes**: 5 (A=0xAA sender, R1=0x11 relay, C=0xCC rogue relay, B=0xBB gateway, D=0xDD IDS)  
**Topology**: A → R1 → C(rogue) → B, IDS=D promiscuous  
**LoRa**: 923.0 MHz, SF9/BW125/CR4/7, 10 dBm  
**Attack**: Rogue relay with valid PSK — sends HMAC-valid Hellos, drops all data packets  
**Date**: 2026-05-14

---

## Attack Summary

| Metric | Value |
|--------|-------|
| Rogue Hellos sent (HMAC=VALID) | 15 |
| Packets dropped by rogue | 35 |
| Gateway RX packets | 24 |
| HMAC patch response | ACCEPTS (PSK valid → route poisoned) |

Rogue relay 0xCC holds the network PSK. It sends valid authenticated Hellos advertising metric=0 (better than honest path). Honest nodes update their routing table: `next_hop[B] = 0xCC`. All subsequent data packets routed to 0xCC are silently dropped.

**Gateway PDR ≈ 0%** once rogue route established (gateway sees silence).

---

## IDS Detection Results

### KLD (Kullback-Leibler Divergence) — inter-arrival time distribution

| Node | Max KLD observed | Threshold | Alert fired? | Verdict |
|------|-----------------|-----------|--------------|---------|
| 0x11 (honest relay) | **754** | 200 | ✅ 2 alerts | **FALSE POSITIVE** |
| 0x22 (sender) | 129 | 200 | ❌ 0 alerts | Missed |
| 0xCC (rogue relay) | — (not observed transmitting data) | 200 | ❌ 0 alerts | **MISSED** |
| 0xBB (gateway) | 7 | 200 | ❌ 0 alerts | Normal |

KLD fired on 0x11 (honest relay) due to variable Hello retransmit intervals (754 >> 200).  
KLD did **NOT** detect 0xCC: the rogue relay transmits valid Hellos at regular intervals → KLD near-normal.  
The actual attack (data drop) is invisible to inter-arrival-time analysis.

### Hamming Distance (HD) — sequence number delta

HD alerts = 18 total across all nodes. All are `near-replay suspect` from sequence number increments of 1–2.  
**No HD alert is attack-specific**: HD captures near-sequential seq numbers, which is normal forwarding behavior.

---

## Key Finding

> **Authentication proves origin; it cannot enforce relay behavior.**

The HMAC patch correctly rejects unauthenticated route injections. But a rogue node holding a valid PSK can:
1. Send HMAC-valid route advertisements (accepted by all nodes)
2. Drop all data packets at the forwarding layer

KLD+HD IDS **cannot detect** this attack class because:
- The rogue relay's control-plane behavior (Hello timing, sequence numbers) is indistinguishable from an honest relay
- The data-plane drop is not observable by an IDS monitoring broadcast traffic
- Only a relay-side or path-probing monitor can detect selective forwarding failure

**IDS verdict**: FNR = 100% for rogue relay (authenticated insider) with KLD+HD strategy.  
**False positive rate**: KLD generated 2 alerts on honest relay 0x11 (routing overhead variability).

---

## Comparison with E15

| IDS Strategy | Attack detected? | FPR | Notes |
|---|---|---|---|
| E13: KLD + HD | ❌ No (FNR=100%) | ~1 FP (0x11) | Cannot see data-layer drop |
| E15: TFLite Autoencoder | ✅ 0xCC detected | High (0x11 also flagged) | Feature-based, sees traffic patterns |

---

## Files

- Logs: `e13_node_{a,b,c,d,r1}.log`
- Common header: `../e13_common.h`
- Firmware: `../node_{a,b,c_attacker,d_ids,r1}/`
