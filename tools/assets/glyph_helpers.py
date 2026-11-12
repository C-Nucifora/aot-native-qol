#!/usr/bin/env python3
"""Private deterministic composition of stock resolution numerals. No fonts/models.
Only this original code may enter a public repository. All generated art stays local.
"""
import argparse,base64,hashlib,io,json,struct,zlib,sys
if sys.flags.optimize:
 raise SystemExit("Optimized Python is unsupported; integrity assertions must stay enabled")
from pathlib import Path
from PIL import Image, ImageFilter
import numpy as np
EXPECTED={'texture':'b0a72fd93e217a7062889eba6ca69606827aad69738073e262bc024bc1b31448','layout':'8c8a9843032bdb252ca48122b2c188a2dfb0e44bb64df769a23404ccbd5ae384'}
LABELS=['640×360/480','800×450/600','1024×576/768','1280×720/720','1280×720/768','1280×720/960','1280×720/1024','1600×900/900','1600×900/1200','1920×1080/1080','1920×1080/1200']
DONORS={'0':(9,3),'1':(9,0),'2':(9,2),'3':(0,4),'4':(0,1),'5':(2,5),'6':(0,0),'8':(9,7)}
def sha(b):return hashlib.sha256(b).hexdigest()
MAX_ASSET_BYTES=32*1024*1024
CHUNK_BYTES=32768
ARCHIVE_HEADER=(491001,5352,2048,0)
def decode_slot(raw,total):
 if not 0<len(raw)<=MAX_ASSET_BYTES or not 0<=total<=MAX_ASSET_BYTES:
  raise ValueError('Unsupported asset size')
 if not total:return raw
 if len(raw)<4 or struct.unpack_from('<I',raw)[0]!=total:
  raise ValueError('Compressed total mismatch')
 at=4;result=bytearray()
 while len(result)<total:
  if len(raw)-at<4:raise ValueError('Truncated chunk length')
  n=struct.unpack_from('<I',raw,at)[0];at+=4
  if not n or n>len(raw)-at:raise ValueError('Chunk outside stored extent')
  expected=min(CHUNK_BYTES,total-len(result))
  decoder=zlib.decompressobj()
  try:piece=decoder.decompress(raw[at:at+n],expected+1)
  except zlib.error as error:raise ValueError('Invalid compressed chunk') from error
  # At most32768 output bytes plus one overflow sentinel per decoder call.
  # No unlimited flush, concatenated streams or trailing compressed data.
  if len(piece)!=expected or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
   raise ValueError('Invalid chunk output length or stream framing')
  result.extend(piece);at+=n
 if len(raw)-at!=4 or struct.unpack_from('<I',raw,at)[0]!=0:
  raise ValueError('Missing terminator or trailing stored data')
 return bytes(result)
def read_slot(path,slot):
 with path.open('rb') as f:
  f.seek(0,2);size=f.tell();f.seek(0);header=f.read(16)
  if len(header)!=16 or struct.unpack('<4I',header)!=ARCHIVE_HEADER:
   raise ValueError('Unsupported archive header')
  count=ARCHIVE_HEADER[1];table_end=16+count*16
  if size<table_end:raise ValueError('Truncated full archive table')
  table=f.read(count*16)
  if len(table)!=count*16:raise ValueError('Truncated full archive table')
  if not 0<=slot<count:raise ValueError('Slot outside table')
  for sector,_,stored,total in struct.iter_unpack('<4I',table):
   start=sector*2048
   if not sector or not stored or start<table_end or start>size or stored>size-start:
    raise ValueError('Archive extent outside data region')
  sector,_,stored,total=struct.unpack_from('<4I',table,slot*16)
  if max(stored,total)>MAX_ASSET_BYTES:raise ValueError('Oversized selected asset')
  f.seek(sector*2048);raw=f.read(stored)
  if len(raw)!=stored:raise ValueError('Truncated selected asset')
 return decode_slot(raw,total)

def dds(payload,w,h):
 header=struct.pack('<7I',124,0x81007,h,w,len(payload),0,1)+bytes(44)+struct.pack('<8I',32,4,0x35545844,0,0,0,0,0)+struct.pack('<5I',0x1000,0,0,0,0)
 return b'DDS '+header+payload

def refine_bc3_alpha(raw, im):
 # Keep Pillow's color blocks. Search endpoint palettes for alpha coverage,
 # lightly penalizing threshold flips as well as ordinary squared error.
 raw=bytearray(raw);a=np.array(im.getchannel('A'),dtype=np.int16)
 for by in range(im.height//4):
  for bx in range(im.width//4):
   vals=a[by*4:by*4+4,bx*4:bx*4+4].reshape(16)
   if np.all(vals==vals[0]):
    v=int(vals[0]);# Constant endpoints decode exactly.
    if v in (0,255):
     at=(by*(im.width//4)+bx)*16;raw[at:at+8]=bytes([v,v])+bytes(6);continue
   lo,hi=int(vals.min()),int(vals.max())
   ends=sorted(set([0,255,33,127,128]+[max(0,min(255,v+d)) for v in [lo,hi] for d in [-8,-4,0,4,8]]))
   pairs=np.array([(x,y) for x in ends for y in ends if x>y],dtype=np.int16)
   x,y=pairs[:,0],pairs[:,1]
   palette=np.stack([x,y]+[((7-j)*x+j*y)//7 for j in range(1,7)],axis=1)
   dist=(palette[:,None,:]-vals[None,:,None]).astype(np.int32)**2
   dist+=128*((palette[:,None,:]>=128)!=(vals[None,:,None]>=128))
   indices=dist.argmin(axis=2);score=np.take_along_axis(dist,indices[:,:,None],axis=2).sum(axis=(1,2));best=int(score.argmin())
   bits=sum(int(v)<<(3*i) for i,v in enumerate(indices[best]));at=(by*(im.width//4)+bx)*16
   raw[at:at+8]=bytes(map(int,pairs[best]))+bits.to_bytes(6,'little')
 return bytes(raw)

def spans(im):
 a=im.getchannel('A');on=[any(a.getpixel((x,y))>=128 for y in range(im.height)) for x in range(im.width)];result=[];start=None
 for x,v in enumerate(on+[False]):
  if v and start is None:start=x
  if not v and start is not None:result.append((start,x));start=None
 return result
