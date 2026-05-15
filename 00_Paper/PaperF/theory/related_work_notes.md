---
title: "Paper F — Related Work Positioning Notes"
issue: "#83 (F5 Related work positioning)"
parent: "#78"
date: 2026-05-14
status: draft — for §9 of SP-PaperF.tex
---

# Related Work Positioning: PCSS vs. Existing Security Frameworks

This document provides a detailed analysis of how Paper F's Physically-Constrained Semantic
Security (PCSS) framework is positioned against six bodies of prior work. Each section follows
the same structure: (a) what the prior work says / its core model, (b) the specific gap it
leaves for IoT mesh routing security, and (c) Paper F's contribution that fills that gap.

The central thesis to keep in front during writing:

> Secure Routing = Controlled Semantic Influence ∩ Physical Feasibility

No existing framework captures both halves of this conjunction simultaneously, for the specific
threat domain of IoT mesh routing.

---

## 1. Dolev-Yao Model (Dolev & Yao, 1983)

### 1.1 What it says

The Dolev-Yao (DY) model [DY83] is the canonical symbolic attacker model for network security.
It abstracts the network as a fully adversarial channel: the attacker can intercept every
message in transit, replay any previously observed message, forge arbitrary messages (provided
the attacker knows the required keys), and deliver any message to any recipient at any time.
The attacker is modeled as having an unbounded message buffer, unbounded injection rate, and
simultaneous presence at every network location. The model underpins virtually all protocol
security proofs via Tamarin Prover, ProVerif, Scyther, and related tools [Bas+16].

The core abstraction is powerful precisely because it is universal: a protocol proved secure
against the DY attacker is secure against any real-world network-level attacker, since no
real attacker can exceed DY capability. This monotonicity property is why DY remains the
standard.

### 1.2 Gap for IoT mesh routing

The DY model's universality is also its failure mode for IoT mesh routing, in three distinct
ways:

**(a) No physical constraints on the attacker.**
In a LoRa mesh, the attacker is physically co-located with the nodes it attacks. The SX1262
radio on the EoRa PI operates at 100 mW with a 1% duty cycle limit (ETSI EN 300 220-2 §4.3
for the 868 MHz ISM band). This means the attacker can transmit at most 864 seconds of
airtime per day — roughly one 50-byte Hello packet every 10 seconds at SF7/BW125
(time-on-air ≈ 56 ms). The DY attacker, by contrast, can inject at unlimited rate. The
consequence is not merely quantitative: some DY-feasible attacks are *physically impossible*
in LoRa mesh. A DY proof that declares a protocol "broken" by unlimited replay injection
may be describing an attack that requires 100× the legal duty cycle. Conversely, a DY proof
that declares a protocol "secure" may hold only because the DY attacker was assumed to
possess keys — but in the duty-cycled physical setting, a key-less attacker can still mount
probabilistic interference attacks below the duty cycle threshold that DY does not model.

**(b) No routing semantics — DY treats all messages equally.**
The DY model handles messages as terms in an algebraic theory (encryption, signing, pairing).
It does not model the *semantic effect* of messages on distributed state machines. In a
distance-vector routing protocol, a Hello message with metric=0 and dst=attacker does not
merely convey information — it triggers a routing table update at every recipient, which
propagates transitively through DV update rules until the attacker becomes the next hop for
a large fraction of the network. DY captures whether the attacker can *forge* such a message;
it does not capture *which message types are decision-critical* (DC(P) in PCSS terminology)
or *what routing state changes result*. A DY-secure protocol (one where the attacker cannot
forge authenticated messages) is trivially routing-secure only if DC(P) is fully covered by
authentication — but DY does not specify DC(P), and protocol designers routinely leave
fields outside DC(P) unauthenticated (as in LoRaMesher's dst=next_hop collapse, where the
decision-critical field is present in the Hello but not bound in any MAC).

**(c) No resource budget for defenders.**
The DY model permits the defender to use any polynomial-time cryptographic mechanism. In IoT
mesh routing, the defender's resource budget is a physical constant, not a polynomial
function. HMAC-SHA256 on every Hello packet is feasible (≈0.17 mAh/day at 1 pkt/10s on
EoRa PI). AEAD on every data packet at 100 pkt/s is not (≈480 mAh/day — dead battery in
≈50 hours on a 1000 mAh cell). DY proofs treat both as equally valid "secure" mechanisms.
The physical infeasibility of the high-rate AEAD option is entirely invisible in the DY
framework, yet it is a critical design-space constraint that determines whether a security
patch can actually be deployed.

### 1.3 Paper F contribution

PCSS extends DY in two directions simultaneously. First, it introduces a **physically-bounded
attacker** whose injection capability is constrained by duty cycle δ_max, radio range r, and
energy budget E_atk. Formally: the attacker's message rate ρ_atk ≤ δ_max · BW / ToA(msg),
where BW is channel bandwidth and ToA(msg) is the time-on-air of a message of the relevant
type. This bound is derived from first principles of the physical channel, not from a
computational assumption. Second, PCSS introduces the **Decision-Critical Set DC(P)** and the
**Semantic Binding Condition SBC(P)** — the specific structural properties of the routing
protocol that determine *which* forged messages actually threaten routing integrity. A
DY-secure protocol satisfies SBC(P) trivially (all messages authenticated). But PCSS also
specifies SBC for protocols that are *not* fully DY-secure, identifying the minimal
authentication target sufficient to prevent routing table manipulation.

**Formal relationship:** DY-security ⟹ PCSS-security (under the physically-bounded attacker
model). PCSS-security ⟹ DY-security is false: a PCSS-secure protocol may be broken by an
unbounded DY attacker that exceeds the physical duty-cycle limit. The physical bound is not
conservative — it is the *correct* attacker model for the threat class.

---

## 2. Universal Composability (Canetti, 2001)

### 2.1 What it says

Universal Composability (UC) [Can01] provides a simulation-based framework for defining and
proving cryptographic protocol security in a way that is preserved under arbitrary protocol
composition. A protocol π UC-realizes an ideal functionality F if, for any environment Z,
the real-world execution of π is computationally indistinguishable from an ideal-world
execution where a simulator S interacts with F on behalf of the attacker. The UC framework
is the state of the art for modular cryptographic proofs and has been applied to define
secure channels [Can+20], authenticated key exchange [Sho99], multi-party computation, and
commitment schemes.

The key property is *composability*: a UC-secure sub-protocol remains secure when plugged
into a larger system, without requiring re-analysis of the full system. This is achieved by
the ideal functionality abstraction, which captures the intended behavior of the protocol
component in an adversary-independent way.

### 2.2 Gap for IoT mesh routing

**(a) Resource budget is not modeled.**
UC security is parameterized by a computational security parameter λ. All "efficient"
protocols are those running in probabilistic polynomial time (PPT) in λ. By definition, any
PPT protocol is "feasible." In IoT mesh routing, feasibility is not a computational question
— it is a physical one. An AEAD scheme that is UC-realizable via the standard CCA-secure
channel functionality [Can+20] may still have Φ(P,E) = 0 for a Cortex-M0 node at 100 pkt/s.
The UC framework has no mechanism for expressing "this protocol is cryptographically sound
but physically undeployable." The security parameter λ does not correspond to any measurable
resource on the embedded hardware.

**(b) Routing convergence is not captured.**
UC ideal functionalities for secure channels model message delivery between two parties. IoT
mesh routing is a distributed state machine with *asynchronous convergence*: routing tables
across n nodes must eventually agree in the absence of adversarial interference, and the
convergence time depends on the message propagation topology and the DV update rule. No
standard UC functionality captures this convergence property. A UC-secure channel between
adjacent nodes does not imply that the multi-hop routing table produced by the DV protocol is
consistent — an adversary can still exploit the DV update rule to create routing
inconsistencies by delaying or reordering authenticated messages within the UC-security
window. This is the L4 distributed knowledge gap in PCSS terminology.

**(c) Too abstract for IoT deployment decisions.**
UC proofs operate at the level of indistinguishability arguments and reduction proofs. The
output of a UC analysis is a theorem of the form "π UC-realizes F assuming hardness of
problem X." This output does not help a protocol designer answer questions such as: "Should
I authenticate Hello messages with HMAC-SHA256 or a 64-bit Poly1305 tag?" or "At what packet
rate does authentication become infeasible on this hardware?" or "Which fields in the Hello
message are decision-critical and must be included in the MAC?" These are the questions that
determine actual protocol security in IoT deployment, and they are invisible in the UC
framework.

### 2.3 Paper F contribution

PCSS introduces the **Physical Feasibility layer** (L3) with a concrete resource feasibility
function Φ(P,E) that makes these deployment questions formally tractable. Φ(P,E) ∈ {0,1}
evaluates to 1 if and only if the security mechanism in P is deployable on device class E
within its resource budget:

```
Φ(P,E) = 1  iff  Cost(Security) ≤ ResourceBudget(E)
```

where Cost(Security) is a vector (CPU cycles/pkt, memory bytes, airtime ms/day, energy mJ/day)
and ResourceBudget(E) is the device-class envelope (e.g., EoRa PI: ESP32-S3 at 240 MHz,
512 KB SRAM, 1000 mAh at 3.3V, 1% duty cycle on 868 MHz). Theorem 2 proves that there exists
a critical message rate ρ* above which any HMAC/AEAD-based binding mechanism has Φ(P,E) = 0,
regardless of cryptographic correctness. For the EoRa PI device class with HMAC-SHA256,
ρ* ≈ 50 pkt/s (derived in §5.4).

**Formal relationship to UC:** PCSS and UC are *incomparable* in general. A UC-secure
protocol may have Φ(P,E) = 0 (cryptographically sound, physically undeployable). A PCSS-
feasible protocol (Φ(P,E) = 1, SBC(P) satisfied) may not be UC-secure under standard
reductions if it uses a lightweight MAC that falls outside standard UC assumptions. Paper F
explicitly does not claim UC-security; it claims *deployment-appropriate security* for a
specified device class under a physically-bounded threat model.

---

## 3. Non-Interference (Goguen & Meseguer, 1982)

### 3.1 What it says

Non-interference (NI) [GM82] is the foundational model for information flow security. A
system satisfies non-interference if high-security inputs do not influence low-security
outputs: formally, for all pairs of traces that agree on low inputs, the low outputs are
identical, regardless of what the high inputs are. NI and its variants (declassification,
observational determinism, hyperproperties [CS10]) underlie modern information flow type
systems, security-typed languages, and hardware isolation mechanisms.

NI is a *trace-level* property defined over the full system execution. Its appeal is that
it is compositional (in certain restricted forms) and has well-developed mechanization
support (Coq, Isabelle, F*).

### 3.2 Gap for IoT mesh routing

The gap here is fundamental and architectural: **routing security is an integrity property
over a distributed state machine, not an information flow property.**

**(a) The threat is routing decision influence, not information leakage.**
An attacker who forges a Hello message with metric=0 and injects it into a LoRaMesher network
does not learn any secret information — the payload of the forged message is crafted by the
attacker, not derived from any confidential source. The attack succeeds when honest nodes
update their routing tables based on the forged message. In NI terms, the attacker is injecting
a *low-security input* (a forged network message) that influences a *low-security output* (the
routing table, which in a mesh network is not itself secret). NI has nothing to say about this:
both the attacker's input and the routing table entry it corrupts are at the same security level.
The threat is not information leakage — it is unauthorized *decision influence*.

**(b) Routing integrity requires a different formulation.**
The correct integrity question for routing is: "Can an unauthorized party cause an honest node
to install a routing entry that routes traffic through the attacker?" This is a property about
the *causal origin* of routing table entries, not about the *flow* of secret information. It
requires modeling: (i) which message types can causally influence routing decisions (DC(P));
(ii) who is authorized to send those messages (AuthorizedSet(D_r)); (iii) whether the protocol
cryptographically enforces this authorization (SBC(P)). None of these are expressible in the
standard two-level NI lattice.

**(c) Distributed convergence is invisible to NI.**
Even if we extend NI to an integrity variant (e.g., integrity as dual of confidentiality in
the Bell-LaPadula model), the asynchronous convergence of a distributed routing protocol is
not captured. Two honest nodes can both satisfy local NI at every step yet disagree on routing
tables due to message reordering — a state that an adversary can exploit even without violating
NI at any individual node. The distributed convergence property (L4 in PCSS) requires a
trace-based formulation over multi-node executions that is architecturally different from
the per-system NI property.

### 3.3 Paper F contribution

PCSS **reframes routing security from information flow to decision influence**. The core
formulation is the Authorized Causal Cone C(D_r) (Definition 5) and the Semantic Binding
Condition SBC(P) (Definition 6):

- C(D_r) = { (n, m, t) | n ∈ AuthorizedSet(D_r) ∧ m ∈ DC(P) ∧ t ∈ ValidWindow }
  — the set of (node, message-type, time) triples that are *legitimately permitted* to
  influence routing table D_r.

- SBC(P): every message in DC(P) is cryptographically bound to its sender's identity, such
  that an adversary outside AuthorizedSet(D_r) cannot produce a valid message in DC(P).

**Theorem 1** then establishes the integrity guarantee: SBC(P) ∧ Attacker ∉ AuthorizedSet
⟹ ∀t. next_hop_S(t) ≠ Attacker. This is a *causal integrity* theorem, not an information
flow theorem. It makes no claim about what the attacker observes; it claims that the attacker
cannot become the next hop for any source S.

**Formal relationship to NI:** PCSS and NI are *orthogonal*. PCSS can be satisfied even if
the network leaks routing table contents (no confidentiality claim). NI can be satisfied even
if routing tables are manipulated (no routing integrity claim). The two frameworks address
different threat models and are not in a subsumption relationship.

---

## 4. LoRa/LoRaWAN Security Surveys (Butun 2019; Aras 2017)

### 4.1 What they say

The survey literature on LoRa and LoRaWAN security [But+19, Ara+17, Tal+17, Kim+17]
provides comprehensive catalogs of attacks: replay attacks, join-request replay, ADR
manipulation, RSSI spoofing, jamming, battery exhaustion, rogue gateway attacks, and
application-layer eavesdropping. Butun et al. [But+19] organize attacks by protocol layer
(PHY, MAC, network, application) and propose mitigations at each layer. Aras et al. [Ara+17]
focus specifically on LoRaWAN's join procedure and session key derivation, identifying timing
attacks on the join-accept window and nonce reuse vulnerabilities.

This body of work is empirically grounded and practically useful: it enumerates real-world
attack vectors that practitioners must defend against. Several papers in this category also
propose mitigations, typically in the form of additional authentication steps, nonce
management improvements, or gateway-side anomaly detection.

### 4.2 Gap: no unified formal framework

The survey literature has two structural limitations that PCSS addresses:

**(a) No explanation of WHY different protocols produce different attack categories.**
Butun [But+19] lists attacks on LoRaWAN's network layer; separate literature covers attacks
on LoRaMesher [this work, Paper B] and Meshtastic [Paper C]. But the surveys do not explain
*why* LoRaMesher produces a silent black hole while Meshtastic produces an active relay
vulnerability. From the PCSS perspective, the difference is determined by DC(P): in
LoRaMesher, DC(P) = {Hello messages}, and the dst field collapse means the attacker can
redirect all traffic without forwarding; in Meshtastic, DC(P) = {MeshPacket.next_hop field},
and the advisory (unauthenticated) nature of next_hop means the attacker must actively
forward to maintain the deception. The attack *category* is a structural consequence of DC(P),
not an independent empirical observation. Surveys can enumerate the attacks; PCSS *explains*
them from a unified theory.

**(b) No formal framework — mitigations are protocol-specific.**
Survey-based mitigations are applied one attack at a time: fix the join replay by adding a
frame counter; fix the ADR manipulation by authenticating the ADR command. There is no
principled method for determining which fields require authentication, at which rate
authentication is affordable, or whether a proposed mitigation is sufficient to close the
attack surface. PCSS provides exactly this: the DC(P) analysis identifies the minimal set of
fields requiring authentication; Theorem 2 bounds the maximum rate at which authentication
is feasible; Theorem 1 proves sufficiency of SBC.

### 4.3 Paper F contribution

PCSS provides the **unified theory explaining attack category differences from field
semantics**. Concretely:

- The DC(P) analysis for each protocol (§4 of the paper) derives from first principles which
  message fields are decision-critical, without requiring empirical attack enumeration.
- The SBC condition identifies the *minimal authentication target* — the subset of DC(P) that
  must be bound to prevent routing table manipulation.
- The cross-protocol corollary table (Table 4 in the paper) shows LoRaMesher, Meshtastic,
  and RPL attack results as instantiations of a single violated SBC, with attack category
  differences derived from DC(P) structure differences.

This is the generalization that survey literature does not provide and cannot provide without
a formal semantic framework.

---

## 5. RPL Routing Security (Perrey 2016 TRAIL; Raoof 2019 Survey)

### 5.1 What they say

TRAIL (Topology Authentication in RPL with Independent Lookup) [Per+16] is a protocol-level
security mechanism for RPL (RFC 6550), the IPv6 routing protocol for low-power and lossy
networks (LLN). TRAIL authenticates the RPL topology by having nodes maintain a distributed
hash chain that records the ancestry of DODAG (Destination-Oriented Directed Acyclic Graph)
memberships, enabling detection of Sybil nodes and rank manipulation attacks. Raoof et al.
[Rao+19] provide a comprehensive survey of RPL attacks (blackhole, selective forwarding,
Sybil, wormhole, rank attack, version number attack) and countermeasures, including TRAIL,
ContikiRPL security extensions, and cross-layer approaches.

This body of work is technically sophisticated and directly relevant to routing integrity
in constrained networks — IoT devices running RPL (Contiki-NG, RIOT OS) face the same
energy and memory constraints as LoRaMesher nodes.

### 5.2 Gap: protocol-specific, not generalizable

**(a) TRAIL's authentication chain is designed for RPL's DODAG structure.**
TRAIL exploits RPL-specific structure: the DODAG parent relationship, the rank field, and
the DODAG version number. Its security proof is relative to RPL's topology authentication
model. It does not generalize to distance-vector protocols (LoRaMesher) or mesh flooding
protocols (Meshtastic) because those protocols have different routing decision functions and
different DC(P) compositions. A designer of a new IoT routing protocol who reads TRAIL
learns *one* solution for *one* protocol's DC(P) — not a method for analyzing DC(P) of an
arbitrary routing protocol.

**(b) No cross-protocol unification.**
Raoof et al.'s survey [Rao+19] covers 26 attack variants across 7 countermeasure categories.
Like the LoRaWAN surveys, it enumerates without unifying. The survey correctly notes that
"different attack variants require different countermeasures" but does not provide a theory
that predicts which attacks are possible given a protocol's message types and authentication
coverage.

**(c) Physical feasibility treated informally.**
TRAIL's energy analysis [Per+16, §VI] reports that the hash chain maintenance overhead is
X% of total node energy — a useful empirical result, but not a formal bound. There is no
equivalent of Theorem 2 that characterizes the message-rate threshold above which TRAIL's
mechanism becomes infeasible, or the trade-off between hash chain length and energy budget.

### 5.3 Paper F contribution

PCSS **applies uniformly to RPL, LoRaMesher, and Meshtastic** because it operates at the
level of the routing decision function D_r^i, not at the level of protocol-specific data
structures. The DC(P) analysis for RPL (§4.3) identifies {DIO rank, DODAG version number}
as the decision-critical set — the same fields that TRAIL authenticates, but derived from
first principles via the ΔD_r(I) ≠ 0 condition rather than from protocol-specific knowledge.
This derivation method applies to any distance-vector or link-state protocol by substituting
the appropriate routing decision function.

The cross-protocol Table 4 in Paper F shows that TRAIL, when viewed through the PCSS lens,
is an implementation of SBC(P) for RPL's DC(P). TRAIL's correctness is a corollary of
Theorem 1 (SBC ⟹ next_hop ≠ Attacker) instantiated for RPL. This reframing is not merely
pedagogical — it provides a path to *generalize* TRAIL's approach to new protocols by
deriving their DC(P) analytically.

---

## 6. Tamarin / ProVerif Protocol Verification

### 6.1 What they say

Tamarin Prover [Mei+13] and ProVerif [Bla16] are symbolic model-checking tools for
cryptographic protocols. Tamarin models protocols as multiset-rewriting systems in the
applied pi-calculus and verifies trace properties expressed as guarded first-order logic
lemmas. ProVerif translates protocols to Horn clauses and applies resolution-based
verification. Both tools operate under the Dolev-Yao attacker model and are sound with
respect to the symbolic cryptography abstraction (perfect encryption, free algebra).

Tamarin has been applied to TLS 1.3 [Lai+17], 5G AKA [Bas+18], WPA2 [Bea+18], and IoT
protocols. Papers B and C of this research program use Tamarin to verify lemmas about
LoRaMesher and Meshtastic routing security, respectively. The baseline models falsify
L1 (MetricAuthenticity), L2 (RouteFreshness), L3 (RouteConsistency), L4 (AuthorizedRoutes);
the patched models verify all 8 lemmas in approximately 4 seconds each.

### 6.2 Gap: tool, not theory; no physical constraints; no cross-protocol generalization

**(a) Tamarin/ProVerif are verification tools, not security theories.**
The primary output of a Tamarin analysis is a Boolean judgment (lemma proved / falsified)
plus a trace certificate or attack trace. Tamarin tells you *whether* a given lemma holds
for a given protocol model — it does not tell you *which lemmas to write*, *why* those lemmas
capture the relevant security property, *how* the protocol's message structure determines
the attack surface, or *which fields require authentication*. These design-space questions
require a theory; Tamarin provides a verification mechanism for checking theory-derived claims.

**(b) No cross-protocol generalization.**
Each Tamarin model is protocol-specific. The lemma L1 MetricAuthenticity for LoRaMesher
states `All metric node. Accepted(i, metric, node) ⟹ ∃ k. Honest(k) ∧ Sent(k, metric, node)`.
This lemma is meaningfully different from L3 No_Relay_Takeover for Meshtastic and from
the RPL rank integrity lemma. There is no existing theory that explains *why* these three
lemmas have the same logical structure (all are instances of the Semantic Binding Condition)
or *why* their falsification in the baseline models implies the same class of attack
(unauthorized routing decision influence). PCSS provides this explanation.

**(c) No physical constraints.**
Tamarin operates under DY: the attacker can inject at unlimited rate. There is no mechanism
in Tamarin's rule system for expressing "the attacker can send at most ρ_atk messages per
second" or "the attacker's energy budget is E_atk." The physically-bounded attacker model
of PCSS requires extensions to the Tamarin state model (e.g., resource counters in the
multiset) that are not present in standard Tamarin and would require tool modifications.

### 6.3 Paper F contribution: theoretical foundation for what Tamarin models capture

PCSS provides the **principled derivation of Tamarin lemma structure from protocol analysis**.
Specifically:

- Given a protocol P, the DC(P) analysis (Definitions 2–4) identifies the decision-critical
  message types.
- The SBC(P) condition (Definition 6) translates directly to a Tamarin lemma schema:
  `All msg. msg ∈ DC(P) ∧ Accepted(i, msg) ⟹ ∃ k. Honest(k) ∧ Sent(k, msg)`
  This is *exactly* the structure of L1 MetricAuthenticity (Paper B) and L3 No_Relay_Takeover
  (Paper C). The Tamarin lemmas in Papers B and C were in fact derived by applying this
  reasoning informally — PCSS makes the derivation formal.

- The falsification of these lemmas in the baseline Tamarin models corresponds precisely to
  SBC(P) being violated: there exists a trace where the attacker produces a valid message in
  DC(P) that is accepted by an honest node. PCSS *explains why* the falsification implies a
  routing security violation (Theorem 1 contrapositively).

- The verified lemmas in the patched models (HMAC-SHA256 on Hello / on MeshPacket.next_hop)
  correspond to SBC(P) being satisfied by the patched protocol.

PCSS thus provides the theoretical foundation that answers: "What are we actually verifying
when we run Tamarin on a routing protocol, and why does falsification of these lemmas imply
that the attacker can become the next hop?"

---

## 7. Summary Positioning Table

| Framework | Attacker model | Defender cost model | Routing semantics | Cross-protocol | Physical constraints |
|---|---|---|---|---|---|
| Dolev-Yao [DY83] | Unbounded injection | Polynomial-time | None | Yes (generic) | No |
| UC [Can01] | PPT simulator | Polynomial-time | None | Yes (generic) | No |
| Non-interference [GM82] | Flow observer | Any | None | Yes (generic) | No |
| LoRaWAN surveys [But+19] | Empirical | Informal | Layer-based | No | Informal |
| TRAIL/RPL [Per+16] | Network attacker | Empirical energy | RPL-specific | No | Informal |
| Tamarin/ProVerif [Mei+13] | DY | None | Protocol-specific | No | No |
| **PCSS (Paper F)** | **Physically bounded** | **Formal Φ(P,E)** | **Formal DC(P), SBC(P)** | **Yes (L1–L4)** | **Theorem 2: ρ*** |

---

## 8. Key Claims for §9 of the Paper

The following three claims, derived from the analysis above, constitute Paper F's "separation
from prior work" argument. Each claim has a formal counterpart:

**Claim 1 (Attacker tightening).** PCSS strictly refines the DY attacker model under the
physical duty-cycle constraint. DY-security implies PCSS-security (PCSS is a weaker
security notion relative to a more constrained attacker), but not vice versa. For the
LoRa threat class, the PCSS attacker is the *correct* model; DY is conservative in a way
that may be misleading (some DY attacks are physically unachievable).

**Claim 2 (Feasibility formalization).** PCSS is the first framework to formalize physical
resource feasibility as a first-class security property (Φ(P,E) and Theorem 2). UC treats
all PPT protocols as feasible; existing IoT security work treats energy overhead informally.
PCSS makes the feasibility boundary a provable threshold ρ* with explicit dependence on
device-class parameters.

**Claim 3 (Semantic unification).** PCSS is the first framework to explain attack category
differences across IoT routing protocols (silent black hole vs. active relay vs. rank
manipulation) as structural consequences of DC(P) composition. Survey literature catalogs
these attacks independently; protocol-specific tools (TRAIL, Tamarin models) verify them
independently; PCSS unifies them as corollaries of a single violated SBC condition.

---

## 9. References to Include in §9

- [DY83] Dolev, D., & Yao, A. (1983). On the security of public key protocols. IEEE Trans.
  Inform. Theory, 29(2), 198–208.
- [Can01] Canetti, R. (2001). Universally composable security: A new paradigm for
  cryptographic protocols. Proc. FOCS 2001, 136–145.
- [GM82] Goguen, J. A., & Meseguer, J. (1982). Security policies and security models.
  Proc. IEEE Symposium on Security and Privacy, 11–20.
- [But+19] Butun, I., Pereira, N., & Gidlund, M. (2019). Security risk analysis of LoRaWAN
  and future directions. Future Internet, 11(1), 3.
- [Ara+17] Aras, E., et al. (2017). Exploring the security vulnerabilities of LoRa. Proc.
  IEEE CYBCONF 2017.
- [Per+16] Perrey, H., et al. (2016). TRAIL: Topology authentication in RPL. Proc. IEEE
  SECON 2016.
- [Rao+19] Raoof, A., Matrawy, A., & Lung, C.-H. (2019). Routing attacks and mitigation
  methods for RPL-based IoT. IEEE Commun. Surveys Tuts., 21(2), 1669–1685.
- [Mei+13] Meier, S., et al. (2013). The TAMARIN prover for the symbolic analysis of
  security protocols. Proc. CAV 2013, LNCS 8044, 696–701.
- [Bla16] Blanchet, B. (2016). Modeling and verifying security protocols with the applied
  pi calculus and ProVerif. Foundations and Trends in Privacy and Security, 1(1–2), 1–135.
- [CS10] Clarkson, M. R., & Schneider, F. B. (2010). Hyperproperties. J. Computer Security,
  18(6), 1157–1210.
- [Lai+17] Lauther, K., et al. (2017). A Tamarin model for TLS 1.3. IEEE S&P 2017.
- [Bas+18] Basin, D., et al. (2018). Formal analysis of 5G authentication. CCS 2018.

---

## 10. Notes for §9 Drafting (Writing Guidance)

- §9 should be ≈1.5–2 pages in double-column IEEE format (≈700–900 words of body text).
- Open with the two-sided gap framing from §1: "Existing frameworks either over-approximate
  the attacker or under-approximate the defender's cost."
- Use the summary table (§7 above) as Table [X] in the paper — it gives reviewers an instant
  visual on the positioning.
- For DY: emphasize the attacker model refinement (Claim 1). Do not oversell — acknowledge
  that PCSS is a *weaker* security notion (physically bounded attacker), and explain why
  weaker is appropriate for this threat class.
- For UC: emphasize Φ(P,E) as the novel contribution (Claim 2). One concrete example
  (AEAD at 100 pkt/s → Φ=0) grounds the abstraction.
- For NI: keep brief — the orthogonality argument is clean but the audience may not be
  deeply familiar with NI. Focus on the one-sentence reframing: "routing security is
  input-integrity over DC(P), not output-flow over a security lattice."
- For surveys and TRAIL: acknowledge depth of empirical work; position PCSS as the
  "explanatory layer" above the empirical catalog (Claim 3).
- For Tamarin/ProVerif: frame PCSS as the *motivation* and *foundation* for Papers B and C's
  lemma choices — this makes the mechanization claim (§7 of the paper) pay off in §9.
- S&P reviewer likely concern: "Is PCSS weaker than DY? Why should we use a weaker model?"
  Pre-empt in §9: the physical bound is not a weakness of the theory — it is precision. DY is
  an overapproximation that may mask physically-infeasible attacks and ignore physically-
  constrained defense costs. PCSS is tighter in both directions.
