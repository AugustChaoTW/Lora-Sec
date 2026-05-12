#!/usr/bin/env python3
"""
parse_e7.py — Analyze E7 Black Hole experiment log
Usage: python3 parse_e7.py results/e7_combined_<timestamp>.log
Output: PDR table, routing events, attack success verdict
"""

import re
import sys
from collections import defaultdict

def parse_log(path):
    nodes = {
        "0xAA": {"sent": 0, "acked": 0, "drops_hmac": 0,
                 "route_events": [], "routed_to_attacker": 0},
        "0x11": {"fwd": 0, "drops_hmac": 0, "fwd_blackhole": 0, "route_events": []},
        "0x22": {"fwd": 0, "drops_hmac": 0, "fwd_blackhole": 0, "route_events": []},
        "0x33": {"fwd": 0, "drops_hmac": 0, "fwd_blackhole": 0, "route_events": []},
        "0xBB": {"rx": 0, "hellos_sent": 0},
        "0xCC": {"hellos_sent": 0, "drops": 0},
    }

    node_prefix = {
        "[A(0xAA)]": "0xAA",
        "[R1(0x11)]": "0x11",
        "[R2(0x22)]": "0x22",
        "[R3(0x33)]": "0x33",
        "[B(0xBB)]": "0xBB",
        "[C(0xCC)]": "0xCC",
    }

    with open(path) as f:
        for line in f:
            # Determine which node emitted this line
            nid = None
            for prefix, n in node_prefix.items():
                if prefix in line:
                    nid = n; break
            if nid is None:
                continue

            n = nodes[nid]

            if nid == "0xAA":
                if "[TX] DATA" in line:
                    n["sent"] += 1
                    if "0xCC" in line:
                        n["routed_to_attacker"] += 1
                elif "[RX] ACK" in line:
                    n["acked"] += 1
                elif "HMAC FAILED" in line:
                    n["drops_hmac"] += 1
                elif "[ROUTE]" in line:
                    m = re.search(r"NH=(0x\w+)\s+metric=(\d+)", line)
                    if m:
                        n["route_events"].append((m.group(1), int(m.group(2))))

            elif nid in ("0x11", "0x22", "0x33"):
                if "[FWD]" in line or "[FWD→" in line:
                    n["fwd"] += 1
                    if "BLACKHOLE" in line or "0xCC" in line:
                        n["fwd_blackhole"] += 1
                elif "HMAC FAILED" in line:
                    n["drops_hmac"] += 1
                elif "[ROUTE]" in line:
                    m = re.search(r"NH=(0x\w+)\s+metric=(\d+)", line)
                    if m:
                        n["route_events"].append((m.group(1), int(m.group(2))))

            elif nid == "0xBB":
                if "[RX] DATA" in line:
                    n["rx"] += 1
                elif "[TX] Hello" in line:
                    n["hellos_sent"] += 1

            elif nid == "0xCC":
                if "[BLACKHOLE] Hello" in line or "Hello sent" in line:
                    n["hellos_sent"] += 1
                elif "[BLACKHOLE] DROP" in line:
                    n["drops"] += 1

    return nodes


def print_report(nodes):
    print("=" * 56)
    print(" E7 Black Hole Demo — Results Report")
    print("=" * 56)
    print()

    # --- PDR ---
    a = nodes["0xAA"]
    sent = a["sent"]; acked = a["acked"]
    pdr = acked / sent * 100 if sent > 0 else 0
    to_atk = a["routed_to_attacker"]
    print(f"Node A (Sender 0xAA)")
    print(f"  Packets sent          : {sent}")
    print(f"  ACKs received         : {acked}")
    print(f"  PDR                   : {pdr:.1f}%")
    print(f"  Pkts routed to 0xCC   : {to_atk}  ({to_atk/sent*100:.0f}% of sent)" if sent else "  Pkts routed to 0xCC   : 0")
    print(f"  HMAC drops            : {a['drops_hmac']}")
    print()

    # --- Relays ---
    for nid, name in [("0x11","R1"), ("0x22","R2"), ("0x33","R3")]:
        r = nodes[nid]
        print(f"Relay {name} ({nid})")
        print(f"  Forwarded total       : {r['fwd']}")
        print(f"  Forwarded → 0xCC      : {r['fwd_blackhole']}")
        print(f"  HMAC drops            : {r['drops_hmac']}")
        if r["route_events"]:
            poisoned = [e for e in r["route_events"] if e[0] == "0xCC"]
            print(f"  Route events          : {len(r['route_events'])}  ({len(poisoned)} poisoned to 0xCC)")
        print()

    # --- Gateway ---
    b = nodes["0xBB"]
    print(f"Node B (Gateway 0xBB)")
    print(f"  Hellos sent           : {b['hellos_sent']}")
    print(f"  Packets received      : {b['rx']}")
    if b["hellos_sent"] > 3 and b["rx"] == 0:
        print(f"  → PDR=0%  Black Hole confirmed from gateway perspective")
    print()

    # --- Attacker ---
    c = nodes["0xCC"]
    print(f"Node C (Attacker 0xCC — Black Hole)")
    print(f"  Signed Hellos sent    : {c['hellos_sent']}")
    print(f"  Packets dropped       : {c['drops']}")
    print()

    # --- Verdict ---
    print("-" * 56)
    total_hmac_fails = sum(
        nodes[n]["drops_hmac"] for n in ("0xAA","0x11","0x22","0x33")
    )
    attack_success = (c["drops"] > 0 and pdr < 20)
    hmac_blocked   = (total_hmac_fails > 0 and c["drops"] == 0)

    if attack_success and total_hmac_fails == 0:
        verdict = "BLACK HOLE SUCCESS — HMAC patch completely bypassed"
        detail  = "Insider held valid PSK. sign_hello() produced valid HMAC.\nAll patched nodes accepted route to 0xCC. Authentication ≠ Availability."
    elif attack_success and total_hmac_fails > 0:
        verdict = "PARTIAL — some nodes rejected, but PDR still low"
        detail  = f"{total_hmac_fails} HMAC drops detected alongside {c['drops']} black-hole drops."
    elif hmac_blocked:
        verdict = "ATTACK BLOCKED — HMAC rejected all attacker Hellos"
        detail  = "This would be E6 behavior, not E7. Check attacker firmware uses sign_hello()."
    else:
        verdict = "INCONCLUSIVE — check log manually"
        detail  = f"PDR={pdr:.1f}%  drops={c['drops']}  hmac_fails={total_hmac_fails}"

    print(f"VERDICT: {verdict}")
    print()
    for line in detail.split("\n"):
        print(f"  {line}")
    print("-" * 56)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: python3 {sys.argv[0]} <combined_log>")
        sys.exit(1)
    nodes = parse_log(sys.argv[1])
    print_report(nodes)
