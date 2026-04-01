#!/usr/bin/env python3
import time
import subprocess

# Test 3 quick scenarios to estimate timing
scenarios = [
    ['--topology=linear', '--attack=none', '--usePatch=0'],
    ['--topology=tree', '--attack=spoofing', '--usePatch=0'], 
    ['--topology=grid', '--attack=replay', '--usePatch=1']
]

times = []
for i, args in enumerate(scenarios):
    start = time.time()
    cmd = ['/workspace/build/lora-mesh-sim'] + args + [
        '--seed=1', '--duration=150', f'--output=/tmp/test_{i}.json'
    ]
    subprocess.run(cmd, capture_output=True)
    elapsed = time.time() - start
    times.append(elapsed)
    print(f'Scenario {i+1}: {elapsed:.1f}s')

avg_time = sum(times) / len(times)
print(f'Average time per scenario: {avg_time:.1f}s')

# Calculate for 27 scenarios (3 topologies × 3 attacks × 3 patches)
total_scenarios = 27
estimated_total = total_scenarios * avg_time
print(f'Estimated time for 27 scenarios: {estimated_total/60:.1f} minutes')
print(f'With 10 parallel workers: {estimated_total/10/60:.1f} minutes')
