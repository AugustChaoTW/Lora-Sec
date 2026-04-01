#!/usr/bin/env python3
import subprocess
import json
from pathlib import Path

# Test single scenario
binary = '/workspace/build/lora-mesh-sim'
output = '/workspace/results/test_python.json'

cmd = [
    binary,
    '--topology=linear',
    '--attack=spoofing', 
    '--usePatch=0',
    '--seed=1',
    '--duration=150',
    f'--output={output}'
]

print('Running:', ' '.join(cmd))
result = subprocess.run(cmd, capture_output=True, text=True)
print('Return code:', result.returncode)
print('Stdout:', result.stdout)
print('Stderr:', result.stderr)

if Path(output).exists():
    with open(output) as f:
        data = json.load(f)
    print('Results:', json.dumps(data, indent=2))
else:
    print('No output file created')
