#!/usr/bin/env python3
"""Private original-glyph FPS/60 asset candidate. Does not install hooks or assets.
Uses the bundled original glyph_helpers.py algorithms.
Generated art and this folder's research data must not enter a public repo.
"""
import argparse,base64,hashlib,importlib.util,io,json,struct,sys
from pathlib import Path
from PIL import Image,ImageFilter
if sys.flags.optimize:raise SystemExit('Integrity assertions must remain enabled')
sys.dont_write_bytecode=True
HERE=Path(__file__).parent
spec=importlib.util.spec_from_file_location('glyph_util',HERE/'glyph_helpers.py');util=importlib.util.module_from_spec(spec);spec.loader.exec_module(util)
EXPECTED={'CmnParts.g1t':'bf141e74dd1da62ecd7fe1a1a58b8aa191c33ab4688ba419e14d2606200a4b45','CmnPartsLayout.bin':'81f90b9c3394687f156ef91e6e87428e84b5f88898630178be4760b00580994e','Font.g1t':'3d67b33decd3e263661efe6b626b0731fe184eea0c6ac50e0d3b63afe5c287af','UIOption.g1t':util.EXPECTED['texture'],'UIOptionLayout.bin':util.EXPECTED['layout']}
def sha(b):return hashlib.sha256(b).hexdigest()
def decode(b):
 assert b[:8]==b'GT1G0600' and b[37]==0x5b and struct.unpack_from('<I',b,16)[0]==1
 d=b[38];w,h=1<<(d&15),1<<(d>>4);assert len(b)==56+w*h
 return Image.open(io.BytesIO(util.dds(b[56:],w,h))).convert('RGBA')
def colored(alpha,rgb):
 out=Image.new('RGBA',alpha.size,(*rgb,0));out.putalpha(alpha);return out
def focused(normal):
 a=normal.getchannel('A');halo=colored(a.filter(ImageFilter.GaussianBlur(4)).point(lambda x:min(255,round(x*1.5))),(255,77,57))
 return Image.alpha_composite(halo,colored(a,(255,255,255)))
def encode(im,refine=None):
 # Height64 is block-aligned; only first60 source rows are sampled at runtime.
 tile=Image.new('RGBA',(im.width,64));tile.paste(im,(0,0));buf=io.BytesIO();tile.save(buf,format='DDS',pixel_format='DXT5');b=buf.getvalue();assert b[84:88]==b'DXT5'
 raw=(refine or util.refine_bc3_alpha)(b[128:],tile);assert len(raw)==im.width*64
 decoded=Image.open(io.BytesIO(util.dds(raw,im.width,64))).convert('RGBA')
 a=tile.getchannel('A').tobytes();d=decoded.getchannel('A').tobytes();inter=sum(x>=128 and y>=128 for x,y in zip(a,d));union=sum(x>=128 or y>=128 for x,y in zip(a,d));iou=inter/union
 assert iou>.995;assert decoded.crop((0,60,im.width,64)).getchannel('A').getbbox() is None
 # Independent block alpha decoder, separate from Pillow's DDS decoding path.
 for by in range(16):
  for bx in range(im.width//4):
   at=(by*(im.width//4)+bx)*16;x,y=raw[at:at+2];bits=int.from_bytes(raw[at+2:at+8],'little')
   palette=[x,y]+([((7-k)*x+k*y)//7 for k in range(1,7)] if x>y else [((5-k)*x+k*y)//5 for k in range(1,5)]+[0,255])
   for j in range(16):assert abs(palette[(bits>>(3*j))&7]-decoded.getpixel((bx*4+j%4,by*4+j//4))[3])<=1
 return raw,decoded,dict(alpha_mae=sum(abs(x-y) for x,y in zip(a,d))/len(a),silhouette_iou=iou,independent_alpha_decode=True)
