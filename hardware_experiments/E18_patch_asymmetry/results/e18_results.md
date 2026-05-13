# E18 Results: Patch Asymmetry

**Date**: 2026-05-14  **Duration**: 120s  **Boards**: 4× EoRa PI

## Summary

| Target | Attack | Patch | Outcome |
|---|---|---|---|
| LoRaMesher + HMAC | Forged Hello (invalid MAC) | HMAC on Hello | **BLOCKED** (7/7 attempts rejected) |
| Meshtastic + HMAC | ROUTE_POISON (bypasses NodeInfo HMAC) | HMAC on NodeInfo | **BYPASSED** (7/7 accepted, poisoned=YES throughout) |

## Key Logs

```
[LM_HMAC]   [PATCH-RESULT] LoRaMesher+HMAC: attack blocked — PDR should be 100%
[MESH_HMAC] [TX] pkt_id=1 to=0xBB next_hop=0xCC poisoned=YES (HMAC insufficient!)
[ATTACKER]  [RESULT] LoRaMesher+HMAC: MAC verification FAILS — attack BLOCKED
[ATTACKER]  [RESULT] Meshtastic+HMAC: poison accepted — attack SUCCEEDS
[MESH_HMAC] [ROOT-CAUSE] Attack vector is MeshPacket.next_hop (data plane),
            not NodeInfo (control plane). Different layer.
[MESH_HMAC] [CORRECT-FIX] Bind next_hop in AEAD AAD or sign MeshPacket header.
```

## Root Cause

Same HMAC primitive applied at different layers produces asymmetric security:
- **LoRaMesher**: attack vector is Hello (control plane) → HMAC on Hello closes it
- **Meshtastic**: attack vector is `MeshPacket.next_hop` (data plane) → HMAC on NodeInfo is irrelevant

## Correct Meshtastic Fix

```
tag = HMAC(from || to || next_hop || pkt_id || ciphertext, channel_psk)
```
Tampering with `next_hop` invalidates `tag` at the receiving relay.
