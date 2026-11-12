#include "aot_mod.h"
#include "private_guards.h"
AOT_API unsigned long __stdcall GetCurrentThreadId(void);
AOT_API int __stdcall GetModuleHandleExW(unsigned long,const uint16_t *,void **);
AOT_API void * __stdcall GetProcAddress(void *,const char *);
AOT_API unsigned long __stdcall GetEnvironmentVariableW(const uint16_t *,uint16_t *,unsigned long);
AOT_API int __stdcall TerminateProcess(void *,unsigned int);
typedef int (__stdcall *ShowFn)(void *,int);
typedef int (__stdcall *VisibleFn)(void *);
static struct {
    uint8_t *base,*code;
    void *pin;
    ShowFn show;
    VisibleFn visible;
    AotPatch patch;
    unsigned phase;
    unsigned long thread;
} state;
static int equal(const uint8_t *a,const uint8_t *b,size_t n) {
    for(size_t i=0;i<n;i++)if(a[i]!=b[i])return 0;
    return 1;
}
static int outside_lock(void) {
#ifdef FOCUS_HOST_CHECK
    extern int focus_host_outside_lock(void);
    return focus_host_outside_lock();
#else
    uint8_t *peb;__asm__("movq %%gs:0x60,%0":"=r"(peb));
    if(!peb)return 0;
    const uint8_t *lock=*(const uint8_t **)(peb+0x110);
    return lock && !*(const volatile uintptr_t *)(lock+0x10);
#endif
}
static _Noreturn void uncertain(void) {
    state.phase=5;
    OutputDebugStringA("[aot-focus] uncertain publication rollback; retaining pin/code and stopping startup\n");
    TerminateProcess(GetCurrentProcess(),0xa074f001u);
    __builtin_trap();
}
#ifndef FOCUS_HOST_CHECK
static int enabled(void) {
    static const uint16_t name[]={'A','O','T','_','F','O','C','U','S','_','K','E','E','P','_','W','I','N','D','O','W',0};
    uint16_t value[4]={0};
    unsigned long n=GetEnvironmentVariableW(name,value,4);
    if(!n)return 1; /* Installing this separate ASI opts in. */
    return n==1 && value[0]=='1';
}
#endif
static int supported(uint8_t *b) {
    if(!b || b[0]!='M' || b[1]!='Z' || *(uint32_t *)(b+0x3c)!=0x148)return 0;
    uint8_t *pe=b+0x148;
    if(*(uint32_t *)pe!=0x4550 || *(uint16_t *)(pe+4)!=0x8664 ||
       *(uint16_t *)(pe+6)!=9 || *(uint32_t *)(pe+8)!=0x57ea28d6 ||
       *(uint16_t *)(pe+24)!=0x20b || *(uint32_t *)(pe+40)!=0xf91310 ||
       *(uint32_t *)(pe+80)!=0xfb5000)return 0;
    /* These owners do not exist at the reviewed loader's cold checkpoint. */
    if(*(void **)(b+0xc9d9c0) || *(void **)(b+0xc9d9c8) || *(void **)(b+0xc492c8))return 0;
    return equal(b+0x419be2,focus_guard,14) && equal(b+0x419ae0,wndproc_guard,16);
}
static int __stdcall show_focus(void *hwnd,int command) {
    if(state.phase==5)uncertain();
    if(state.phase==2 && hwnd && hwnd==*(void **)(state.base+0xc9d9c8) &&
       *(uint32_t *)(state.base+0xc48d80)==0 && command==6)
        return state.visible(hwnd);
    return state.show(hwnd,command);
}
/* Cold rollback only. No unsafe live-unhook or unload entry point. */
__declspec(dllexport) unsigned int AotFocusRollbackCold(void) {
    if(state.phase!=2 || !outside_lock() || GetCurrentThreadId()!=state.thread ||
       *(void **)(state.base+0xc9d9c0) || *(void **)(state.base+0xc9d9c8) ||
       *(void **)(state.base+0xc492c8))return AOT_BAD_RANGE;
    AotStatus result=aot_revert(state.base,0xfb5000,&state.patch,1);
    if(result==AOT_ROLLBACK_FAILED)uncertain();
    if(result==AOT_OK)state.phase=4;
    /* Pin and near thunk remain process-lifetime, including after rollback. */
    return result;
}
static AotStatus arm(void) {
    if(state.phase || !outside_lock())return AOT_BAD_RANGE;
    uint8_t *b=GetModuleHandleW(0);
    if(!supported(b))return AOT_SIGNATURE_MISMATCH;
    static const uint16_t user32[]={'u','s','e','r','3','2','.','d','l','l',0};
    void *u=GetModuleHandleW(user32);
    ShowFn show=(ShowFn)GetProcAddress(u,"ShowWindow");
    VisibleFn visible=(VisibleFn)GetProcAddress(u,"IsWindowVisible");
    if(!u || !show || !visible || *(void **)(b+0x865688)!=(void *)show)return AOT_SIGNATURE_MISMATCH;
    uintptr_t next=(uintptr_t)b+0x419bef;
    uint8_t *code=aot_allocate_near(next,next,16);
    if(!code)return AOT_BAD_RANGE;
    code[0]=0xff;code[1]=0x25;
    for(unsigned i=2;i<6;i++)code[i]=0;
    uintptr_t fn=(uintptr_t)&show_focus;
    for(unsigned i=0;i<8;i++)code[6+i]=(uint8_t)(fn>>(8*i));
    int32_t delta;
    if(!aot_rel32(next,(uintptr_t)code,&delta)){VirtualFree(code,0,AOT_MEM_RELEASE);return AOT_BAD_RANGE;}
    unsigned long old;
    if(!VirtualProtect(code,16,0x20,&old) || !FlushInstructionCache(GetCurrentProcess(),code,16)){
        VirtualFree(code,0,AOT_MEM_RELEASE);return AOT_PROTECTION_FAILED;
    }
    if(!GetModuleHandleExW(5,(const uint16_t *)(uintptr_t)&show_focus,&state.pin)){
        VirtualFree(code,0,AOT_MEM_RELEASE);return AOT_PROTECTION_FAILED;
    }
    state.base=b;state.code=code;state.show=show;state.visible=visible;state.thread=GetCurrentThreadId();
    state.patch.rva=0x419bea;state.patch.length=6;
    for(unsigned i=0;i<6;i++)state.patch.expected[i]=focus_guard[8+i];
    state.patch.replacement[0]=0xe8;
    for(unsigned i=0;i<4;i++)state.patch.replacement[1+i]=(uint8_t)((uint32_t)delta>>(8*i));
    state.patch.replacement[5]=0x90;
    if(!equal(b+0x419be2,focus_guard,14)){state.phase=3;return AOT_SIGNATURE_MISMATCH;}
    state.phase=1;
    AotStatus result=aot_apply(b,0xfb5000,&state.patch,1);
    if(result==AOT_ROLLBACK_FAILED)uncertain();
    state.phase=result==AOT_OK?2:3;
    /* Retain pin/thunk on all post-publication outcomes. Never unload hooks. */
    return result;
}
#ifdef FOCUS_HOST_CHECK
AotStatus focus_host_arm(void){return arm();}
void focus_host_reset(void){state=(__typeof__(state)){0};}
int focus_host_show(void *w,int c){return show_focus(w,c);}
#else
__declspec(dllexport) void InitializeASI(void) {
    static const uint16_t loader_name[]={'d','3','d','1','1','.','d','l','l',0};
    void *loader=GetModuleHandleW(loader_name);
    if(!outside_lock() || !loader || !GetProcAddress(loader,"IsUltimateASILoader") ||
       (uintptr_t)__builtin_return_address(0)!=(uintptr_t)loader+0x202fe || !enabled())return;
    AotStatus result=arm();
    OutputDebugStringA(result==AOT_OK?"[aot-focus] focus-loss minimize suppression armed\n":
                      "[aot-focus] safely refused; native focus handling retained\n");
}
#endif
__declspec(dllexport) unsigned int AotFocusStatus(void){return state.phase;}
int __stdcall DllMain(void *self,unsigned long reason,void *reserved) {
    (void)reserved;
    if(reason==1)DisableThreadLibraryCalls(self);
    return 1;
}
