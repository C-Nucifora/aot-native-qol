#!/usr/bin/env python3
"""Create an original C++ DXBC pack with build-time-reflected sampler bindings."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

FIELDS = [('EdgeVS','edge_vs'),('EdgePS','edge_ps'),('WeightVS','weight_vs'),
          ('WeightPS','weight_ps'),('BlendVS','blend_vs'),('BlendPS','blend_ps'),
          ('FullscreenVS','fullscreen_vs'),('CopyPS','copy_ps'),('CasPS','cas_ps')]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('results', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--c-output', type=Path)
    args = parser.parse_args()
    results = {r['entry']:r for r in json.loads(args.results.read_text())}
    parts = ['#pragma once', '#include "renderer.hpp"', 'namespace aot::postprocess::compiled {']
    bindings = {}
    c_parts = ['/* Generated from validated DXBC; preserve bundled shader licenses. */', '#pragma once']
    for entry, _ in FIELDS:
        record = results[entry]
        data = (args.results.parent / (entry+'.cso')).read_bytes()
        if not record['valid_dxbc'] or data[:4] != b'DXBC' or struct.unpack_from('<I',data,24)[0] != len(data) or hashlib.sha256(data).hexdigest() != record['sha256']:
            raise ValueError(f'invalid or stale DXBC: {entry}')
        chunks = struct.unpack_from('<I',data,28)[0]
        for i in range(chunks):
            offset = struct.unpack_from('<I',data,32+4*i)[0]
            if offset+8 > len(data) or offset+8+struct.unpack_from('<I',data,offset+4)[0] > len(data):
                raise ValueError(f'out-of-bounds DXBC chunk: {entry}')
        parts.append('inline constexpr unsigned char '+entry+'[] = {'+','.join(str(v) for v in data)+'};')
        if entry != 'CasPS':
            c_parts.append('static const unsigned char aa_'+entry+'[] = {'+','.join(str(v) for v in data)+'};')
        slots = {'LinearSampler':-1,'PointSampler':-1}
        for line in record['bindings']:
            match = re.fullmatch(r'binding (\w+) type=(\d+) slot=(\d+) count=(\d+)',line)
            if match and match[1] in slots:
                if match[2] != '3' or match[4] != '1' or int(match[3]) >= 16:
                    raise ValueError('unexpected sampler binding')
                slots[match[1]] = int(match[3])
        bindings[entry] = slots
    parts.append('inline ShaderPack shaders() { ShaderPack p;')
    for entry, field in FIELDS:
        slots=bindings[entry]
        parts.append(f'p.{field} = {{{entry},sizeof({entry}),{slots["LinearSampler"]},{slots["PointSampler"]}}};')
    parts.append('return p; }\n}')
    args.output.write_text('\n'.join(parts)+'\n')
    if args.c_output:
        args.c_output.write_text('\n'.join(c_parts)+'\n')

if __name__ == '__main__':
    main()
