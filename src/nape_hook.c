#include "nape_hook.h"
#include "aot_owned_files.h"
#include "source-replay-private.h"
static const uint8_t site_bytes[6]={AOT_NAPE_SITE_BYTES};
typedef struct {uint8_t *at;size_t n;} Emit;
static void byte(Emit *e,unsigned b){e->at[e->n++]=(uint8_t)b;}
static void bytes(Emit *e,const uint8_t *p,size_t n){for(size_t i=0;i<n;i++)byte(e,p[i]);}
static void imm64(Emit *e,uintptr_t x){for(unsigned i=0;i<8;i++)byte(e,(unsigned)(x>>(8*i)));}
static int branch(Emit *e,uint8_t op,uintptr_t to){
 byte(e,op);int32_t d;if(!aot_rel32((uintptr_t)e->at+e->n+4,to,&d))return 0;
 for(unsigned i=0;i<4;i++)byte(e,(uint32_t)d>>(8*i));return 1;
}
static int restore(Emit *e,uintptr_t target){
 /* Original RSP at1960c is16-byte aligned; seven GPRs+RFLAGS=64 bytes,
  * plus128 bytes shadow/XMM storage leaves the helper call aligned. */
 for(unsigned i=0;i<6;i++){
  const uint8_t op[5]={0xf3,0x0f,0x6f,(uint8_t)(0x44+8*i),0x24};
  bytes(e,op,5);byte(e,0x20+16*i);
 }
 const uint8_t tail[]={0x48,0x81,0xc4,0x80,0,0,0,0x41,0x5b,0x41,0x5a,0x41,0x59,0x41,0x58,0x5a,0x59,0x58,0x9d};
 bytes(e,tail,sizeof(tail));return branch(e,0xe9,target);
}
AotStatus nape_hook_prepare(uint8_t *base,size_t size,const uint8_t expected[6],NapePermit permit,AotPatch *patch){
 if(!base || size!=0xfb5000 || !expected || !permit || !patch)return AOT_BAD_RANGE;
 for(unsigned i=0;i<6;i++)if(expected[i]!=site_bytes[i] || base[0x1960c+i]!=expected[i])return AOT_SIGNATURE_MISMATCH;
 uint8_t *code=aot_allocate_near((uintptr_t)base+0x19612,(uintptr_t)base+0x1962b,512);
 if(!code)return AOT_BAD_RANGE;Emit e={code,0};
 bytes(&e,site_bytes,4); /* Same COMISS including unordered exception/flags. */
 byte(&e,0x0f);if(!branch(&e,0x86,(uintptr_t)base+0x19612))return AOT_BAD_RANGE;
 const uint8_t save[]={0x9c,0x50,0x51,0x52,0x41,0x50,0x41,0x51,0x41,0x52,0x41,0x53,0x48,0x81,0xec,0x80,0,0,0};
 bytes(&e,save,sizeof(save));
 for(unsigned i=0;i<6;i++){
  const uint8_t op[5]={0xf3,0x0f,0x7f,(uint8_t)(0x44+8*i),0x24};
  bytes(&e,op,5);byte(&e,0x20+16*i);
 }
 const uint8_t args[]={0x48,0x89,0xf1,0x89,0xda,0x0f,0x28,0xd6,0x41,0x0f,0x28,0xda,0x48,0xb8};
 bytes(&e,args,sizeof(args));imm64(&e,(uintptr_t)permit);
 const uint8_t call[]={0xff,0xd0,0x85,0xc0,0x0f,0x84};bytes(&e,call,sizeof(call));
 size_t refused=e.n;e.n+=4;
 if(!restore(&e,(uintptr_t)base+0x19612))return AOT_BAD_RANGE;
 int32_t delta;if(!aot_rel32((uintptr_t)code+refused+4,(uintptr_t)code+e.n,&delta))return AOT_BAD_RANGE;
 for(unsigned i=0;i<4;i++)code[refused+i]=(uint8_t)((uint32_t)delta>>(8*i));
 if(!restore(&e,(uintptr_t)base+0x1962b) || e.n>512)return AOT_BAD_RANGE;
 unsigned long old;
 if(!VirtualProtect(code,512,0x20,&old) || !FlushInstructionCache(GetCurrentProcess(),code,e.n))return AOT_PROTECTION_FAILED;
 *patch=(AotPatch){.rva=0x1960c,.length=6};
 for(unsigned i=0;i<6;i++)patch->expected[i]=expected[i];
 if(!aot_rel32((uintptr_t)base+0x19611,(uintptr_t)code,&delta))return AOT_BAD_RANGE;
 patch->replacement[0]=0xe9;for(unsigned i=0;i<4;i++)patch->replacement[i+1]=(uint8_t)((uint32_t)delta>>(8*i));patch->replacement[5]=0x90;
 return AOT_OK;
}
