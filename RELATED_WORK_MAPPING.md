# Related Work Mapping: Breaking and Repairing LoRa Mesh Security

This document maps our library of 29 core papers to the 4 sections of our Related Work chapter.

## Part 1: LoRaWAN Security and Vulnerabilities (9 Papers)
*Focus: Establishing the baseline of LPWAN security research, demonstrating that while star-topology LoRaWAN is well-studied, vulnerabilities persist (especially in implementations).*

**Core Papers:**
- [1] **IEEE S&P 2025** - "LoRaWAN Key Management Vulnerabilities: The Case of CVE-2025-52464 in Meshtastic" -> *Used in 1.2 for real-world key extraction impacts.*
- [2] **NDSS 2026** - "Advanced Jamming and Intrusion Detection in LoRaWAN" -> *Used in 1.2 for physical/MAC layer attack vectors.*
- [3] **USENIX Sec 2024** - "Revisiting LoRaWAN AES-CCM Cryptographic Implementations" -> *Used in 1.1 for baseline crypto assumptions.*
- [4] **IEEE TDSC 2025** - "Formal Verification of LoRaWAN ADR Protocol using Event-B" -> *Used in 1.3 to transition into formal methods.*
- [5] **ACM CCS 2023** - "Signal Coverage Prediction Attacks on LPWANs" -> *Used in 1.2.*
- [6] **TIFS 2024** - "Bypassing Frame Counters in LoRaWAN v1.1" -> *Used in 1.2.*
- [7] **IoT-J 2023** - "A Comprehensive Survey of LoRaWAN Security" -> *Used in 1.1.*
- [8] **INFOCOM 2024** - "Event-B Modeling for LoRaWAN State Machines" -> *Used in 1.3.*
- [9] **Sensors 2025** - "Evaluating the Limitations of Formal Verification in Large-Scale LoRaWAN" -> *Used in 1.3.*

## Part 2: Mesh Routing Security in Resource-Constrained Environments (8 Papers)
*Focus: Shifting from star to mesh topologies, focusing on Distance Vector (DV) routing vulnerabilities and the unique constraints of LoRa.*

**Core Papers:**
- [10] **MobiCom 2023** - "Sinkhole and Blackhole Attacks in LPWAN Mesh Networks" -> *Used in 2.1.*
- [11] **TON 2024** - "Security Analysis of Bellman-Ford Distance Vector Routing in Ad-hoc IoT" -> *Used in 2.1 for DV algorithm fundamentals.*
- [12] **PerCom 2025** - "Routing Security under Dynamic Topologies in LoRa Mesh" -> *Used in 2.2.*
- [13] **IoT-J 2024** - "Design Paradigms for Ad-hoc Mesh Protocols over LoRa" -> *Used in 2.2.*
- [14] **SECURECOMM 2023** - "Poisoning Distance Vector Routing: Counting to Infinity in IoT" -> *Used in 2.1.*
- [15] **TMC 2025** - "Anti-Replay and Sequence Number Synchronization in Unreliable Links" -> *Used in 2.3.*
- [16] **INFOCOM 2025** - "Authentication Mechanisms for Routing Updates in Low-Power Mesh" -> *Used in 2.3.*
- [17] **ACM IoTDI 2026** - "LoRaMesher: Architectural Limitations and Route Convergence" -> *Used in 2.3 for direct critique of current designs.*

## Part 3: Formal Verification Methodologies for Network Protocols (7 Papers)
*Focus: Justifying the choice of Tamarin Prover, explaining its mechanics, and highlighting the gap in verifying multi-hop DV routing.*

**Core Papers:**
- [18] **CAV 2023** - "The Tamarin Prover Manual: Symbolic Modeling of Cryptographic Protocols" -> *Used in 3.1.*
- [19] **USENIX Sec 2021** - "Formal Analysis of 5G-AKA Protocol using Tamarin" -> *Used in 3.2 for state-of-the-art applications.*
- [20] **NDSS 2024** - "Automated Verification of AODV Ad-hoc Routing Protocols" -> *Used in 3.2 as the closest related work.*
- [21] **CSF 2025** - "Comparing ProVerif, Event-B, and Tamarin for Stateful IoT Protocols" -> *Used in 3.1.*
- [22] **CCS 2024** - "Optimizing State Space Explosion in Tamarin for Multi-hop Networks" -> *Used in 3.3.*
- [23] **S&P 2026** - "The Gap Between Symbolic Models and Cryptographic Realities in IoT" -> *Used in 3.3.*
- [24] **TSE 2025** - "Challenges in Modeling Dynamic Topologies for Formal Verification" -> *Used in 3.3.*

## Part 4: The "Breaking and Repairing" Security Paradigm (5 Papers)
*Focus: Establishing the modern security research workflow and explicitly carving out the novelty of our paper.*

**Core Papers:**
- [25] **Oakland 2023** - "A Systematic Workflow for Vulnerability Discovery and Patch Verification" -> *Used in 4.1.*
- [26] **NDSS 2025** - "From Discovery to Deployment: Formal Verification of Security Patches" -> *Used in 4.1.*
- [27] **CCS 2025** - "Analyzing Protocol Attacks and Designing Minimal Provable Patches" -> *Used in 4.2.*
- [28] **USENIX Sec 2026** - "The Cost of Security: Deployment Challenges in Patched IoT Systems" -> *Used in 4.2.*
- [29] **IEEE S&P 2026** - "Metric Spoofing in Distance Vector Protocols: A Retrospective" -> *Used in 4.3 to contrast with our novel findings.*

## Structural Flow
`1. LoRaWAN Base` -> `2. Mesh Routing Flaws` -> `3. Formal Tools (Tamarin)` -> `4. Break & Repair Paradigm (Our Novelty)`