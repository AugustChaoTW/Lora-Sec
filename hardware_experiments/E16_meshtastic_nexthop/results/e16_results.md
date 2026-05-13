# E16 Results: Meshtastic next_hop Hijacking

**Date**: 2026-05-14  **Duration**: 120 s  **Boards**: 3× EoRa PI

## Metrics

| Metric | Value |
|---|---|
| Sender TX | 25 packets |
| Attacker intercepted | 25/25 = **100%** |
| Gateway PDR | 25/25 = **100%** |
| Gateway via=RELAY | 25/25 (hop_limit=2) |
| Gateway via=DIRECT | 25/25 (hop_limit=3) |

## Key Findings

1. **Active relay confirmed**: attacker receives payload[0]=0xAA on every packet before forwarding
2. **Gateway PDR=100%**: attack invisible to availability-based IDS
3. **Only anomaly signal at gateway**: hop_limit 3→2 (indicates relay hop, but not attacker identity)
4. **Contrast with LoRaMesher E5b**: E5b PDR=0% (detectable); E16 PDR=100% (undetectable at gateway)

## Attack Mechanism

```
Sender A → [to=GW, next_hop=0xCC] → Attacker C  (RECEIVES payload)
                                         ↓ forward [next_hop=GW, hop_limit-=1]
                                      Gateway B  (sees hop_limit=2, cannot identify C)
```

## Firmware Notes

Bug fixed: attacker must filter `buf[0]==0xFE` (ROUTE_POISON) to avoid self-defeating forwarding loop.
