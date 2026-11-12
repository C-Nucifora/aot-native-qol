#include "aot_bridge.h"
#ifdef AOT_FPS_CANDIDATE
#include "aot_fps_stage.h"
#ifdef AOT_FPS_SESSION
#include "aot_fps_session.h"
#endif
#ifdef AOT_FPS_TRIAL
#include "aot_fps_trial.h"
#endif
#endif
#include "aot_startup.h"
#include "aot_owned_files.h"
#include "bridge_guards.h"
AOT_API unsigned long __stdcall GetCurrentThreadId(void);
AOT_API int __stdcall TerminateProcess(void *,unsigned int);
AOT_API unsigned long __stdcall GetModuleFileNameW(void *,uint16_t *,unsigned long);
AOT_API int __stdcall GetModuleHandleExW(unsigned long,const uint16_t *,void **);
AOT_API void * __stdcall GetProcAddress(void *,const char *);
static struct {
    uint8_t *base;
    void *code,*pin;
    AotPatch calls[5];
    uint16_t texture[1024],layout[1024];
    unsigned long thread;
    unsigned phase,hold_resolution,raw,have_raw;
} bridge;
unsigned aot_bridge_phase(void){return bridge.phase;}
static _Noreturn void fatal_uncertain(void){
    bridge.phase=5;
    /* Uncertain executable bytes/protection/cache state cannot safely run game
     * startup or DLL cleanup. Keep every owner and terminate this process now. */
    OutputDebugStringA("[aot-native] FATAL: uncertain patch rollback; stopping process before continuation\n");
    TerminateProcess(GetCurrentProcess(),0xa0740001u);
    /* Do not continue even if the OS termination request unexpectedly fails. */
#ifdef AOT_BRIDGE_HOST_CHECK
    __builtin_trap();
#else
    /* Windows x64 FAST_FAIL_FATAL_APP_EXIT; bypass normal exception cleanup. */
    __asm__ volatile("int $0x29"::"c"(7):"memory");
    __builtin_unreachable();
#endif
}
static const void *resolution_name(void){return *(void **)(bridge.base+0xa6c398);}
static void *resolution_type(void){return *(void **)(bridge.base+0xa6c518);}
static int same_thread(void){return GetCurrentThreadId()==bridge.thread;}
static int read_value(const void *name,void *type,unsigned *value){
    if(bridge.phase==5)fatal_uncertain();
    int result=((int (*)(const void *,void *,unsigned *))(bridge.base+0x41e180))(name,type,value);
    if(result && bridge.phase==2 && name==resolution_name()){
        bridge.raw=*value;bridge.have_raw=1;
    }
    return result;
}
static uintptr_t write_value(const void *name,void *type,unsigned value){
    if(bridge.phase==5)fatal_uncertain();
    if(bridge.hold_resolution && name==resolution_name())return 1;
    return ((uintptr_t (*)(const void *,void *,unsigned))(bridge.base+0x41e080))(name,type,value);
}
static uintptr_t explicit_write(const void *name,void *type,unsigned value){
    if(bridge.phase==5)fatal_uncertain();
    /* This call site belongs to stock graphics Apply/recovery commit. An
     * explicit settings commit may replace a key held from unsupported startup. */
    bridge.hold_resolution=0;
    return ((uintptr_t (*)(const void *,void *,unsigned))(bridge.base+0x41e080))(name,type,value);
}
static uintptr_t load_settings(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){
    if(bridge.phase==5)fatal_uncertain();
    if(bridge.phase==1 && same_thread())bridge.phase=2;
    return ((uintptr_t (*)(uintptr_t,uintptr_t,uintptr_t,uintptr_t))(bridge.base+0x41dca0))(a,b,c,d);
}
static int initialize_graphics(void){
    int (*original)(void)=(int (*)(void))(bridge.base+0x419600);
    if(bridge.phase==5)fatal_uncertain();
    if(bridge.phase!=2 || !same_thread()){bridge.phase=4;return original();}
    /* This call site is reached only after SteamAPI_Init returns true. Unlike
     * the native resolver's fallback0, a missing SteamUtils is UNKNOWN here. */
    void *(*utils)(void)=*(void *(**)(void))(bridge.base+0x8657e0);
    if(!utils || !utils()){bridge.phase=4;return original();}
    unsigned locale=((unsigned (*)(void))(bridge.base+0xdd5b0))();
    if(locale>8 || locale==2 || locale==4){bridge.phase=4;return original();}
    uint8_t old_locale=bridge.base[0xc7a480];
    /* An actual resolver result, never an inferred zero. Restore the native
     * sentinel afterwards; the original app initializer will store it itself. */
    bridge.base[0xc7a480]=(uint8_t)locale;
    AotStatus result=aot_startup_resolution_at_checkpoint(bridge.texture,bridge.layout);
    bridge.base[0xc7a480]=old_locale;
    if(result!=AOT_OK){
        if(aot_startup_phase()==4)fatal_uncertain();
        bridge.phase=4;return original();
    }
    unsigned *selection=(unsigned *)(bridge.base+0xc48d84),old=*selection;
    if(bridge.have_raw && bridge.raw>=11 && bridge.raw<13)*selection=bridge.raw;
    /* Normal initializer calls41a190->41a7c0, so it applies the final validated
     * index using stock window/device setup before any native menu exists. */
    int initialized=original();
    if(initialized<=0){
        *selection=old;
        if(aot_startup_rollback_at_checkpoint()!=AOT_OK)fatal_uncertain();
        bridge.phase=4;
        return initialized;
    }
    bridge.phase=3;bridge.hold_resolution=0;
    /* Other16 settings already followed the stock persistence path. */
    write_value(resolution_name(),resolution_type(),*selection);
    return initialized;
}
static int matches(const uint8_t *base,const AotPatch *p){
    for(unsigned i=0;i<p->length;i++)if(base[p->rva+i]!=p->expected[i])return 0;
    return 1;
}
static int asset_paths(void *self){
    unsigned long n=GetModuleFileNameW(self,bridge.texture,1024);
    if(!n || n>=1024)return 0;
    while(n && bridge.texture[n-1]!='\\' && bridge.texture[n-1]!='/')--n;
    static const char folder[]="aot-native-assets\\";
    static const char texture[]="UIOption.g1t",layout[]="UIOptionLayout.bin";
    if(!n || n+sizeof(folder)+sizeof(layout)>=1024)return 0;
    for(unsigned i=0;folder[i];i++)bridge.texture[n++]=(uint8_t)folder[i];
    for(unsigned i=0;i<n;i++)bridge.layout[i]=bridge.texture[i];
    for(unsigned i=0;i<sizeof(texture);i++)bridge.texture[n+i]=(uint8_t)texture[i];
    for(unsigned i=0;i<sizeof(layout);i++)bridge.layout[n+i]=(uint8_t)layout[i];
    return 1;
}
static int outside_loader_lock(void){
#if defined(AOT_BRIDGE_HOST_CHECK)
    extern int aot_check_outside_loader_lock(void);
    return aot_check_outside_loader_lock();
#else
    /* Supported x64 NT/Wine layout: TEB->PEB, PEB.LoaderLock, critical-section
     * OwningThread. Refuse null/unknown state and loader-callout initialization. */
    uint8_t *peb;__asm__("movq %%gs:0x60,%0":"=r"(peb));
    if(!peb)return 0;
    const uint8_t *lock=*(const uint8_t **)(peb+0x110);
    if(!lock)return 0;
    uintptr_t owner=*(const volatile uintptr_t *)(lock+0x10);
    return owner!=(uintptr_t)GetCurrentThreadId();
#endif
}
static AotStatus arm(void *self){
    if(bridge.phase || !outside_loader_lock() || !aot_supported_executable())return AOT_SIGNATURE_MISMATCH;
    uint8_t *base=GetModuleHandleW(0);
    if(!base || base[0]!='M' || base[1]!='Z' || *(uint32_t *)(base+0x3c)!=0x148 ||
       *(uint32_t *)(base+0x148+24+56)!=0xfb5000)return AOT_SIGNATURE_MISMATCH;
    /* Still before window/settings initialization and no UI resource owner. */
    if(*(void **)(base+0xc9d9c0) || *(void **)(base+0xc9d9c8) || *(void **)(base+0xc9cae8))return AOT_BAD_RANGE;
    for(size_t i=0;i<sizeof(bridge_preserved)/sizeof(bridge_preserved[0]);i++)
        if(!matches(base,&bridge_preserved[i]))return AOT_SIGNATURE_MISMATCH;
    for(unsigned i=0;i<5;i++)if(!matches(base,&bridge_calls[i]))return AOT_SIGNATURE_MISMATCH;
    if(!asset_paths(self))return AOT_BAD_RANGE;
    void *functions[5]={(void *)&load_settings,(void *)&read_value,(void *)&write_value,
        (void *)&initialize_graphics,(void *)&explicit_write};
    uintptr_t low=(uintptr_t)base+0x37cbe3,high=(uintptr_t)base+0x41e590;
    uint8_t *code=aot_allocate_near(low,high,5*16);
    if(!code)return AOT_BAD_RANGE;
    for(unsigned i=0;i<5;i++){
        uint8_t *p=code+i*16;p[0]=0xff;p[1]=0x25;for(unsigned j=2;j<6;j++)p[j]=0;
        uintptr_t target=(uintptr_t)functions[i];for(unsigned j=0;j<8;j++)p[6+j]=(uint8_t)(target>>(j*8));
        bridge.calls[i]=bridge_calls[i];int32_t delta;
        if(!aot_rel32((uintptr_t)base+bridge.calls[i].rva+5,(uintptr_t)p,&delta)){
            VirtualFree(code,0,AOT_MEM_RELEASE);return AOT_BAD_RANGE;
        }
        bridge.calls[i].replacement[0]=0xe8;
        for(unsigned j=0;j<4;j++)bridge.calls[i].replacement[1+j]=(uint8_t)((uint32_t)delta>>(j*8));
    }
    unsigned long old;
    if(!VirtualProtect(code,80,0x20,&old) || !FlushInstructionCache(GetCurrentProcess(),code,80)){
        VirtualFree(code,0,AOT_MEM_RELEASE);return AOT_PROTECTION_FAILED;
    }
    if(!GetModuleHandleExW(5,(const uint16_t *)(uintptr_t)&aot_bridge_phase,&bridge.pin)){
        VirtualFree(code,0,AOT_MEM_RELEASE);return AOT_PROTECTION_FAILED;
    }
    bridge.base=base;bridge.code=code;bridge.thread=GetCurrentThreadId();bridge.hold_resolution=1;bridge.phase=1;
    AotStatus result=aot_apply(base,0xfb5000,bridge.calls,5);
    if(result==AOT_ROLLBACK_FAILED)fatal_uncertain();
    else if(result!=AOT_OK){bridge.phase=0;bridge.code=0;VirtualFree(code,0,AOT_MEM_RELEASE);}
    return result;
}
#ifdef AOT_BRIDGE_HOST_CHECK
AotStatus aot_check_bridge_arm(void *self){return arm(self);}
#else
__declspec(dllexport) void InitializeASI(void){
    /* Only the reviewed archived loader ABI is supported. DllMain never calls
     * this. The loader-lock owner check precedes file IO and code mutation. */
    static const uint16_t loader_name[]={'d','3','d','1','1','.','d','l','l',0};
    void *loader=GetModuleHandleW(loader_name);
    if(!outside_loader_lock() || !loader || !GetProcAddress(loader,"IsUltimateASILoader"))return;
    if((uintptr_t)__builtin_return_address(0)!=(uintptr_t)loader+0x202fe)return;
    void *self=0;
    if(!GetModuleHandleExW(6,(const uint16_t *)(uintptr_t)&InitializeASI,&self))return;
    AotStatus result=arm(self);
    if(result==AOT_ROLLBACK_FAILED)fatal_uncertain();
    OutputDebugStringA(result==AOT_OK?"[aot-native] resolution bridge armed; locale pending\n":"[aot-native] bridge refused safely; no activation\n");
#ifdef AOT_FPS_CANDIDATE
    if(result==AOT_OK){
#ifdef AOT_FPS_SESSION
        if(aot_fps_session_start(self)!=AOT_OK){
            OutputDebugStringA("[aot-fps] session backend refused; no FPS menu publication; stock timing retained\n");
            return;
        }
#endif
#ifdef AOT_FPS_TRIAL
        AotStatus experiment=aot_fps_trial_start(self);
        if(experiment!=AOT_OK)
            OutputDebugStringA("[aot-fps-trial] explicit experiment refused; stock timing retained\n");
#endif
        AotStatus fps=aot_fps_stage_start(self);
        if(fps!=AOT_OK){
            OutputDebugStringA("[aot-fps] bootstrap refused; resolution startup continues\n");
            (void)aot_fps_stage_remove();
        }
    }
#endif
}
#endif
