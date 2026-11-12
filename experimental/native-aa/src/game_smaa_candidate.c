/* Original opt-in native SMAA prototype; stock timing and settings unchanged. */
#include "aa_native.h"
int _fltused;
void *_AddressOfReturnAddress(void);
#pragma intrinsic(_AddressOfReturnAddress)
#include <stdint.h>
#include "private_bytes.h"
#include <stddef.h>
#define API __declspec(dllimport)
API void *__stdcall GetModuleHandleW(const uint16_t *);
API int __stdcall GetModuleHandleExW(uint32_t,const uint16_t *,void **);
API void *__stdcall GetProcAddress(void *,const char *);
API uint32_t __stdcall GetModuleFileNameW(void *,uint16_t *,uint32_t);
API uint32_t __stdcall GetCurrentThreadId(void);
API uint32_t __stdcall GetPrivateProfileIntW(const uint16_t *,const uint16_t *,int32_t,const uint16_t *);
API uint32_t __stdcall GetEnvironmentVariableW(const uint16_t *,uint16_t *,uint32_t);
API void *__stdcall GetCurrentProcess(void);
API int __stdcall ReadProcessMemory(void *,const void *,void *,size_t,size_t *);
API size_t __stdcall VirtualQuery(const void *,void *,size_t);
API int __stdcall VirtualProtect(void *,size_t,uint32_t,uint32_t *);
API void *__stdcall VirtualAlloc(void *,size_t,uint32_t,uint32_t);
API int __stdcall VirtualFree(void *,size_t,uint32_t);
API int __stdcall FlushInstructionCache(void *,const void *,size_t);
API void __stdcall AcquireSRWLockExclusive(void *);
API void __stdcall ReleaseSRWLockExclusive(void *);
API void *__stdcall CreateFileW(const uint16_t *,uint32_t,uint32_t,void *,uint32_t,uint32_t,void *);
API int __stdcall WriteFile(void *,const void *,uint32_t,uint32_t *,void *);
API int __stdcall FlushFileBuffers(void *);
API int __stdcall DisableThreadLibraryCalls(void *);
API void __stdcall OutputDebugStringA(const char *);
API int __stdcall TerminateProcess(void *,uint32_t);

void *memcpy(void *d,const void *s,size_t n){uint8_t *a=d;const uint8_t *b=s;for(size_t i=0;i<n;i++)a[i]=b[i];return d;}
void *memset(void *d,int c,size_t n){uint8_t *a=d;for(size_t i=0;i<n;i++)a[i]=(uint8_t)c;return d;}
static int equal(const void *a,const void *b,size_t n){const uint8_t *x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return 0;return 1;}
typedef struct {uint32_t a;uint16_t b,c;uint8_t d[8];} Guid;
typedef struct {void **v;} Com;
static const Guid iid_texture={0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};
static const Guid iid_device={0xdb6f6ddb,0xac77,0x4e88,{0x82,0x53,0x81,0x9d,0xf9,0xbb,0xf1,0x40}};
typedef struct {uint32_t width,height,mips,array,format,samples,quality,usage,bind,cpu,misc;} TextureDesc;
typedef struct {uint32_t width,height,numerator,denominator,format,scanline,scaling,samples,quality,usage,buffers;void *window;int32_t windowed;uint32_t effect,flags;} SwapDesc;
typedef struct {uint32_t format,dimension,data[3];} ViewDesc;
typedef struct {float x,y,width,height,minimum,maximum;} Viewport;
typedef struct {void *base,*allocation;uint32_t allocation_protect,pad;size_t size;uint32_t state,protect,type,pad2;} MemoryRegion;
_Static_assert(sizeof(MemoryRegion)==48,"memory-query ABI");
_Static_assert(sizeof(void *)==8,"x64 only");
_Static_assert(sizeof(TextureDesc)==44,"texture descriptor ABI");
_Static_assert(sizeof(SwapDesc)==72 && offsetof(SwapDesc,window)==48,"swapchain descriptor ABI");
_Static_assert(sizeof(ViewDesc)==20 && sizeof(Viewport)==24,"view ABI");
static void release(Com *p){if(p)((uint32_t(__stdcall *)(Com *))p->v[2])(p);}
static void addref(Com *p){((uint32_t(__stdcall *)(Com *))p->v[1])(p);}
static int32_t query(Com *p,const Guid *g,Com **out){return ((int32_t(__stdcall *)(Com *,const Guid *,void **))p->v[0])(p,g,(void **)out);}
static void *log_handle=(void *)(uintptr_t)-1,*log_lock,*relay;
static uint8_t *image;
static int32_t initialized,pending;
static unsigned attempts,producer;
static unsigned native_enabled,draws,refusals,scope_active;
static struct {char bytes[4096];size_t used;} line;
static void text(const char *s){for(size_t i=0;s[i] && line.used<sizeof(line.bytes);i++)line.bytes[line.used++]=s[i];}
static void number(uint64_t n,unsigned base){char temp[24];unsigned k=0;do{temp[k++]="0123456789abcdef"[n%base];n/=base;}while(n && k<sizeof temp);while(k && line.used<sizeof(line.bytes))line.bytes[line.used++]=temp[--k];}
static void value(const char *name,uint64_t n){text(name);text("=");number(n,10);text(" ");}
static void hex(const char *name,uint64_t n){text(name);text("=0x");number(n,16);text(" ");}
static void flush(void){uint32_t written=0;text("\r\n");if(log_handle!=(void *)(uintptr_t)-1)(void)WriteFile(log_handle,line.bytes,(uint32_t)line.used,&written,0);line.used=0;}
static void message(const char *s){AcquireSRWLockExclusive(&log_lock);text(s);flush();if(log_handle!=(void *)(uintptr_t)-1)FlushFileBuffers(log_handle);ReleaseSRWLockExclusive(&log_lock);}
static int read_at(uintptr_t at,void *out,size_t n){size_t got=0;return ReadProcessMemory(GetCurrentProcess(),(void *)at,out,n,&got)&&got==n;}
static void phase_objects(void){
    uintptr_t list=0;
    if(!read_at((uintptr_t)image+0xc20b38,&list,8)||!list){text("phase_objects=unavailable");flush();return;}
    for(unsigned i=0;i<4;i++){
        uintptr_t object=0,table=0,method=0;
        if(!read_at(list+i*16,&object,8)||!object||!read_at(object,&table,8))continue;
        value("phase_object",i);hex("object",object);hex("vtable",table);
        const unsigned offsets[]={0x40,0x48,0x50,0x58};
        const char *names[]={"method40","method48","method50","method58"};
        for(unsigned k=0;k<4;k++)if(read_at(table+offsets[k],&method,8))hex(names[k],method);
        flush();
    }
}
static int claim(int32_t *p){int32_t expected=0;return __atomic_compare_exchange_n(p,&expected,1,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);}
static void unclaim(void){__atomic_store_n(&pending,0,__ATOMIC_SEQ_CST);}
static void describe(const char *name,Com *resource){
    Com *texture=0;if(!resource || query(resource,&iid_texture,&texture)<0){text(name);text("=not_texture2d");flush();return;}
    TextureDesc d={0};((void(__stdcall *)(Com *,TextureDesc *))texture->v[10])(texture,&d);
    text(name);text(" ");hex("resource_identity",(uintptr_t)texture);value("width",d.width);value("height",d.height);value("format",d.format);value("samples",d.samples);value("quality",d.quality);value("mips",d.mips);value("array",d.array);value("bind",d.bind);flush();release(texture);
}
static void snapshot(Com *context,Com *swap){
    Com *device=0,*swap_device=0;
    ((void(__stdcall *)(Com *,Com **))context->v[3])(context,&device);
    int32_t hr=((int32_t(__stdcall *)(Com *,const Guid *,void **))swap->v[7])(swap,&iid_device,(void **)&swap_device);
    int same=hr>=0 && device && device==swap_device;value("same_device",(unsigned)same);flush();release(device);release(swap_device);if(!same)return;
    SwapDesc d={0};hr=((int32_t(__stdcall *)(Com *,SwapDesc *))swap->v[12])(swap,&d);
    if(hr>=0){text("swapchain ");value("width",d.width);value("height",d.height);value("format",d.format);value("samples",d.samples);value("windowed",(unsigned)d.windowed);value("buffers",d.buffers);flush();}
    else{hex("swapchain_error",(uint32_t)hr);flush();}
    Com *buffer=0;hr=((int32_t(__stdcall *)(Com *,uint32_t,const Guid *,void **))swap->v[9])(swap,0,&iid_texture,(void **)&buffer);
    if(hr>=0){describe("backbuffer",buffer);release(buffer);}else{hex("backbuffer_error",(uint32_t)hr);flush();}
    Com *targets[8]={0},*depth=0;
    ((void(__stdcall *)(Com *,uint32_t,Com **,Com **))context->v[89])(context,8,targets,&depth);
    for(unsigned i=0;i<8;i++)if(targets[i]){
        ViewDesc vd={0};((void(__stdcall *)(Com *,ViewDesc *))targets[i]->v[8])(targets[i],&vd);
        value("rtv",i);value("view_format",vd.format);value("dimension",vd.dimension);flush();
        Com *resource=0;((void(__stdcall *)(Com *,Com **))targets[i]->v[7])(targets[i],&resource);
        describe("rtv_resource",resource);release(resource);release(targets[i]);
    }
    if(depth){Com *resource=0;((void(__stdcall *)(Com *,Com **))depth->v[7])(depth,&resource);describe("depth",resource);release(resource);release(depth);}
    Viewport viewport[16]={0};uint32_t count=16;
    ((void(__stdcall *)(Com *,uint32_t *,Viewport *))context->v[95])(context,&count,viewport);
    if(count>16)count=16;
    for(unsigned i=0;i<count;i++){uint32_t bits[6];memcpy(bits,&viewport[i],sizeof bits);value("viewport",i);hex("x_float_bits",bits[0]);hex("y_float_bits",bits[1]);hex("width_float_bits",bits[2]);hex("height_float_bits",bits[3]);flush();}
}
typedef struct {Com *context;void *executor;} CallbackArgs;
typedef void(__stdcall *Callback)(CallbackArgs *,void *);
typedef void(__stdcall *Enqueue)(void *,Callback,void *);
typedef void(__stdcall *Composite)(void *);
typedef void(__stdcall *PostEffects)(int,void *);
static PostEffects post_effects;
static Composite composite;
static Enqueue enqueue;
static struct {Com *swap;unsigned sequence,mode,thread;uintptr_t marker;} probe;
/* All gate reads occur on the original CPU producer before queue submission.
 * The stock return is2ddeff: scene virtual+30 in the measured normal frame. */
/*3f6e70 reserves8+18*24 bytes for empty native bucket-list nodes.
 *440 is the empty allocator cursor, not an earlier UI draw count. */
static int empty_ui_arena(void){
    struct {uint32_t count,pad;struct {uintptr_t table,next,previous;} node[18];} arena;
    _Static_assert(sizeof arena==440,"native empty arena layout");
    uint32_t cursor=0;uintptr_t head=0,start=(uintptr_t)image+0xc20d38;
    if(!read_at((uintptr_t)image+0xc20c2c,&cursor,4)||cursor!=sizeof arena||
       !read_at((uintptr_t)image+0xc48d38,&head,8)||head!=start||
       !read_at((uintptr_t)image+0xc20d30,&arena,sizeof arena)||arena.count!=18)return 0;
    for(unsigned i=0;i<18;i++)if(arena.node[i].table!=(uintptr_t)image+0xa01e80||
       arena.node[i].next!=(i<17?start+(i+1)*24:0)||
       arena.node[i].previous!=(i?start+(i-1)*24:0))return 0;
    return 1;
}
static int scene_gate(uintptr_t stock_return){
    if(stock_return!=(uintptr_t)image+0x2ddeff)return 0;
    uint32_t mode=0,index=0,type=0,blocked=0;
    uintptr_t scene=0,table=0,screens=0,object=0;
    if(!read_at((uintptr_t)image+0xc9c640,&mode,4)||mode!=3||
       !read_at((uintptr_t)image+0xc9c6a0,&index,4)||index>9||
       !read_at((uintptr_t)image+0xc9c650+index*8,&scene,8)||!scene||
       !read_at(scene,&table,8)||table!=(uintptr_t)image+0x9f2608||
       !read_at(scene+0x18,&type,4)||type!=10||!read_at(scene+0xe4,&blocked,4)||blocked||
       !read_at((uintptr_t)image+0xc9c704,&blocked,1)||(blocked&2)||
       !empty_ui_arena()||
       !read_at((uintptr_t)image+0xc20b38,&screens,8)||!screens)return 0;
    for(unsigned i=0;i<4;i++){
        if(!read_at(screens+i*16,&object,8))return 0;
        if(object && (i!=0 || !read_at(object,&table,8)||table!=(uintptr_t)image+0x9feac8))return 0;
    }
    return 1;
}
/* Return the bound scene target as an owned reference only after SRGB view,
 * backbuffer-sized dimensions and full-scene rectangle validation. The early
 * scene target may be offscreen; it must never be guessed from window size. */
static Com *approved_target(Com *context,Com *swap){
    Com *targets[8]={0},*depth=0,*buffer=0,*resource=0,*texture=0,*out=0;
    ((void(__stdcall *)(Com *,uint32_t,Com **,Com **))context->v[89])(context,8,targets,&depth);
    int single=targets[0]!=0;for(unsigned i=1;i<8;i++)if(targets[i])single=0;
    if(!single)goto done;
    ViewDesc v={0};((void(__stdcall *)(Com *,ViewDesc *))targets[0]->v[8])(targets[0],&v);
    if((v.format!=29&&v.format!=91)||v.dimension!=4||v.data[0])goto done;
    ((void(__stdcall *)(Com *,Com **))targets[0]->v[7])(targets[0],&resource);
    if(!resource||query(resource,&iid_texture,&texture)<0)goto done;
    if(((int32_t(__stdcall *)(Com *,uint32_t,const Guid *,void **))swap->v[9])(swap,0,&iid_texture,(void **)&buffer)<0)goto done;
    TextureDesc d={0};((void(__stdcall *)(Com *,TextureDesc *))texture->v[10])(texture,&d);
    TextureDesc back={0};((void(__stdcall *)(Com *,TextureDesc *))buffer->v[10])(buffer,&back);
    int matching_family=v.format==29?(d.format==27||d.format==29):(d.format==90||d.format==91);
    if(d.width!=back.width||d.height!=back.height||back.format!=91||back.samples!=1||!matching_family||d.samples!=1||d.quality||d.mips!=1||d.array!=1||d.width<128||d.width>8192||d.height<128||d.height>8192)goto done;
    Viewport vp[16]={0};uint32_t count=16;((void(__stdcall *)(Com *,uint32_t *,Viewport *))context->v[95])(context,&count,vp);
    if(count!=1||vp[0].x!=0||vp[0].y!=0||vp[0].width!=(float)d.width||vp[0].height!=(float)d.height||vp[0].minimum!=0||vp[0].maximum!=1)goto done;
    out=targets[0];targets[0]=0;
done:
    for(unsigned i=0;i<8;i++)release(targets[i]);
    release(depth);release(texture);release(resource);release(buffer);return out;
}
static void __stdcall on_probe(CallbackArgs *a,void *opaque){
    (void)opaque;int result=100;unsigned context_type=~0u;
    if(a&&a->context)context_type=((unsigned(__stdcall *)(Com *))a->context->v[112])(a->context);
    if(context_type==1){
        Com *target=approved_target(a->context,probe.swap);
        if(target){result=AotAaRecordDeferred(a->context,target,1,1);release(target);}
    }
    if(result==0)++draws;else ++refusals;
    if(probe.sequence<=3 || (result!=0 && refusals<=3)){
        AcquireSRWLockExclusive(&log_lock);text("native_smaa_callback ");value("sequence",probe.sequence);value("context_type",context_type);hex("marker_return_rva",probe.marker);value("result",(unsigned)result);value("lists_merged",draws);value("refusals",refusals);value("cas",0);flush();
        if(a && a->context && probe.sequence<=3){snapshot(a->context,probe.swap);phase_objects();}
        ReleaseSRWLockExclusive(&log_lock);
    }
    release(probe.swap);probe.swap=0;unclaim();
}
static void log_scene_refusal(uintptr_t stock_return,uint32_t before_batch,int stack_ok){
    static unsigned logged;
    if(__atomic_fetch_add(&logged,1,__ATOMIC_RELAXED)>=3)return;
    uint32_t mode=99,index=99,batch=99,selector=99,gamma=99;uintptr_t owner=0;
    (void)read_at((uintptr_t)image+0xc9c640,&mode,4);(void)read_at((uintptr_t)image+0xc9c6a0,&index,4);
    (void)read_at((uintptr_t)image+0xc20c2c,&batch,4);
    if(read_at((uintptr_t)image+0xc1bb78,&owner,8)&&owner){(void)read_at(owner+0x40,&selector,4);(void)read_at(owner+0x70,&gamma,4);}
    AcquireSRWLockExclusive(&log_lock);text("native_smaa_scene_refused ");value("stack_read",(unsigned)stack_ok);hex("scene_return",stock_return);value("stack_mode",mode);value("scene_index",index);value("batch_before",before_batch);value("batch_after",batch);value("selector",selector);value("gamma",gamma);flush();ReleaseSRWLockExclusive(&log_lock);
}
static void __stdcall before_effects(int phase,void *viewport){
    if(__atomic_load_n(&producer,__ATOMIC_SEQ_CST)!=GetCurrentThreadId()){post_effects(phase,viewport);return;}
    uintptr_t origin=(uintptr_t)__builtin_return_address(0);
    int marker=origin==(uintptr_t)image+0xeae9a || origin==(uintptr_t)image+0xeadec;
    uintptr_t owner=0;uint32_t gamma=1,selector=99,index=99,batch=1;
    int encoding=read_at((uintptr_t)image+0xc1bb78,&owner,8)&&owner&&
       read_at(owner+0x40,&selector,4)&&selector<=1&&read_at(owner+0x2d0,&index,4)&&index<=1&&
       read_at(owner+(uintptr_t)index*0x140+0x70,&gamma,4)&&gamma==0;
    (void)read_at((uintptr_t)image+0xc20c2c,&batch,4);
    int no_earlier_ui=empty_ui_arena();
    if(native_enabled&&scope_active&&marker&&phase==0&&(!encoding||!no_earlier_ui)){
        static unsigned inner_refusals;
        if(inner_refusals++<3){AcquireSRWLockExclusive(&log_lock);text("native_smaa_marker_refused ");hex("marker_return_rva",origin-(uintptr_t)image);value("encoding_guard",(unsigned)encoding);value("selector",selector);value("state_index",index);value("gamma_selected",gamma);value("ui_batch_bytes",batch);flush();ReleaseSRWLockExclusive(&log_lock);}
    }
    if(native_enabled&&scope_active&&marker&&phase==0&&producer==GetCurrentThreadId()&&encoding&&no_earlier_ui&&claim(&pending)){
        uintptr_t manager=0,cursor=0,end=0,swap=0;uint8_t mode=1;
        int capacity=read_at((uintptr_t)image+0xc492c8,&manager,8)&&manager&&read_at(manager+0x2b08,&cursor,8)&&read_at(manager+0x2b00,&end,8)&&
          read_at(manager+0x2f98,&swap,8)&&swap&&read_at(manager+0x75,&mode,1)&&mode==0&&end>=cursor&&end-cursor>=20;
        MemoryRegion region={0};
        int writable=capacity&&VirtualQuery((void *)cursor,&region,sizeof region)&&region.state==0x1000&&
          (region.protect&(4u|8u|0x40u|0x80u))&&!(region.protect&(0x100u|1u))&&
          (uintptr_t)region.base<=cursor&&cursor-(uintptr_t)region.base<=region.size&&region.size-(cursor-(uintptr_t)region.base)>=20;
        if(writable){probe.swap=(Com *)swap;probe.sequence=++attempts;probe.mode=mode;probe.thread=producer;probe.marker=origin-(uintptr_t)image;addref(probe.swap);enqueue((void *)manager,on_probe,&probe);}
        else unclaim();
    }
    /* Exact stock effects owner receives original args after the AA marker.
     * Its numerical overlays and all later screen/HUD draws remain outside AA. */
    post_effects(phase,viewport);
}
static void __stdcall after_composite(void *self){
    uintptr_t stock_return=0;
    int stack_ok=read_at((uintptr_t)_AddressOfReturnAddress()+0xa0,&stock_return,8);
    uint32_t before_batch=1;(void)read_at((uintptr_t)image+0xc20c2c,&before_batch,4);
    unsigned thread=GetCurrentThreadId(),expected=0;
    (void)__atomic_compare_exchange_n(&producer,&expected,thread,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);
    if(__atomic_load_n(&producer,__ATOMIC_SEQ_CST)!=thread){composite(self);return;}
    unsigned previous_scope=scope_active;
    scope_active=native_enabled&&producer==GetCurrentThreadId()&&!previous_scope&&stack_ok&&scene_gate(stock_return);
    if(native_enabled&&!scope_active)log_scene_refusal(stock_return,before_batch,stack_ok);
    composite(self);scope_active=previous_scope;
}
static int outside_lock(void){uint8_t *peb=0;uintptr_t lock=0,owner=0;__asm__("movq %%gs:0x60,%0":"=r"(peb));
    return peb && read_at((uintptr_t)peb+0x110,&lock,8)&&lock&&read_at(lock+0x10,&owner,8)&&owner!=(uintptr_t)GetCurrentThreadId();}
#include "arena_private_guards.h"
static int guard(void){
    uint8_t arena_code[sizeof arena_reset];
    if(!read_at((uintptr_t)image+0x3f6e70,arena_code,sizeof arena_reset)||!equal(arena_code,arena_reset,sizeof arena_reset)||
       !read_at((uintptr_t)image+0x3f76c0,arena_code,sizeof arena_node_init)||!equal(arena_code,arena_node_init,sizeof arena_node_init))return 0;
    static const uint8_t main_call[]={AOT_PRIVATE_MAIN_CALL},fallback_call[]={AOT_PRIVATE_FALLBACK_CALL};
    static const uint8_t frame[]={AOT_PRIVATE_FRAME};
    static const uint8_t call[]={AOT_PRIVATE_CALL};
    static const uint8_t entry[]={AOT_PRIVATE_ENTRY};
    static const uint8_t emitter[]={AOT_PRIVATE_EMITTER};
    uint8_t bytes[19];uintptr_t handler=0;
    if(!read_at((uintptr_t)image+0xeae95,bytes,5)||!equal(bytes,main_call,5)||
       !read_at((uintptr_t)image+0xeade7,bytes,5)||!equal(bytes,fallback_call,5))return 0;
    static const uint8_t phase[]={AOT_PRIVATE_PHASE};
    if(!read_at((uintptr_t)image+0x2ddefc,bytes,sizeof phase)||!equal(bytes,phase,sizeof phase))return 0;
    if(!read_at((uintptr_t)image+0x9f2638,&handler,8)||handler!=(uintptr_t)image+0x2cc830||
       !read_at((uintptr_t)image+0x9f2640,&handler,8)||handler!=(uintptr_t)image+0x2cc850)return 0;
    if(!read_at((uintptr_t)image+0x1176f0,bytes,sizeof frame)||!equal(bytes,frame,sizeof frame))return 0;
    if(!read_at((uintptr_t)image+0x1176fc,bytes,sizeof call)||!equal(bytes,call,sizeof call))return 0;
    if(!read_at((uintptr_t)image+0xeaba0,bytes,sizeof entry)||!equal(bytes,entry,sizeof entry))return 0;
    if(!read_at((uintptr_t)image+0x461420,bytes,sizeof emitter)||!equal(bytes,emitter,sizeof emitter))return 0;
    return read_at((uintptr_t)image+0x8961a0+0x56*8,&handler,8)&&handler==(uintptr_t)image+0x463a20;
}
static int checkpoint(void){
    uint32_t pe=0,stamp=0,size=0,signature=0;uint16_t machine=0;uintptr_t zero;
    if(!image||!read_at((uintptr_t)image+0x3c,&pe,4)||pe!=0x148||!read_at((uintptr_t)image+pe,&signature,4)||signature!=0x4550||
       !read_at((uintptr_t)image+pe+4,&machine,2)||machine!=0x8664||!read_at((uintptr_t)image+pe+8,&stamp,4)||stamp!=0x57ea28d6||
       !read_at((uintptr_t)image+pe+24+56,&size,4)||size!=0xfb5000)return 0;
    const uint32_t globals[]={0xc492c8,0xc9d9c0,0xc9d9c8,0xc9cae8};
    for(unsigned i=0;i<4;i++)if(!read_at((uintptr_t)image+globals[i],&zero,8)||zero)return 0;
    return guard();
}
static void *near_relay(uintptr_t origin){uintptr_t step=0x10000,start=origin>INT32_MAX?origin-INT32_MAX:step;
    start=(start+step-1)&~(step-1);uintptr_t end=origin+INT32_MAX-4096;
    for(uintptr_t p=start;p<=end;p+=step){void *memory=VirtualAlloc((void *)p,4096,0x3000,4);if(memory)return memory;if(end-p<step)break;}return 0;}
typedef struct {uint32_t rva;uint8_t original[5];unsigned relay_offset;uint32_t protection;int changed;} Hook;
static Hook hooks[]={
    {0x117703,{AOT_PRIVATE_HOOK_OUTER},0,0,0},
    {0xeae95,{AOT_PRIVATE_MAIN_CALL},32,0,0},
    {0xeade7,{AOT_PRIVATE_FALLBACK_CALL},32,0,0}
};
static int write_hook(Hook *h,const uint8_t *bytes){
    uint32_t ignored=0;void *site=image+h->rva;h->changed=0;
    if(!VirtualProtect(site,5,0x40,&h->protection))return 0;
    memcpy(site,bytes,5);h->changed=1;
    int cache=FlushInstructionCache(GetCurrentProcess(),site,5);
    int protection=VirtualProtect(site,5,h->protection,&ignored);return cache&&protection;
}
static int rollback_hook(Hook *h){
    uint32_t ignored=0;void *site=image+h->rva;
    if(!VirtualProtect(site,5,0x40,&ignored))return 0;
    memcpy(site,h->original,5);
    int cache=FlushInstructionCache(GetCurrentProcess(),site,5);
    int protection=VirtualProtect(site,5,h->protection,&ignored);
    return cache&&protection&&equal(site,h->original,5);
}
__declspec(dllexport) void InitializeASI(void){
    static const uint16_t loader_name[]={'d','3','d','1','1','.','d','l','l',0};
    void *loader=GetModuleHandleW(loader_name),*self=0;
    /* Exact reviewed loader callout and cold initialization, never DllMain. */
    if(!outside_lock()||!loader||!GetProcAddress(loader,"IsUltimateASILoader")||
       (uintptr_t)__builtin_return_address(0)!=(uintptr_t)loader+0x202fe||!claim(&initialized))return;
    if(!GetModuleHandleExW(6,(const uint16_t *)(uintptr_t)&InitializeASI,&self))return;
    uint16_t path[280],ini[280];uint32_t n=GetModuleFileNameW(self,path,274);if(!n||n>=274)return;
    memcpy(ini,path,n*2);ini[n]='.';ini[n+1]='i';ini[n+2]='n';ini[n+3]='i';ini[n+4]=0;
    static const uint16_t section[]={'N','a','t','i','v','e','S','M','A','A',0},key[]={'E','n','a','b','l','e','d',0};
    native_enabled=GetPrivateProfileIntW(section,key,0,ini)==1;
    path[n++]='.';path[n++]='l';path[n++]='o';path[n++]='g';path[n]=0;
    log_handle=CreateFileW(path,4,3,0,4,0x80,0);if(log_handle==(void *)(uintptr_t)-1)return;
    message("prototype=native_smaa scene=10 normal_phase_only=1 cas=0 bootstrap=reviewed_loader_callout");
    if(!native_enabled){message("native_smaa_disabled=ini_default_off no_hook_installed=1");return;}
    static const uint16_t basalt[]={'E','N','A','B','L','E','_','V','K','B','A','S','A','L','T',0};uint16_t enabled_layer[8]={0};
    uint32_t layer_n=GetEnvironmentVariableW(basalt,enabled_layer,8);
    if(layer_n && !(layer_n==1 && enabled_layer[0]=='0')){message("native_smaa_refused=vkbasalt_enabled no_hook_installed=1");return;}
    image=GetModuleHandleW(0);if(!checkpoint()){message("refused=build_guard_or_cold_checkpoint");return;}
    if(!GetModuleHandleExW(5,(const uint16_t *)(uintptr_t)&InitializeASI,&self)){message("refused=module_pin");return;}
    composite=(Composite)(image+0xeaba0);post_effects=(PostEffects)(image+0x32d4c0);enqueue=(Enqueue)(image+0x461420);
    relay=near_relay((uintptr_t)image+0x117708);if(!relay){message("refused=near_allocation");return;}
    for(unsigned i=0;i<2;i++){
        uint8_t *r=(uint8_t *)relay+i*32;r[0]=0xff;r[1]=0x25;for(unsigned j=2;j<6;j++)r[j]=0;
        uintptr_t target=i?(uintptr_t)&before_effects:(uintptr_t)&after_composite;memcpy(r+6,&target,8);
    }
    uint32_t old=0;
    if(!VirtualProtect(relay,4096,0x20,&old)||!FlushInstructionCache(GetCurrentProcess(),relay,46)){VirtualFree(relay,0,0x8000);relay=0;message("refused=relay_protection_or_cache");return;}
    uint8_t patches[3][5]={{0xe8},{0xe8},{0xe8}};
    for(unsigned i=0;i<3;i++){
        intptr_t delta=(intptr_t)relay+hooks[i].relay_offset-(intptr_t)(image+hooks[i].rva+5);
        if(delta<INT32_MIN||delta>INT32_MAX){VirtualFree(relay,0,0x8000);relay=0;message("refused=relay_range");return;}
        int32_t relative=(int32_t)delta;memcpy(patches[i]+1,&relative,4);
    }
    if(!outside_lock()||!checkpoint()){VirtualFree(relay,0,0x8000);relay=0;message("refused=checkpoint_changed");return;}
    for(unsigned i=0;i<3;i++)if(!write_hook(&hooks[i],patches[i])){
        int rollback=1;for(unsigned j=i+1;j>0;j--)if(hooks[j-1].changed&&!rollback_hook(&hooks[j-1]))rollback=0;
        if(!rollback){message("fatal=uncertain_call_rollback_relay_retained");TerminateProcess(GetCurrentProcess(),0xa074aa01);__builtin_trap();}
        VirtualFree(relay,0,0x8000);relay=0;message("refused=call_write_rolled_back");return;
    }
    message("native_smaa_ready=1 opt_in=1 scene10_guard=1 insertion=before_effects_numeric_hud target=full_srgb29_or91 single_sample=1 cas=0");
}
int __stdcall DllMain(void *instance,uint32_t reason,void *reserved){(void)reserved;if(reason==1)DisableThreadLibraryCalls(instance);return 1;}
