/* Original freestanding x64 deferred scene diagnostic v2. No shader, draw, timing or settings writes. */
#include <stdint.h>
#include "private_bytes.h"
#include <stddef.h>
#define API __declspec(dllimport)
API void *__stdcall GetModuleHandleW(const uint16_t *);
API int __stdcall GetModuleHandleExW(uint32_t,const uint16_t *,void **);
API void *__stdcall GetProcAddress(void *,const char *);
API uint32_t __stdcall GetModuleFileNameW(void *,uint16_t *,uint32_t);
API uint32_t __stdcall GetCurrentThreadId(void);
API uint64_t __stdcall GetTickCount64(void);
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
static uint8_t original_call[5]={AOT_PRIVATE_ORIGINAL_CALL};
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
static Composite composite;
static Enqueue enqueue;
static struct {Com *swap;unsigned sequence,mode,thread;} probe;
static void __stdcall on_probe(CallbackArgs *a,void *opaque){
    (void)opaque;AcquireSRWLockExclusive(&log_lock);
    if(a && a->context){unsigned type=((unsigned(__stdcall *)(Com *))a->context->v[112])(a->context);
        text("candidate=after_gameplay_composite pre_ui_verified=0 native_aa_draws=0 ");value("sequence",probe.sequence);value("producer_thread",probe.thread);value("callback_thread",GetCurrentThreadId());value("mode",probe.mode);value("context_type",type);hex("executor",(uintptr_t)a->executor);flush();
        if(type<=1)snapshot(a->context,probe.swap);else{text("snapshot=unknown_context_refused");flush();}
        phase_objects();
        FlushFileBuffers(log_handle);
    }
    ReleaseSRWLockExclusive(&log_lock);release(probe.swap);probe.swap=0;unclaim();
}
static void effects_observation(void){
    static uint32_t observation_thread;
    uint32_t thread=GetCurrentThreadId(),expected=0;
    (void)__atomic_compare_exchange_n(&observation_thread,&expected,thread,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);
    if(__atomic_load_n(&observation_thread,__ATOMIC_SEQ_CST)!=thread)return;
    static uint64_t start,last;
    static uint32_t previous_matrix[16];
    static unsigned sample;
    uint64_t now=GetTickCount64();
    if(!start)start=now;
    if(now-start>60000 || (last && now-last<250))return;
    last=now;
    uintptr_t owner=0;uint32_t selector=0,index=0,fields[0x140/4]={0};
    if(!read_at((uintptr_t)image+0xc1bb78,&owner,8)||!owner||
       !read_at(owner+0x40,&selector,4)||!read_at(owner+0x2d0,&index,4)||index>1||selector>1||
       !read_at(owner+(uintptr_t)index*0x140,fields,sizeof fields))return;
    AcquireSRWLockExclusive(&log_lock);
    text("effect_observation=producer_after_composite uploader_execution_verified=0 ");
    value("sample",++sample);value("elapsed_ms",now-start);value("selector",selector);value("state_index",index);
    value("viewproj_changed",sample==1||!equal(previous_matrix,fields+0x80/4,sizeof previous_matrix));flush();
    const unsigned offsets[]={0x50,0x54,0x58,0x5c,0x60,0x64,0x68,0x6c,0x70,0xe8,0xec,0x108,0x10c,0x110,0x114,0x118,0x11c};
    const char *names[]={"delta_time_bits","lightshaft_bits","dof_bits","motionblur_bits","fxaa_bits","autoexposure_bits","lensflare_bits","anamorphic_bits","gamma_bits","exposure_dial_bits","shutter_speed_bits","adapt_light_bits","adapt_dark_bits","fxaa_threshold_bits","fxaa_min_bits","fxaa_sharpness_bits","fxaa_range_bits"};
    for(unsigned k=0;k<sizeof offsets/sizeof offsets[0];k++)hex(names[k],fields[offsets[k]/4]);
    flush();ReleaseSRWLockExclusive(&log_lock);
    memcpy(previous_matrix,fields+0x80/4,sizeof previous_matrix);
}
static void __stdcall after_composite(void *self){
    composite(self);effects_observation();if(!claim(&pending))return;
    unsigned n=++attempts;if(n>3){unclaim();return;}
    if(!producer)producer=GetCurrentThreadId();
    if(producer!=GetCurrentThreadId()){message("probe_skipped=producer_thread_changed");unclaim();return;}
    uintptr_t manager=0,cursor=0,end=0,swap=0;uint8_t mode=0;
    if(!read_at((uintptr_t)image+0xc492c8,&manager,8)||!manager||!read_at(manager+0x2b08,&cursor,8)||!read_at(manager+0x2b00,&end,8)||
       !read_at(manager+0x2f98,&swap,8)||!swap||!read_at(manager+0x75,&mode,1)||end<cursor||end-cursor<20){message("probe_skipped=manager_or_capacity");unclaim();return;}
    MemoryRegion region={0};
    if(!VirtualQuery((void *)cursor,&region,sizeof region)||region.state!=0x1000||
       !(region.protect&(4u|8u|0x40u|0x80u))||(region.protect&(0x100u|1u))||
       (uintptr_t)region.base>cursor||cursor-(uintptr_t)region.base>region.size||
       region.size-(cursor-(uintptr_t)region.base)<20){message("probe_skipped=queue_not_writable");unclaim();return;}
    /* Producer writes use exactly the engine's current buffer and native emitter.
     * Stock cursor/end bounds and producer-thread identity are required. */
    probe.swap=(Com *)swap;probe.sequence=n;probe.mode=mode;probe.thread=GetCurrentThreadId();addref(probe.swap);
    enqueue((void *)manager,on_probe,&probe);
}
static int outside_lock(void){uint8_t *peb=0;uintptr_t lock=0,owner=0;__asm__("movq %%gs:0x60,%0":"=r"(peb));
    return peb && read_at((uintptr_t)peb+0x110,&lock,8)&&lock&&read_at(lock+0x10,&owner,8)&&owner!=(uintptr_t)GetCurrentThreadId();}
static int guard(void){
    static const uint8_t call[]={AOT_PRIVATE_CALL};
    static const uint8_t entry[]={AOT_PRIVATE_ENTRY};
    static const uint8_t emitter[]={AOT_PRIVATE_EMITTER};
    uint8_t bytes[19];uintptr_t handler=0;
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
static int write_call(const uint8_t *bytes,uint32_t *original_protection,int *changed){
    uint32_t old=0,ignored=0;void *site=image+0x117703;
    *changed=0;if(!VirtualProtect(site,5,0x40,&old))return 0;
    *original_protection=old;memcpy(site,bytes,5);*changed=1;
    int cache=FlushInstructionCache(GetCurrentProcess(),site,5);
    int protection=VirtualProtect(site,5,old,&ignored);return cache&&protection;
}
static int rollback_call(uint32_t original_protection){
    uint32_t ignored=0;void *site=image+0x117703;
    if(!VirtualProtect(site,5,0x40,&ignored))return 0;
    memcpy(site,original_call,5);
    int cache=FlushInstructionCache(GetCurrentProcess(),site,5);
    int protection=VirtualProtect(site,5,original_protection,&ignored);
    return cache&&protection&&equal(site,original_call,5);
}
__declspec(dllexport) void InitializeASI(void){
    static const uint16_t loader_name[]={'d','3','d','1','1','.','d','l','l',0};
    void *loader=GetModuleHandleW(loader_name),*self=0;
    /* Exact reviewed loader callout and cold initialization, never DllMain. */
    if(!outside_lock()||!loader||!GetProcAddress(loader,"IsUltimateASILoader")||
       (uintptr_t)__builtin_return_address(0)!=(uintptr_t)loader+0x202fe||!claim(&initialized))return;
    if(!GetModuleHandleExW(6,(const uint16_t *)(uintptr_t)&InitializeASI,&self))return;
    uint16_t path[280];uint32_t n=GetModuleFileNameW(self,path,274);if(!n||n>=274)return;
    path[n++]='.';path[n++]='l';path[n++]='o';path[n++]='g';path[n]=0;
    log_handle=CreateFileW(path,4,3,0,4,0x80,0);if(log_handle==(void *)(uintptr_t)-1)return;
    message("diagnostic_only=1 native_aa_draws=0 pre_ui_verified=0 bootstrap=reviewed_loader_callout diagnostic_version=3 deferred_getters=1 effects_observation_seconds=60 interval_ms=250");
    image=GetModuleHandleW(0);if(!checkpoint()){message("refused=build_guard_or_cold_checkpoint");return;}
    if(!GetModuleHandleExW(5,(const uint16_t *)(uintptr_t)&InitializeASI,&self)){message("refused=module_pin");return;}
    composite=(Composite)(image+0xeaba0);enqueue=(Enqueue)(image+0x461420);
    relay=near_relay((uintptr_t)image+0x117708);if(!relay){message("refused=near_allocation");return;}
    uint8_t *r=relay;r[0]=0xff;r[1]=0x25;for(unsigned i=2;i<6;i++)r[i]=0;uintptr_t target=(uintptr_t)&after_composite;memcpy(r+6,&target,8);
    uint32_t old=0;
    if(!VirtualProtect(relay,4096,0x20,&old)||!FlushInstructionCache(GetCurrentProcess(),relay,14)){VirtualFree(relay,0,0x8000);relay=0;message("refused=relay_protection_or_cache");return;}
    intptr_t delta=(intptr_t)relay-(intptr_t)(image+0x117708);
    if(delta<INT32_MIN||delta>INT32_MAX){VirtualFree(relay,0,0x8000);relay=0;message("refused=relay_range");return;}
    uint8_t patch[5]={0xe8};int32_t relative=(int32_t)delta;memcpy(patch+1,&relative,4);
    /* Globals remain cold, exact caller still unchanged, and loader lock absent. */
    if(!outside_lock()||!checkpoint()){VirtualFree(relay,0,0x8000);relay=0;message("refused=checkpoint_changed");return;}
    uint32_t original_protection=0;int changed=0;
    if(!write_call(patch,&original_protection,&changed)){
        if(changed && !rollback_call(original_protection)){message("fatal=uncertain_call_rollback_relay_retained");OutputDebugStringA("[aot-aa-probe] fatal rollback failure\n");TerminateProcess(GetCurrentProcess(),0xa074aa01);__builtin_trap();}
        VirtualFree(relay,0,0x8000);relay=0;message("refused=call_write_rolled_back");return;
    }
    message("diagnostic_ready=1 max_probe_attempts=3 native_aa_draws=0");
}
int __stdcall DllMain(void *instance,uint32_t reason,void *reserved){(void)reserved;if(reason==1)DisableThreadLibraryCalls(instance);return 1;}
