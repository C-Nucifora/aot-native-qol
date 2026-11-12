#!/usr/bin/env python3
"""Private six-target original-glyph FPS value atlas; no heading art or installation.
Public packaging includes original builder code only, never generated assets.
Uses bundled build_fps_assets.py and glyph_helpers.py original algorithms.
"""
import argparse,base64,hashlib,importlib.util,io,json,struct,sys
from pathlib import Path
from PIL import Image
import numpy as np
if sys.flags.optimize:raise SystemExit('Integrity assertions must remain enabled')
sys.dont_write_bytecode=True
HERE=Path(__file__).parent
spec=importlib.util.spec_from_file_location('fps_util',HERE/'build_fps_assets.py');fps=importlib.util.module_from_spec(spec);spec.loader.exec_module(fps)
TARGETS=[60,90,120,144,165,240]
STATES=['dim','normal','focused']
def sha(b):return hashlib.sha256(b).hexdigest()
def refine_value_alpha(raw,im):
 # Extra threshold weight for small isolated value labels. Palette errors
 # remain bounded by measured alpha error; no glyph dilation/sharpening.
 raw=bytearray(raw);a=np.array(im.getchannel('A'),dtype=np.int16)
 for by in range(im.height//4):
  for bx in range(im.width//4):
   vals=a[by*4:by*4+4,bx*4:bx*4+4].reshape(16);at=(by*(im.width//4)+bx)*16
   if np.all(vals==vals[0]) and int(vals[0]) in (0,255):
    raw[at:at+8]=bytes([int(vals[0])]*2)+bytes(6);continue
   lo,hi=int(vals.min()),int(vals.max())
   ends=sorted(set([0,255,33,127,128]+[max(0,min(255,v+d)) for v in [lo,hi] for d in [-8,-4,0,4,8]]))
   pairs=np.array([(x,y) for x in ends for y in ends if x>y],dtype=np.int16);x,y=pairs[:,0],pairs[:,1]
   palette=np.stack([x,y]+[((7-k)*x+k*y)//7 for k in range(1,7)],axis=1)
   dist=(palette[:,None,:]-vals[None,:,None]).astype(np.int32)**2
   dist+=1024*((palette[:,None,:]>=128)!=(vals[None,:,None]>=128))
   indices=dist.argmin(axis=2);scores=np.take_along_axis(dist,indices[:,:,None],axis=2).sum(axis=(1,2));best=int(scores.argmin())
   bits=sum(int(v)<<(3*i) for i,v in enumerate(indices[best]));raw[at:at+8]=bytes(map(int,pairs[best]))+bits.to_bytes(6,'little')
 return bytes(raw)

def build(sources,out):
 cmn=sources['CmnParts.g1t'];layout=sources['CmnPartsLayout.bin'];assert len(cmn)==56+2048*2048 and cmn[38]==0xbb
 assert struct.unpack_from('<I',layout,20)[0]==255 and len(layout)==32+255*16
 option=fps.decode(sources['UIOption.g1t']);ol=sources['UIOptionLayout.bin'];rows=[];spans=[]
 for i,text in enumerate(fps.util.LABELS):
  x,y,w,h=struct.unpack_from('<4I',ol,32+(39+i*2)*16);assert (w,h)==(348,60)
  im=option.crop((x,y,x+w,y+h));s=fps.util.spans(im);assert len(s)==len(text);rows.append(im);spans.append(s)
 donors=dict(fps.util.DONORS);donors['9']=(9,1)
 provenance=[]
 for c in sorted(set(''.join(map(str,TARGETS)))):
  row,index=donors[c];assert fps.util.LABELS[row][index]==c
  provenance.append(dict(glyph=c,record=39+row*2,stock_label=fps.util.LABELS[row],span=list(spans[row][index])))
 payload=bytearray(2048*4096);payload[:len(cmn)-56]=cmn[56:];h=bytearray(layout);tiles=[];panels=[]
 out.mkdir(parents=True,exist_ok=True)
 for ti,target in enumerate(TARGETS):
  alpha=Image.new('L',(192,60))
  if target==60:
   # Preserve the already verified contiguous60 substring and its exact spacing.
   alpha.paste(rows[7].getchannel('A').crop((46,0,88,60)),(11,0))
  else:
   for ci,c in enumerate(str(target)):
    row,index=donors[c];left,right=spans[row][index]
    # Same tabular digit-center step as the stock four-digit labels, integer
    # shifts only. Staged short-label spacing is explicit, never font synthesis.
    center=(left+right-1)/2;shift=round(21.5+ci*21.5-center)
    glyph=rows[row].getchannel('A').crop((left-1,0,right+1,60))
    alpha.paste(glyph,(left-1+shift,0))
  assert alpha.getbbox() and alpha.crop((90,0,192,60)).getbbox() is None
  normal=fps.colored(alpha,(178,166,142));states=[fps.colored(alpha,(126,108,72)),normal,fps.focused(normal)]
  preview=[]
  for state,(name,im) in enumerate(zip(STATES,states)):
   j=ti*3+state;raw,decoded,checks=fps.encode(im,refine_value_alpha);filename=f'{target}-{name}';(out/(filename+'.bc3')).write_bytes(raw);im.save(out/(filename+'-working.png'));decoded.save(out/(filename+'-roundtrip.png'))
   x=(j%10)*192;y=2048+(j//10)*64;rect=[x,y,192,60]
   for r in range(16):
    at=((y//4+r)*512+x//4)*16;payload[at:at+768]=raw[r*768:(r+1)*768]
   h.extend(struct.pack('<4I',*rect));tiles.append(dict(target=target,state=state,state_name=name,record=255+j,rect=rect,file=filename+'.bc3',sha256=sha(raw),bytes=len(raw),**checks))
   bg=Image.new('RGBA',decoded.size,(28,19,6,255));bg.alpha_composite(decoded);buf=io.BytesIO();bg.convert('RGB').resize((384,128),Image.Resampling.NEAREST).save(buf,format='PNG');preview.append('<figure><figcaption>'+name+'</figcaption><img width="384" height="128" src="data:image/png;base64,'+base64.b64encode(buf.getvalue()).decode()+'"></figure>')
  panels.append('<h2>'+str(target)+'</h2><div>'+''.join(preview)+'</div>')
 g=bytearray(cmn[:56]);g.extend(payload);g[38]=0xcb;struct.pack_into('<I',g,8,len(g));struct.pack_into('<I',h,20,273)
 assert g[56:56+2048*2048]==cmn[56:] and h[32:32+255*16]==layout[32:]
 assert len(g)==56+2048*4096 and len(h)==32+273*16
 for tile in tiles:
  raw=(out/tile['file']).read_bytes();x,y,w,_=tile['rect'];assert len(raw)==192*64
  assert struct.unpack_from('<4I',h,32+tile['record']*16)==tuple(tile['rect'])
  for r in range(16):
   at=((y//4+r)*512+x//4)*16;assert g[56+at:56+at+768]==raw[r*768:(r+1)*768]
 (out/'CmnParts.g1t').write_bytes(g);(out/'CmnPartsLayout.bin').write_bytes(h)
 result=dict(stage='private offline six-target value art, no timing or runtime installation',targets=TARGETS,state_order=STATES,source_hashes={n:sha(b) for n,b in sources.items()},original_record_count=255,new_record_count=273,original_blocks_preserved=True,original_records_preserved=True,width=2048,height=4096,texture_sha256=sha(g),layout_sha256=sha(h),tiles=tiles,glyph_provenance=provenance,heading='No FPS heading records. Native ASCII fallback owns heading.',descriptor={'size':120,'pointer_offsets':[0,8,16],'rect_offsets':[24,40,56],'state_order':STATES,'preserve_tail':[72,120],'template':'zero full0x78 then native3780a0 case0 after resources ready','value_geometry':[744,0,192,60]},limits=['Original glyph masks are translated, never resampled; staged short-value kerning has not been validated in-game.','Focused glow is a fitted Gaussian approximation, not stock baked pixels.','Availability of artwork does not demonstrate timing equivalence or actual attainable FPS.'],installed=False)
 (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
 (out/'review.html').write_text('<!doctype html><meta charset="utf-8"><title>Six nativeFPS value candidates</title><style>body{background:#111;color:#eee;font:16px system-ui;max-width:1240px;margin:30px auto}div{display:flex;flex-wrap:wrap}figure{margin:8px}img{max-width:100%;height:auto}h2{font-size:21px}</style><h1>Original-glyph FPS values</h1><p>Private BC3 round trips at2x. Each row shows native state0 dim/inactive,1 normal,2 focused. Exact original numeral shapes, integer translation only. Selected glow and short-label spacing require visual approval. No FPS heading atlas records; no runtime/timing changes.</p>'+''.join(panels))
 return result
