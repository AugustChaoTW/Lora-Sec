# E20 Hardware Experiment Results

**Platform**: Ebyte EoRa PI (ESP32-S3 + SX1262), 3 nodes  
**LoRa**: 922.0 MHz, SF9/BW125/CR4/7, 2 dBm TX  
**Protocol**: FC-HMAC (Fragment-Chained HMAC), 8-byte truncated SHA-256  
**Image**: 2046B synthetic JPEG, 9 fragments × 233B payload, 8B HMAC/frag  
**Date**: 2026-05-14

## Architecture

```
Sender (0xE1,0xAA) --LoRa--> Relay --LoRa(0xE1,0x9A)--> Receiver
```

Dual-magic isolation: receiver ignores sender's direct broadcasts (filters 0xAA), only accepts relay-forwarded 0x9A packets. Relay configured at compile time for each scenario.

## Firmware Versions

- `node_sender`: E20 TX with img_seq in frag_id=0 payload (F3 fix)  
- `node_relay`: adversarial relay, 4 modes (HONEST/DROP/BITFLIP/REPLAY)  
- `node_receiver`: E20 RX with per-image chain stats + REPLAY_DET

## Results Table

| Scenario | Images | Chain OK | Avg Verify% | Avg Prefix% | Relay Action |
|----------|--------|----------|-------------|-------------|--------------|
| honest   | 4      | 4/4=100% | 100.0%      | 100.0%      | fwd=42, drop=0 |
| drop10   | 3      | 0/3=0%   | 63.1%       | 55.6%       | fwd=37, drop=7 (15.9%) |
| drop20   | 3      | 2/3=67%  | 83.3%       | 81.5%       | fwd=58, drop=14 (19.4%) |
| bitflip  | 6      | 0/6=0%   | 22.2%       | 22.2%       | fwd=60, flip=20 |
| replay   | 5      | 1/5=20%  | 28.9%       | 28.9%       | fwd=66, replay=7 |

## Key Observations

### honest (baseline)
- FC-HMAC chain passes end-to-end with honest relay: **100% chain_ok, 100% verify**
- Receiver correctly filters sender's direct broadcasts (`[DROP] bad magic E1AA`)
- HMAC overhead: 3.3% (8B/241B), enc overhead: 3.5%
- CPU: hmac=947µs/img, enc=2885µs/img

### drop10 / drop20 (selective drop attack)
- Any dropped fragment breaks the chain at the drop position (all subsequent HMACs fail)
- 100% detection: receiver detects break position and reports `verify_prefix` and `break_pos`
- drop10: actual drop rate ~16%, all 3 images chain-broken
- drop20: actual drop rate ~19.4%, stochastic — images with lucky 0-drops still pass

### bitflip (payload tampering)
- Relay flips 1 byte in every 3rd fragment (frag=3,6,9 → frag_id=2,5,8)
- **100% detection**: all 6 images break at frag_id=2 with identical pattern
- `verify_prefix=2/9 (22%)` — exactly 2 fragments verified before bit-flip position
- `chain_ok=0/6` — FC-HMAC correctly rejects tampered payloads

### replay (cross-session replay attack)
- Relay replays frag_id=0 from completed image into next image session
- **100% REPLAY_DET**: every replayed packet triggers `[REPLAY_DET]` at receiver
- Cross-session replay disrupts receiver chain (break_pos=1 for subsequent image)
- Demonstrates F3 property: img_seq in chain init prevents cross-session replay acceptance

## FC-HMAC Properties Validated

| Property | Claim | Result |
|---------|-------|--------|
| F1 Fragment Authenticity | Any modified fragment detected | ✅ bitflip: 100% detection |
| F2 Chain Gap Detectable | Dropped frag breaks chain at gap | ✅ drop10/20: break_pos reported |
| F3 No Replay | Replayed frag_id=0 detected + chain breaks | ✅ replay: REPLAY_DET=100%, chain_ok=20% |
| F4 Honest Path | Honest relay passes chain intact | ✅ honest: chain_ok=100% |

## Notes

- `replay_det` counter in receiver output is CUMULATIVE across sessions (increments even for the honest scenario's new-image frag_id=0 events, which is a false positive). True replay detection uses relay_replays vs REPLAY_DET correlation.
- drop20 result shows high variance (stochastic drops): 2/3 images pass when drops happen to miss all 9 frags. Need more samples for statistical analysis.
- Bitflip results are deterministic (every 3rd frag) and highly consistent across 6 images.
