#!/usr/bin/env python3
"""Compile flattened licensed shader sources using an explicit build-only Wine prefix."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess

ENTRIES = [('smaa','EdgeVS','vs_5_0'),('smaa','EdgePS','ps_5_0'),
           ('smaa','WeightVS','vs_5_0'),('smaa','WeightPS','ps_5_0'),
           ('smaa','BlendVS','vs_5_0'),('smaa','BlendPS','ps_5_0'),
           ('cas','CasPS','ps_5_0'),('copy','FullscreenVS','vs_5_0'),('copy','CopyPS','ps_5_0')]

def flatten(path, root, stack=()):
    path = path.resolve()
    if not path.is_relative_to(root) or path in stack:
        raise ValueError(f'unsafe or cyclic include: {path}')
    text = path.read_bytes().decode('latin1')
    return re.sub(r'^\s*#include\s+"([^"]+)"\s*$',
                  lambda m: flatten(path.parent / m[1], root, stack + (path,)), text, flags=re.M)

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--compiler', required=True, type=Path)
    p.add_argument('--wine-prefix', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    args = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.update(WINEPREFIX=str(args.wine_prefix.resolve()), WINEDEBUG='-all', DISPLAY='', WAYLAND_DISPLAY='')
    results = []
    for src, entry, profile in ENTRIES:
        flat = out / f'{src}-flattened.hlsl'
        flat.write_text(flatten(root / 'shaders' / f'{src}.hlsl', root), encoding='utf8')
        output = out / f'{entry}.cso'
        run = subprocess.run([str(args.compiler.resolve()), 'Z:'+str(flat), entry, profile, 'Z:'+str(output)],
                             env=env, capture_output=True, text=True, timeout=90)
        result = dict(entry=entry, profile=profile, exitcode=run.returncode, output=run.stdout+run.stderr, valid_dxbc=False)
        if run.returncode == 0 and output.exists():
            data = output.read_bytes()
            result['valid_dxbc'] = len(data) >= 32 and data[:4] == b'DXBC' and struct.unpack_from('<I',data,24)[0] == len(data)
            result['sha256'] = hashlib.sha256(data).hexdigest()
            result['bytes'] = len(data)
            result['bindings'] = run.stdout.splitlines()
        results.append(result)
        print(entry, run.returncode, result['valid_dxbc'], flush=True)
    (out / 'results.json').write_text(json.dumps(results, indent=2)+'\n')
    return 0 if all(r['valid_dxbc'] for r in results) else 1

if __name__ == '__main__':
    raise SystemExit(main())
