#!/usr/bin/env python3
"""Locally reproduce supported seven-value art. Never installs or downloads."""
import sys
if sys.flags.optimize:raise SystemExit('Optimized execution is unsupported')
sys.dont_write_bytecode=True
import argparse,hashlib,json,struct
from pathlib import Path
import build_seven_value_assets as seven
SLOTS={1621:'Font.g1t',1622:'CmnParts.g1t',1623:'CmnPartsLayout.bin',1702:'UIOption.g1t',1703:'UIOptionLayout.bin'}
EXPECTED_TEXTURE='46ab9e4274c50c382285a8579783d3149f124ed9b69264f02083fb93032ae45c'
EXPECTED_LAYOUT='a8ffad74ab3d5e47a5e1e16490b2919deb8c9e04b132c67c59eb0c9944e728d5'
def sha(data):return hashlib.sha256(data).hexdigest()
def load_file(path):
 if not path.is_file() or path.stat().st_size>32*1024*1024:raise ValueError('Missing or oversized input: '+str(path))
 return path.read_bytes()
def inputs(args):
 sources={}
 if args.archive:
  sources={name:seven.six.fps.util.read_slot(args.archive,slot) for slot,name in SLOTS.items()}
 else:
  sources={name:load_file(args.inputs/name) for name in SLOTS.values() if name!='Font.g1t' or not args.font}
 if args.font:sources['Font.g1t']=load_file(args.font)
 for name,data in sources.items():
  if sha(data)!=seven.six.fps.EXPECTED[name]:raise ValueError('Unsupported input identity: '+name)
 return sources
def main():
 p=argparse.ArgumentParser(description=__doc__);group=p.add_mutually_exclusive_group(required=True);group.add_argument('--archive',type=Path);group.add_argument('--inputs',type=Path,help='Locally extracted five named source assets');p.add_argument('--font',type=Path,help='Optional exact supported Font.g1t override');p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 root=Path(__file__).resolve().parents[2];out=a.output.resolve()
 if out==root or root in out.parents:
  if not any(x in out.parents for x in [root/'build',root/'private']):raise ValueError('Game-derived output must be under ignored build/private or outside the source tree')
 # Validate every input before creating any output file/directory.
 sources=inputs(a)
 if out.exists():raise ValueError('Use a new private output directory; existing files are preserved')
 result=seven.build(sources,out)
 if result['texture_sha256']!=EXPECTED_TEXTURE or result['layout_sha256']!=EXPECTED_LAYOUT:raise ValueError('Rebuild differs from reviewed assets; retain privately for diagnosis, do not install')
 print(json.dumps({'texture_sha256':result['texture_sha256'],'layout_sha256':result['layout_sha256'],'records':result['new_record_count'],'tiles':len(result['tiles']),'min_iou':min(t['silhouette_iou'] for t in result['tiles'])},indent=2))
if __name__=='__main__':
 try:main()
 except (OSError,ValueError,AssertionError) as e:raise SystemExit(str(e))
