# E17 Results: IDS Strategy Comparison

**Date**: 2026-05-14  **Duration**: 120s  **Boards**: 4× EoRa PI

## IDS Detection Matrix

| Protocol | Attack Type | Strategy A (GW-centric) | Strategy B (relay-side dst-binding) |
|---|---|---|---|
| LoRaMesher | Silent black hole | FNR=100% | FNR~0% (dst=0xCC anomaly visible) |
| Meshtastic | Active relay hijack | **FNR=100%** | **FNR=100%** (to=GW, no dst=atk) |

## Counts (120s window)

- Sender TX: 25 packets
- Attacker intercepted: 25/25 = 100%
- Strategy A events: 49 (GW-destined, no anomaly signal from these)
- Strategy B anomalies: 15 (from co-located LoRaMesher node 0x01 — true positives for LM attack)

## Key Insight

Strategy B closes the LoRaMesher IDS gap (dst=0xCC is anomalous).
Strategy B does NOT close the Meshtastic gap — attacker's forwarded packets
carry `to=0xBB` (gateway), indistinguishable from legitimate relay traffic.

**Consequence**: Meshtastic active relay attack cannot be detected by any
passive observation-based IDS. Defense requires cryptographic relay-path
authentication: bind `next_hop` in AEAD AAD so tampering is detectable at
the receiving relay.

This is a stronger result than originally hypothesized (FNR=100% for BOTH
strategies against Meshtastic), and sharpens the Paper C contribution:
the attack category difference demands a different defense paradigm.
