---
title: "Paper D — Contribution Document"
target: IEEE Internet of Things Journal (IoT-J)
backup: IEEE TII / Computer Networks
status: E19 hardware complete; PSNR eval complete; Tamarin PROVED; E20 hardware complete (all 5 scenarios validated 2026-05-14)
date: 2026-05-14
---

# Authenticated Selective Encryption for Image Transmission over LoRa

## Thesis

Under LoRa 255-byte payload and ETSI 1% duty-cycle constraints, a
structure-approximating selective encryption scheme targeting DCT sensitivity
regions (fixed-stride: every 64B window, first 16B XOR'd with HMAC-SHA256
keystream) encrypts only 24% of image bytes while achieving visual-domain
obfuscation (PSNR_attack target: <20 dB — to be confirmed by E19-PSNR eval),
with CPU encryption load reduced to 24% of full-encryption cost (2807 μs vs
~11600 μs estimated for full) at identical airtime and PDR.
Fragment-Chained HMAC (FC-HMAC, 8B truncated, chain-linked per fragment)
achieves 3.3% per-packet overhead and enables chain-break detection on any
single tampered or reordered fragment. E20 hardware validation (3-node relay
experiment, 2026-05-14) confirms: honest relay 100% chain-ok; bit-flip attack
100% detected (break at flipped frag); drop10/drop20 chain breaks at drop
position; cross-session replay triggers REPLAY_DET at 100% rate (F3 property).

---

## Hardware Note (EoRa PI adaptation)

Issue spec: ESP32-CAM + SX1276. Actual hardware: EoRa PI (ESP32-S3 + SX1262).
- No camera: use embedded synthetic JPEG (xxd byte array) for protocol validation
- mbedTLS built into ESP32 Arduino SDK → HMAC-SHA256 + AES-CTR available
- 2MB PSRAM → sufficient for 200×150 JPEG (~8KB)
- SX1262 vs SX1276: same LoRa modulation, different RadioLib API
- Camera (OV2640) attachment: Phase 3, DVP wiring to EoRa PI GPIO

Phase 1 (current): synthetic JPEG → FC-HMAC → LoRa TX → measure airtime/overhead
Phase 3 (future): live OV2640 capture

---

## Algorithm Design

### 1. JPEG Selective Encryption

JPEG DCT structure:
- Each 8×8 block → 64 DCT coefficients
- DC coefficient (index 0): average block brightness, highest visual impact
- AC coefficients: high-frequency detail, lower individual impact

**Implementation (Phase 1 — structure-approximating):**

True DC coefficient positions require Huffman stream parsing (variable-length
codes → DC bytes not at fixed offsets). Phase 1 uses a conservative
fixed-stride approximation as a computationally lightweight proxy:

```
DC_ONLY (implemented): XOR bytes [i .. i+16) for every i in {0, 64, 128, ...}
  → encrypts 16/64 = 25% of bytes per stride window
  → hardware measured: 496/2046 = 24.2% of image bytes (see E19 results)
  → CPU: 2807 μs per 2046B image (9 HMAC-PRF calls)
```

Encryption granularities (planned Phase 2 — with JPEG parser):
```
FULL:    HMAC-PRF stream cipher on entire byte stream → 100% ciphertext
DC-only: HMAC-PRF on true DC coefficient bytes only   → ~5-10% ciphertext (actual)
ROI:     HMAC-PRF on ROI macroblock bytes             → ~30-40% ciphertext
```

Visual security result (E19-PSNR eval, `tools/psnr_attack_eval.py`):

| Mode | Enc bytes | A1 naive | A2 format-aware |
|------|-----------|----------|-----------------|
| ENC_NONE | 0% | PSNR=∞ (perfect) | N/A |
| ENC_DC_ONLY | 24.9% | **JPEG decode FAIL** | **JPEG decode FAIL** |
| ENC_FULL | 99.8% | **JPEG decode FAIL** | **JPEG decode FAIL** |

DC_ONLY at 24.9% encrypted bytes completely prevents JPEG reconstruction under
both attacker models. PSNR target superseded: decode failure is stronger.
A2 (format-aware) attacker zeroes encrypted positions but JPEG structure still
broken because stride starts at byte 0 (SOI marker) and Huffman tables corrupted.
**Status: ✅ COMPLETE — #59 resolved**

**Limitation:** Phase 1 stride-based approach is NOT true DC coefficient
encryption. True DC extraction requires Huffman stream decoder. The
approximation covers DCT-sensitive regions approximately; actual coverage
overlap with DC bytes to be quantified (see `tools/jpeg_dc_offset_extractor.py`).

### 2. Fragment-Chained HMAC (FC-HMAC)

Problem: JPEG spans 30–60 LoRa packets. Packet loss breaks integrity.
Goal: verify received fragments even with partial loss, detect tampering.

Design (as implemented in E19):
```
Session key K: 16-byte pre-shared (AES-128 equivalent)

Packet i structure:
  [ magic(2B:E1,9A) | fragment_id(2B) | total_frags(2B) | flags(1B) | payload(N) | hmac8(8B) ]

hmac8[i] = HMAC-SHA256(K, fragment_id || flags || payload || hmac8[i-1])[:8]

Initial: hmac8[-1] = HMAC-SHA256(K, 0x00000000 || total_frags)[:8]
  (image_seq NOT included — replay risk; see #62 F3 gap)
```

**Hardware result (E19, 120s, SF9/BW125):**
- 72/72 fragments verified, chain_ok=100%, PDR=100%
- HMAC overhead: 8B/241B = 3.3% per packet, 72B/2046B = 3.5% per image
- HMAC CPU: ~950 μs per image (9 fragments)
- Total crypto CPU: 950 + 2807 = 3757 μs ≈ 3.76ms per 2046B image

Chain verification:
- Receiver maintains chain state: last_valid_hmac
- For each received packet: verify HMAC using stored chain state
- Lost packet breaks chain at that point; receiver can:
  - Re-request retransmit (if ACK mode)
  - Accept partial verification: count verified prefixes

Verifiability under loss:
- 10% loss: expect ~90% packets received, ~80-90% in unbroken prefix chains
- 20% loss: expect ~80% received, ~60-75% verifiable

HMAC truncation analysis:
- 4B (32-bit): forgery prob = 2^-32 per attempt — too weak
- 8B (64-bit): forgery prob = 2^-64 — sufficient (target)
- 12B (96-bit): forgery prob = 2^-96 — strong but wastes payload

Target: 8B HMAC truncation, 3.1% payload overhead per packet.

### 3. Airtime Model

LoRa airtime formula (SF=9, BW=125, CR=4/7):
- Symbol duration: Ts = 2^SF / BW = 4.096ms
- Measured airtime per 248B fragment: 1692ms (E19 hardware)
- Measured airtime per image (9 frags, 2046B): 14.9s

**Airtime savings clarification (see #60):**

Selective encryption does NOT reduce airtime when transmitting full image.
Packet count = ceil(img_size / 233B) regardless of encryption mode.
Airtime depends on packet count × packet size, not encrypted byte count.

| Metric | ENC_NONE | ENC_DC_ONLY | ENC_FULL |
|--------|----------|-------------|----------|
| Packet count | 9 | 9 | 9 |
| Airtime | 14.9s | 14.9s | 14.9s |
| CPU enc time | 0 μs | 2807 μs | ~11600 μs (est.) |
| Encrypted bytes | 0% | 24.2% | 100% |

**CPU savings** from DC_ONLY: 2807 μs vs ~11600 μs for FULL = ~76% reduction.
This is the quantifiable efficiency gain, not airtime.

**Airtime reduction** only possible via ROI-only partial transmission (E21):
- Skip non-ROI packets entirely → transmit 24% of packets
- Receiver reconstructs partial image (ROI only)
- True airtime reduction: ~76% at cost of incomplete image

---

## Experiments

### E19 — Selective Encryption Overhead (2× EoRa PI) ✅ COMPLETE
- Hardware: 2× EoRa PI (ESP32-S3 + SX1262), 922.0 MHz, SF9/BW125/CR4/7, 2 dBm
- 120s run: 8 complete images, 72/72 fragments verified, chain_ok=100%, PDR=100%
- Airtime per image: 14.9s; HMAC overhead: 3.3% per packet; crypto CPU: 3.76ms
- DC_ONLY encrypted: 24.2% of image bytes; CPU: 2807 μs (vs ~11600 μs for FULL est.)
- Results: `hardware_experiments/E19_selective_enc/results/e19_results.md`

### E19-PSNR — Visual Security Evaluation (PC simulation) ✅ COMPLETE (#59)
- Script: `tools/psnr_attack_eval.py`
- Result: DC_ONLY 24.9% enc → JPEG decode FAIL for both naive + format-aware attackers
- Stronger than PSNR target: attacker gets zero visual information, not just degraded image
- Root cause: stride covers byte 0 (SOI marker) + Huffman tables → format broken

### E20 — FC-HMAC Verifiability Under Packet Loss (3× EoRa PI) ⬜ PENDING (#61)
- Hardware: sender + relay/dropper + receiver
- Drop 0%, 10%, 20% of fragments; measure verifiable prefix length
- Compare FC-HMAC (chained) vs per-packet HMAC (Ryczek 2025 model)
- Also: bit-flip detection rate, replay detection rate
- Key result: verifiability vs loss rate table

### E21 — CPU/Energy Tradeoff: ENC_NONE vs DC_ONLY vs FULL (2× EoRa PI) ⬜ PENDING
- Vary: ENC_NONE / ENC_DC_ONLY / ENC_FULL on same 2046B JPEG
- Measure: CPU enc time, HMAC CPU, total crypto overhead
- Baseline: no encryption + no HMAC
- If Joulescope available: mWh per image per mode

---

## Tamarin Model

File: `phase2_tamarin_models/paper_d/fc_hmac.spthy`

**Status: ✅ ALL 4 LEMMAS PROVED — 1.53s** (Tamarin 1.13.0, Docker lora-mesh-tamarin)

Model structure:
- `mac/2` as perfect PRF (opaque, no inversion equation)
- `OneSetup` + `ReceiverOnce` restrictions bound state space
- `Sender_Init`: h0 = mac(K, <sess, tf>)
- `Sender_Frag1/2`: tag_i = mac(K, <id, f, p, tag_prev>), put on Out(...)
- `Receiver_Frag1/2`: checks Eq(tag_rcv, mac(K, <...>)) via restriction

**Firmware note (F3):** Model requires `img_seq` in chain init.
E19 firmware omits img_seq (F3 gap noted as limitation).
E20 firmware adds img_seq to init (frag_id=0 payload[0..3] carries img_seq).

Verification results:
| Lemma | Result | Steps |
|-------|--------|-------|
| F4 Executability | **PROVED** | 14 |
| F1 Fragment_Authenticity | **PROVED** | 10 |
| F2 Chain_Gap_Detectable | **PROVED** | 3 |
| F3 No_Replay | **PROVED** | 12 |

Log: `phase2_tamarin_models/paper_d/fc_hmac_verification.log`

---

## Feynman Self-Examination (Methodology)

*Answers to crane Feynman probing questions — used to harden §3 Methods.*

**Q1: Explain methodology to a first-year student.**
We send a camera image over a slow radio (LoRa: max 255 bytes per packet, 1%
airtime duty cycle). The image is split into ~9 packets. We encrypt only the
"most important" bytes (DCT sensitivity approximation: first 16B of every 64B
block) instead of everything — this cuts CPU cost by 76%. To verify integrity,
each packet carries an 8-byte MAC that chains to the previous packet's MAC.
So if someone tampers with packet 3, packets 4–9 all fail verification. The
chain is the key novelty vs. simply MACing each packet independently.

**Q2: Why FC-HMAC instead of independent per-packet HMAC (Ryczek 2025)?**
Per-packet HMAC (Ryczek): each packet independently verifiable, loss does not
cascade. Attack: attacker can silently drop and replace individual packets without
breaking other packets' MACs.
FC-HMAC: tag[i] = HMAC(K, frag_id || payload || tag[i-1]). Attacker who replaces
packet i must also forge tag[i], which requires K. More importantly, reordering
or inserting a fragment from a different session is immediately detectable because
the chain state won't match. Simpler method (per-packet) fails to detect ordering
attacks and cross-session replay.

**Q3: What assumptions does the method make? What breaks when violated?**
- **K is secret**: if K is compromised, all MACs are forgeable. Mitigation: per-session
  HKDF-derived keys.
- **img_seq is unique**: if sender reuses img_seq, F3 (no replay) breaks. Current
  E19 firmware uses monotonic counter (image_seq++) — wraps at uint32 max (never
  in practice). But firmware currently omits img_seq from chain init — see #62.
- **No parallel sessions**: model has OneSession restriction. Real multi-device
  deployments need per-sender session namespacing.
- **MAC security**: modeled as perfect PRF. Real HMAC-SHA256 truncated to 8B has
  2^-64 forgery probability per attempt — sufficient under low-rate LoRa.

**Q4: If reviewer says "this is just HMAC with minor modifications" — defense?**
Three novelties that distinguish from prior work:
1. **Fragment chaining** (not in Ryczek, not in TinySec): each tag covers the
   previous tag, making the chain a commitment to the entire prior transmission
   sequence. This is structural, not cosmetic.
2. **Hardware implementation on LoRa with 255B constraint**: Ryczek uses AES-CBC
   (full) + independent HMAC. We replace both with HMAC-SHA256 PRF (software,
   no HW AES crash) + FC chain. The 255B constraint makes every byte count;
   8B chain HMAC at 3.3% overhead is a deliberate design tradeoff.
3. **Formal verification**: F2 (no silent gap) and F3 (no replay) are machine-
   checked by Tamarin Prover. Ryczek has no formal model.

**Q5: Feynman break — where explanation still fails:**
"DC-only encrypts the most important bytes" — but the bytes encrypted are at
fixed stride offsets, not actually DCT coefficient positions. A critic could
say the visual obfuscation result comes from header corruption (SOI marker at
byte 0), not from semantic DCT-domain encryption. This is the B1 limitation.
The honest answer: we encrypt critical JPEG format bytes (which happen to
include both headers and AC/DC data), achieving format-level obfuscation,
not frequency-domain selective encryption. Phase 2 would do true DC parsing.

---

## Related Work Gap

Nearest prior work: **Ryczek & Sobieraj (Electronics 2025)** — ESP32-CAM + E220 LoRa,
20 kB images, AES-128-CBC full encryption, HMAC-SHA256 per-packet, PDR=100%, <1% energy overhead.

| Dimension | Ryczek 2025 | **Paper D** |
|-----------|-------------|-------------|
| Encryption | Full (AES-128-CBC) | Selective (stride-based, 24% bytes) |
| Authentication | Per-packet HMAC (independent) | FC-HMAC chain (linked per fragment) |
| Loss behavior | Each packet independent | Chain break; verifiable prefix |
| Formal verification | None | Tamarin F2+F3 (#62) |
| HMAC overhead | ~4–5% est. | 3.3%/packet, 3.5%/image (measured) |
| PSNR under attack | Not measured | JPEG decode FAIL (#59 closed) |
| Hardware | ESP32-CAM + E220 | ESP32-S3 + SX1262 (EoRa PI) |

**Differentiation**: FC-HMAC chain enables ordering-attack detection + replay detection
(vs Ryczek's per-packet approach which is order-agnostic). Formal model proves
chain-gap and replay properties. Selective encryption reduces CPU load 76%.

---

## Open Items (ordered by priority)

- [x] Algorithm design (FC-HMAC + selective enc spec)
- [x] E19 firmware: sender + receiver — complete
- [x] Sample JPEG as C byte array (test_image.h)
- [x] Airtime + overhead hardware measurement (E19)
- [ ] **B1** Reframe thesis: stride-based, not true DC coefficient (#58)
- [x] **B2** E19-PSNR: Python PSNR_attack measurement (#59) — `tools/psnr_attack_eval.py`
- [ ] **B3** Correct airtime claim → CPU savings (#60) — done in this doc
- [ ] **R1** E20 firmware: FC-HMAC verifiability under 10–20% loss (#61)
- [x] **R2** Tamarin model: fc_hmac.spthy F1-F4 ALL PROVED (#62 closed)
- [ ] **R3** Related work comparison table in LaTeX (#63)
- [ ] E21 firmware: CPU/energy tradeoff (ENC_NONE/DC_ONLY/FULL)
- [ ] Paper LaTeX draft (IEEE IoT-J format)
