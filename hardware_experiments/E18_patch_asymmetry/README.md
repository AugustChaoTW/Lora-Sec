# E18: Patch Asymmetry — HMAC Effective for LoRaMesher, Ineffective for Meshtastic

## Thesis
The HMAC-SHA256 patch that closes the LoRaMesher attack does NOT close the equivalent Meshtastic attack. The asymmetry arises because:
- **LoRaMesher attack**: control-plane poisoning (Hello/RouteUpdate) → HMAC on Hello blocks it
- **Meshtastic attack**: data-plane next_hop hijack (MeshPacket header field) → HMAC on NodeInfo is irrelevant

## Expected Results
| Target | Attack | Patch | PDR | Outcome |
|---|---|---|---|---|
| LoRaMesher + HMAC | Forged Hello (invalid MAC) | HMAC on Hello | 100% | BLOCKED |
| Meshtastic + HMAC | ROUTE_POISON (bypasses NodeInfo HMAC) | HMAC on NodeInfo | 0% | BYPASSED |

## Node Assignment (4 boards needed, 6 available)
- Node A (0xAA): LoRaMesher+HMAC sender — `node_sender_hmac/`
- Node A2 (0xA2): Meshtastic+HMAC sender — `node_sender_mesh_hmac/`
- Node B (0xBB): Gateway (receives both) — `node_gateway/`
- Node C (0xCC): Attacker — `node_attacker_hmac/`

## Correct Meshtastic Fix
Bind `next_hop` in AEAD additional authenticated data (AAD):
```
ciphertext = AES-CTR(payload, psk)
tag = HMAC(from || to || next_hop || pkt_id || ciphertext, psk)
```
Any tampering with `next_hop` invalidates `tag` → relay authentication at the packet level.
See `phase2_tamarin_models/meshtastic/meshtastic_patch.spthy` (planned).

## Key Serial Log Lines
- `[RESULT] LoRaMesher+HMAC: MAC verification FAILS — attack BLOCKED`
- `[RESULT] Meshtastic+HMAC: poison accepted — attack SUCCEEDS`
- `[KEY-FINDING] HMAC protects control plane; attack is on data-plane next_hop`
- `[ASYMMETRY] LoRaMesher+HMAC: BLOCKS attack | Meshtastic+HMAC: FAILS`
