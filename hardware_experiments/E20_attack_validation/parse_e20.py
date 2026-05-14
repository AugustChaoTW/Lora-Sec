#!/usr/bin/env python3
"""
E20 Results Parser
Reads receiver log, extracts per-image metrics, generates results table.

Usage: python3 parse_e20.py <log_dir> <scenario>
"""
import sys
import re
import pathlib
import json

def parse_receiver_log(log_path):
    images = []
    current = None

    for line in pathlib.Path(log_path).read_text().splitlines():
        # [HH:MM:SS][RECEIVER] [NEW_IMG] total_frags=9 img_seq=0
        m = re.search(r'\[NEW_IMG\] total_frags=(\d+)', line)
        if m:
            current = {
                'total_frags': int(m.group(1)),
                'recv': 0, 'verified': 0, 'failed': 0,
                'chain_ok': None, 'chain_broken': None,
                'verify_prefix': 0, 'break_pos': 'none',
                'replay_det': 0,
            }
            seq_m = re.search(r'img_seq=(\d+)', line)
            if seq_m:
                current['img_seq'] = int(seq_m.group(1))

        # [IMG_DONE] seq=0 recv=9/9 verified=9 failed=0 chain_ok=100% chain_broken=NO
        m = re.search(r'\[IMG_DONE\].*recv=(\d+)/(\d+).*verified=(\d+).*failed=(\d+).*chain_ok=([\d.]+)%.*chain_broken=(\w+)', line)
        if m and current:
            current['recv']         = int(m.group(1))
            current['recv_total']   = int(m.group(2))
            current['verified']     = int(m.group(3))
            current['failed']       = int(m.group(4))
            current['chain_ok_pct'] = float(m.group(5))
            current['chain_broken'] = m.group(6) == 'YES'

        # [E20] verify_prefix=9/9 (100%) break_pos=none replay_det=0
        m = re.search(r'\[E20\] verify_prefix=(\d+)/(\d+) \(([\d.]+)%\) break_pos=(\S+) replay_det=(\d+)', line)
        if m and current:
            current['verify_prefix']     = int(m.group(1))
            current['verify_prefix_pct'] = float(m.group(3))
            current['break_pos']         = m.group(4)
            current['replay_det']        = int(m.group(5))
            images.append(dict(current))
            current = None

    return images

def parse_relay_log(log_path):
    stats = {'forwarded': 0, 'dropped': 0, 'flipped': 0, 'replayed': 0}
    for line in pathlib.Path(log_path).read_text().splitlines():
        if '[FWD]' in line:     stats['forwarded'] += 1
        if '[DROP]' in line:    stats['dropped'] += 1
        if '[FLIP]' in line:    stats['flipped'] += 1
        if '[REPLAY]' in line and 'sent' in line: stats['replayed'] += 1
    return stats

def main():
    if len(sys.argv) < 3:
        print("Usage: parse_e20.py <log_dir> <scenario>")
        sys.exit(1)

    log_dir  = pathlib.Path(sys.argv[1])
    scenario = sys.argv[2]

    rx_log    = log_dir / "receiver.log"
    relay_log = log_dir / "relay.log"

    if not rx_log.exists():
        print(f"[ERR] {rx_log} not found")
        sys.exit(1)

    images = parse_receiver_log(rx_log)
    relay  = parse_relay_log(relay_log) if relay_log.exists() else {}

    print(f"\n{'='*65}")
    print(f"E20 Results: {scenario}")
    print(f"Log dir: {log_dir}")
    print(f"{'='*65}")
    print(f"Images processed: {len(images)}")
    if relay:
        print(f"Relay — fwd:{relay['forwarded']} drop:{relay['dropped']} "
              f"flip:{relay['flipped']} replay:{relay['replayed']}")
    print()

    if not images:
        print("[WARN] No [IMG_DONE] events found in receiver log")
        return

    # Per-image table
    print(f"{'Img':>4} {'Recv':>5} {'Verified':>9} {'Failed':>7} "
          f"{'ChainOk%':>9} {'Prefix':>7} {'BrokenAt':>9} {'Replay':>7}")
    print(f"{'-'*65}")
    for i, img in enumerate(images):
        brk = img.get('chain_broken', False)
        pfx = img.get('verify_prefix', img['verified'])
        pfx_pct = img.get('verify_prefix_pct', 100 if not brk else 0)
        bp  = img.get('break_pos', 'none')
        rp  = img.get('replay_det', 0)
        print(f"{i:>4} {img['recv']:>5}/{img.get('recv_total', img['recv']):<2} "
              f"{img['verified']:>6} {img['failed']:>7} "
              f"{img.get('chain_ok_pct', 100):>8.0f}% {pfx:>4}/{img['total_frags']:<2} "
              f"{'frag'+bp if bp!='none' else 'none':>9} {rp:>7}")

    # Summary statistics
    n = len(images)
    avg_verified = sum(i['verified'] / i['recv'] * 100 for i in images if i['recv'] > 0) / n
    avg_prefix   = sum(i.get('verify_prefix', i['verified']) / i['total_frags'] * 100 for i in images) / n
    chain_ok_n   = sum(1 for i in images if not i.get('chain_broken', False))
    total_relay  = sum(relay.values()) if relay else 0

    print(f"\n--- Summary ({scenario}) ---")
    print(f"Images:               {n}")
    print(f"Chain OK (no break):  {chain_ok_n}/{n} = {chain_ok_n/n*100:.0f}%")
    print(f"Avg verification%:    {avg_verified:.1f}%")
    print(f"Avg prefix%:          {avg_prefix:.1f}%")
    if relay:
        drop_rate = relay['dropped'] / (relay['forwarded'] + relay['dropped']) * 100 if (relay['forwarded'] + relay['dropped']) > 0 else 0
        print(f"Relay drop rate:      {drop_rate:.1f}% ({relay['dropped']}/{relay['forwarded']+relay['dropped']})")
        print(f"Relay bit-flips:      {relay['flipped']}")
        print(f"Relay replays:        {relay['replayed']}")

    # Save JSON
    result = {
        'scenario': scenario,
        'n_images': n,
        'chain_ok_rate': chain_ok_n / n,
        'avg_verification_pct': round(avg_verified, 1),
        'avg_prefix_pct': round(avg_prefix, 1),
        'relay_stats': relay,
        'images': images,
    }
    out_json = log_dir / "e20_result.json"
    out_json.write_text(json.dumps(result, indent=2))
    print(f"\nSaved: {out_json}")

if __name__ == "__main__":
    main()
