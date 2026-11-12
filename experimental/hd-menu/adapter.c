/* Single-generation private prototype. No live unload or native release. */
#include "aot_mod.h"
#include "guards.h"
AOT_API unsigned long __stdcall GetCurrentThreadId(void);
AOT_API void * __stdcall GetProcAddress(void *,const char *);
__declspec(dllexport) void InitializeASI(void);
AOT_API unsigned long __stdcall GetModuleFileNameW(void *,uint16_t *,unsigned long);
AOT_API unsigned int __stdcall GetPrivateProfileIntW(const uint16_t *,const uint16_t *,int,const uint16_t *);
AOT_API int __stdcall GetModuleHandleExW(unsigned long,const uint16_t *,void **);
AOT_API void * __stdcall CreateFileW(const uint16_t *,unsigned long,unsigned long,void *,unsigned long,unsigned long,void *);
AOT_API int __stdcall ReadFile(void *,void *,unsigned long,unsigned long *,void *);
AOT_API int __stdcall CloseHandle(void *);
AOT_API int __stdcall TerminateProcess(void *,unsigned);
#include <stddef.h>
typedef struct {void **v;} Com;
typedef struct {uint32_t a;uint16_t b,c;uint8_t d[8];} Guid;
typedef struct {uint32_t width,height,mips,array,format,samples,quality,usage,bind,cpu,misc;} TexDesc;
typedef struct {uint32_t format,dimension,mip,mips,unused[2];} ViewDesc;
typedef struct Rect {int32_t x,y,w,h;} Rect;
typedef void (*Draw)(void *,const Rect *,const Rect *,const void *,unsigned,int,int);
static const Guid iid_tex={0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};
static const Guid iid_srv={0xb0e06fe0,0x8192,0x4e1a,{0xb1,0xca,0x36,0xd7,0x41,0x47,0x10,0xb2}};
_Static_assert(sizeof(TexDesc)==44 && sizeof(Rect)==16,"D3D11/native ABI");
typedef struct {uint8_t wrapper[40];uint8_t meta[72];void *native;Com *tex,*srv,*alt,*stock_tex;void *stock_wrapper,*stock_meta,*stock_srv,*stock_alt;} Atlas;
static struct {
 uint8_t *base,*relay,*raw[2];void *self,*pin,*manager;Com *device;
 Atlas atlas[2];AotPatch patch[2];_Atomic unsigned phase,thread,busy,draws;unsigned attempted;_Atomic unsigned init_thread;
 uint16_t config[1024],files[2][1024];
} hd;
/* phase0 untouched,1 off/refused,2 cold armed,3 first generation ready,
 * phase4 permanently disabled with all refs retained,5 uncertain rollback. */
#ifndef HDUI_NATIVE
#define HDUI_NATIVE(r,t) ((t)(hd.base+(r)))
#endif
static int equal(const void *aa,const void *bb,size_t n){const uint8_t *a=aa,*b=bb;for(size_t i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static void copy(void *dd,const void *ss,size_t n){uint8_t *d=dd;const uint8_t *s=ss;for(size_t i=0;i<n;i++)d[i]=s[i];}
static void addref(Com *p){((unsigned long (*)(Com *))p->v[1])(p);}
static void release(Com *p){if(p)((unsigned long (*)(Com *))p->v[2])(p);}
static int query(Com *p,const Guid *id,void **out){return p && ((int32_t (*)(Com *,const Guid *,void **))p->v[0])(p,id,out)>=0 && *out;}
static Com *device(Com *p){Com *d=0;if(p)((void (*)(Com *,Com **))p->v[3])(p,&d);return d;}
static void disable(void){hd.phase=4;OutputDebugStringA("[aot-hdui] permanently disabled; first generation retained until process exit\n");}
static _Noreturn void uncertain(void){hd.phase=5;TerminateProcess(GetCurrentProcess(),0xa074d001u);__builtin_trap();}
static int path(const char *suffix,uint16_t *out){unsigned long n=GetModuleFileNameW(hd.self,out,1024);if(!n||n>=1024)return 0;while(n&&out[n-1]!='\\'&&out[n-1]!='/')--n;if(!n)return 0;for(unsigned i=0;;i++){if(n+i>=1024)return 0;out[n+i]=(uint8_t)suffix[i];if(!suffix[i])return 1;}}
static int outside_lock(void){
#ifdef HDUI_HOST
 extern uint8_t *hdui_host_peb(void);uint8_t *peb=hdui_host_peb();
#else
 uint8_t *peb;__asm__("movq %%gs:0x60,%0":"=r"(peb));
#endif
 return peb && *(void **)(peb+0x110) && !*(uintptr_t *)((uint8_t *)*(void **)(peb+0x110)+0x10);
}
static int caller_authorized(uintptr_t caller){
 static const uint16_t name[]={'d','3','d','1','1','.','d','l','l',0};
 void *loader=GetModuleHandleW(name),*owner=0;
 return outside_lock() && loader && GetProcAddress(loader,"IsUltimateASILoader") &&
  caller==(uintptr_t)loader+0x202fe &&
  GetModuleHandleExW(6,(const uint16_t *)caller,&owner) && owner==loader;
}
static int cold_roots(void){
 static const uint32_t roots[]={0xc9d9c0,0xc9d9c8,0xc9cae8,0xc492c8,0xc9cbc0,0xc9cc18};
 if(!hd.base)return 0;
 for(unsigned i=0;i<sizeof(roots)/sizeof(roots[0]);i++)if(*(void **)(hd.base+roots[i]))return 0;
 return 1;
}
static int supported(void){uint8_t *b=hd.base;
 if(!b||b[0]!='M'||b[1]!='Z'||*(uint32_t *)(b+60)!=0x148)return 0;
 uint8_t *p=b+0x148;
 if(*(uint32_t *)p!=0x4550||*(uint16_t *)(p+4)!=0x8664||*(uint32_t *)(p+8)!=0x57ea28d6||*(uint16_t *)(p+24)!=0x20b||*(uint32_t *)(p+80)!=0xfb5000)return 0;
 for(unsigned i=0;i<sizeof(native_guards)/sizeof(native_guards[0]);i++){
  const Guard *g=&native_guards[i];if(!equal(b+g->rva,g->bytes,g->n))return 0;
 }
 return 1;
}
static int load(unsigned i){
 const size_t n=2097208;hd.raw[i]=VirtualAlloc(0,n+1,AOT_MEM_RESERVE_COMMIT,AOT_PAGE_READWRITE);if(!hd.raw[i])return 0;
 void *f=CreateFileW(hd.files[i],0x80000000,1,0,3,0x80,0);if(f==(void *)(intptr_t)-1)return 0;
 unsigned long got=0;int ok=ReadFile(f,hd.raw[i],(unsigned long)n+1,&got,0);int closed=CloseHandle(f);
 if(!ok||!closed||got!=n||!equal(hd.raw[i],atlas_header,56))return 0;
 /* Entire private payload frozen by generated SHA256. */
 uint8_t hash[32];hd_sha256(hd.raw[i],n,hash);if(!equal(hash,atlas_hash[i],32))return 0;
 unsigned long old;return VirtualProtect(hd.raw[i],n+1,0x02,&old);
}
/* Native BC3 loader owns immutable TYPELESS textures; typed views select gamma. */
static int texture_info(Com *resource,unsigned w,unsigned h,Com **tex_out){
 Com *tex=0;if(!query(resource,&iid_tex,(void **)&tex))return 0;
 TexDesc d={0};((void (*)(Com *,TexDesc *))tex->v[10])(tex,&d);
 int ok=d.width==w&&d.height==h&&d.mips==1&&d.array==1&&d.format==76&&d.samples==1&&!d.quality&&d.usage==1&&d.bind==8&&!d.cpu&&!d.misc;
 if(ok)*tex_out=tex;else release(tex);return ok;
}
static int same_resource(Com *view,Com *texture,unsigned format){
 Com *srv=0,*resource=0,*tex=0;
 if(!query(view,&iid_srv,(void **)&srv))return 0;
 ViewDesc d={0};((void (*)(Com *,ViewDesc *))srv->v[8])(srv,&d);
 if(d.format!=format||d.dimension!=4||d.mip||d.mips!=1){release(srv);return 0;}
 ((void (*)(Com *,Com **))srv->v[7])(srv,&resource);
 int ok=query(resource,&iid_tex,(void **)&tex)&&tex==texture;
 release(tex);release(resource);release(srv);return ok;
}
static void *metadata(void *wrapper){return wrapper && *(uint32_t *)((uint8_t *)wrapper+24)==1?*(void **)((uint8_t *)wrapper+32):0;}
static int create_generation(void *off,void *on){
 if(hd.attempted)return 0;hd.attempted=1;
 hd.manager=*(void **)(hd.base+0xc492c8);if(!hd.manager)return 0;
 for(unsigned i=0;i<2;i++){
  Atlas *a=&hd.atlas[i];a->stock_wrapper=i?on:off;a->stock_meta=metadata(a->stock_wrapper);
  if(!a->stock_meta||*(uint16_t *)((uint8_t *)a->stock_meta+48)!=512||*(uint16_t *)((uint8_t *)a->stock_meta+50)!=1024)return 0;
  if(!texture_info(*(Com **)a->stock_meta,512,1024,&a->stock_tex))return 0;
  a->stock_srv=*(void **)((uint8_t *)a->stock_meta+8);
  a->stock_alt=*(void **)((uint8_t *)a->stock_meta+16);
  if(!same_resource(a->stock_srv,a->stock_tex,77)||
     (a->stock_alt&&!same_resource(a->stock_alt,a->stock_tex,78)))return 0;
  /* Loader returned ref, raw bytes, and explicit COM refs are process-lifetime.
   * No native wrapper is destroyed even on validation failure. */
  a->native=HDUI_NATIVE(0x418fb0,void *(*)(const void *,unsigned))(hd.raw[i],2097208);
  void *m=metadata(a->native);if(!m||!*(uint32_t *)((uint8_t *)a->native+8))return 0;
  if(*(uint16_t *)((uint8_t *)m+48)!=1024||*(uint16_t *)((uint8_t *)m+50)!=2048||*(void **)((uint8_t *)m+24)||*(void **)((uint8_t *)m+32))return 0;
  if(!texture_info(*(Com **)m,1024,2048,&a->tex))return 0;
  a->srv=*(Com **)((uint8_t *)m+8);a->alt=*(Com **)((uint8_t *)m+16);
  if(!a->srv||!same_resource(a->srv,a->tex,77)||(a->alt&&!same_resource(a->alt,a->tex,78)))return 0;
  addref(a->srv);if(a->alt)addref(a->alt);
  Com *d=device(a->tex),*s=device(a->stock_tex);
  if(!d||!s||d!=s||(hd.device&&hd.device!=d)){release(d);release(s);return 0;}
  if(!hd.device)hd.device=d;else release(d);release(s);
  copy(a->meta,m,72);*(uint32_t *)(a->wrapper+24)=1;*(void **)(a->wrapper+32)=a->meta;
 }
 return 1;
}
static int generation_valid(void *off,void *on){
 if(*(void **)(hd.base+0xc492c8)!=hd.manager)return 0;
 if(((int32_t (*)(Com *))hd.device->v[39])(hd.device)<0)return 0;
 for(unsigned i=0;i<2;i++){
  Atlas *a=&hd.atlas[i];void *w=i?on:off;void *m=metadata(w);
  if(w!=a->stock_wrapper||m!=a->stock_meta||!m||*(void **)m!=(void *)a->stock_tex||*(void **)((uint8_t *)m+8)!=a->stock_srv||*(void **)((uint8_t *)m+16)!=a->stock_alt)return 0;
  Com *d=device(a->stock_tex);int ok=d==hd.device;release(d);if(!ok)return 0;
 }
 return 1;
}
static void heading(void *texture,const Rect *dst,const Rect *src,const void *tint,unsigned layer,int index,int flags){
 Draw stock=HDUI_NATIVE(0x3e8380,Draw);
 if(hd.phase==4||hd.busy){stock(texture,dst,src,tint,layer,index,flags);return;}
 unsigned thread=GetCurrentThreadId(),unclaimed=0;
 (void)__atomic_compare_exchange_n((unsigned *)&hd.thread,&unclaimed,thread,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);
 if(hd.thread!=thread){disable();stock(texture,dst,src,tint,layer,index,flags);return;}
 void *off=*(void **)(hd.base+0xc9cbc0),*on=*(void **)(hd.base+0xc9cc18);
 if(hd.phase==3&&!generation_valid(off,on))disable();
 int valid=hd.phase!=4&&*(uint8_t *)(hd.base+0xc7a480)==0&&off&&on&&(texture==off||texture==on)&&src&&dst&&src->x==0&&src->w==512&&src->h==60&&src->y>=0&&src->y<=120&&src->y%60==0&&dst->w==512&&dst->h==60&&index==0&&flags==1;
 if(!valid){stock(texture,dst,src,tint,layer,index,flags);return;}
 hd.busy=1;
 if(hd.phase==2){if(create_generation(off,on)){unsigned expected=2;(void)__atomic_compare_exchange_n((unsigned *)&hd.phase,&expected,3,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);}else disable();}
 if(hd.phase==3){Rect doubled={0,src->y*2,1024,120};Atlas *a=&hd.atlas[texture==on];stock(a->wrapper,dst,&doubled,tint,layer,index,flags);hd.draws++;}
 else stock(texture,dst,src,tint,layer,index,flags);
 hd.busy=0;
}
static void renderer_retire(void *owner){
 /* Runs before the reviewed renderer destructor releases its internal state.
  * Never free our references here: earlier worker packets may still borrow them. */
 if(hd.attempted && owner==hd.manager)disable();
 HDUI_NATIVE(0x47a540,void (*)(void *))(owner);
}
__declspec(dllexport) unsigned AotHdUiStatus(void){return hd.phase;}
__declspec(dllexport) unsigned AotHdUiDraws(void){return hd.draws;}
__declspec(dllexport) unsigned AotHdUiRollbackCold(void){
 if(hd.phase!=2||hd.attempted||hd.init_thread!=GetCurrentThreadId()||!outside_lock()||*(void **)(hd.base+0xc492c8))return AOT_BAD_RANGE;
 AotStatus r=aot_revert(hd.base,0xfb5000,hd.patch,2);if(r==AOT_ROLLBACK_FAILED)uncertain();if(r==AOT_OK)hd.phase=1;return r;
}
static void initialize_from(uintptr_t caller){
 if(hd.phase || !caller_authorized(caller))return;
 unsigned thread=GetCurrentThreadId(),unclaimed=0;
 if(!thread || (!__atomic_compare_exchange_n((unsigned *)&hd.init_thread,&unclaimed,thread,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST) && unclaimed!=thread))return;
 unsigned expected=0;
 if(!__atomic_compare_exchange_n((unsigned *)&hd.phase,&expected,1,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST))return;
 void *self=0;
 if(!GetModuleHandleExW(6,(const uint16_t *)(uintptr_t)&InitializeASI,&self) || self!=hd.self)return;
 hd.base=GetModuleHandleW(0);
 if(!supported() || !cold_roots())return;
 if(!path("aot-native-hdui.cfg",hd.config))return;
 static const uint16_t section[]={'H','D','U','I',0},key[]={'E','n','a','b','l','e','d',0};
 if(GetPrivateProfileIntW(section,key,0,hd.config)!=1)return;
 if(!path("hdui-assets\\UIOptionSteamMenuOff-2x.g1t",hd.files[0])||!path("hdui-assets\\UIOptionSteamMenuOn-2x.g1t",hd.files[1])||!load(0)||!load(1))return;
 if(!GetModuleHandleExW(5,(const uint16_t *)(uintptr_t)&heading,&hd.pin))return;
 uintptr_t low=(uintptr_t)hd.base+0x3d844b,high=(uintptr_t)hd.base+0x472e48;
 hd.relay=aot_allocate_near(low,high,32);if(!hd.relay)return;
 uint32_t sites[2]={0x3d8446,0x472e43};void *targets[2]={(void *)&heading,(void *)&renderer_retire};
 for(unsigned k=0;k<2;k++){
  uint8_t *thunk=hd.relay+16*k;thunk[0]=0xff;thunk[1]=0x25;for(unsigned i=2;i<6;i++)thunk[i]=0;
  uintptr_t target=(uintptr_t)targets[k];for(unsigned i=0;i<8;i++)thunk[6+i]=(uint8_t)(target>>(i*8));
  int32_t delta;if(!aot_rel32((uintptr_t)hd.base+sites[k]+5,(uintptr_t)thunk,&delta))return;
  hd.patch[k].rva=sites[k];hd.patch[k].length=5;copy(hd.patch[k].expected,hd.base+sites[k],5);hd.patch[k].replacement[0]=0xe8;
  for(unsigned i=0;i<4;i++)hd.patch[k].replacement[i+1]=(uint8_t)((uint32_t)delta>>(i*8));
 }
 unsigned long old;
 if(!VirtualProtect(hd.relay,32,0x20,&old)||!FlushInstructionCache(GetCurrentProcess(),hd.relay,32))return;
 if(hd.init_thread!=GetCurrentThreadId() || !caller_authorized(caller) || !cold_roots() || !supported())return;
 AotStatus r=aot_apply(hd.base,0xfb5000,hd.patch,2);if(r==AOT_ROLLBACK_FAILED)uncertain();if(r==AOT_OK)hd.phase=2;
}
__declspec(dllexport) void InitializeASI(void){
#ifdef HDUI_HOST
 extern uintptr_t hdui_host_return_address(void);
 initialize_from(hdui_host_return_address());
#else
 initialize_from((uintptr_t)__builtin_return_address(0));
#endif
}
int __stdcall DllMain(void *self,unsigned long reason,void *reserved){(void)reserved;if(reason==1){hd.self=self;DisableThreadLibraryCalls(self);}return 1;}
