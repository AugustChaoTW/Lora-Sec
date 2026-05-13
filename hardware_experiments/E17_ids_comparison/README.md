# E17: Gateway-Centric vs Relay-Side IDS Comparison

## Purpose
Quantify the false-negative rate of gateway-centric IDS vs relay-side dst-binding IDS against the LoRaMesher silent black hole attack.

## Expected Results
| Strategy | Detection Rate | FNR |
|---|---|---|
| Strategy A: Gateway-centric | 0% | 100% |
| Strategy B: Relay-side dst-binding | ~100% | ~0% |

## Node Assignment (6 boards)
- Node A (0xAA): Sender — `node_sender_lm/`
- Node B (0xBB): Gateway — `node_gateway_lm/`
- Node C (0xCC): Attacker — `node_attacker/`
- Node D (0xDD): Observer (relay-side IDS) — `node_observer/`
- Node E (0xEE): Second sender (load) — `node_sender_lm/` with NODE_ID=0xEE
- Node F (0xFF): Second observer (gateway-side IDS) — monitors gateway only

## Protocol
1. Flash all 6 boards
2. Run 60s baseline (no attack)
3. Run 120s attack phase
4. Collect serial logs

## Key Serial Log Lines
- `[IDS-A]` — gateway-centric events (misses attack)
- `[IDS-B] ANOMALY` — relay-side true positive
- `[CONTRAST]` — explicit FNR comparison
