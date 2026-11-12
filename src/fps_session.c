/* The session backend is copied unchanged into the ignored candidate build.
 * Interpose its original Wait import only to load the persisted selection at
 * an actual owner checkpoint and log stable state. No hotkey or target expiry. */
#define AOT_API
#include "aot_fps_session.h"
#include "aot_owned_files.h"
#include "aot_settings_io.h"
#include "aot_fps_capabilities.h"
#ifdef AOT_FPS_TIMING
#include "aot_timing.h"
#define SESSION_EXTRA_HOOKS AOT_TIMING_CALLS
#else
#define SESSION_EXTRA_HOOKS 0u
#endif
#include <stdatomic.h>
/* Freestanding support for the reviewed backend's guard-structure copy. */
void *memcpy(void *out,const void *in,size_t count){
    volatile uint8_t *d=out;const volatile uint8_t *s=in;
    for(size_t i=0;i<count;i++)d[i]=s[i];return out;
}
void * __stdcall CreateFileW(const uint16_t *,unsigned long,unsigned long,void *,unsigned long,unsigned long,void *);
int __stdcall ReadFile(void *,void *,unsigned long,unsigned long *,void *);
int __stdcall WriteFile(void *,const void *,unsigned long,unsigned long *,void *);
int __stdcall CloseHandle(void *);
unsigned long __stdcall GetLastError(void);
static unsigned long __stdcall session_wait(void *,unsigned long);
static int __stdcall session_terminate(void *,unsigned int);
#define WaitForSingleObject session_wait
#define TerminateProcess session_terminate
#include "fps_native_backend.c"
#undef WaitForSingleObject
#undef TerminateProcess
unsigned long __stdcall WaitForSingleObject(void *,unsigned long);
int __stdcall TerminateProcess(void *,unsigned int);
unsigned long __stdcall GetModuleFileNameW(void *,uint16_t *,unsigned long);
int __stdcall GetModuleHandleExW(unsigned long,const uint16_t *,void **);
void * __stdcall CreateThread(void *,size_t,unsigned long (__stdcall *)(void *),void *,unsigned long,unsigned long *);
void __stdcall Sleep(unsigned long);
#include "fps_session_private_guards.h"
static struct {
    unsigned enabled,loaded,last_effective;
    FpsSessionPhase last_phase;
    _Atomic(unsigned) stop;
    void *log,*pin;
    uint16_t config[1024];
} session;
static void event(const char *text,uint64_t value){
    const char *message=text;char line[192];unsigned count=0;
    while(*text && count<150)line[count++]=*text++;
    char digits[24];unsigned k=0;
    do{digits[k++]=(char)('0'+value%10);value/=10;}while(value);
    line[count++]=' ';while(k)line[count++]=digits[--k];
    line[count++]='\r';line[count++]='\n';
    if(session.log){unsigned long wrote=0;WriteFile(session.log,line,count,&wrote,0);}
    OutputDebugStringA(message);
}
/* Monitor callers use only atomics and the backend's try-only published
 * snapshot. Mutable native state is inspected only on its verified owner. */
static void snapshot(unsigned reason,int owner_fields){
    event("diagnostic reason",reason);
    event("diagnostic clock_ms",GetTickCount64());
    event("diagnostic current_thread",GetCurrentThreadId());
    uint64_t word=atomic_load_explicit(&watchdog_word,memory_order_acquire);
    event("diagnostic heartbeat_ms",word&HEARTBEAT_MASK);
    event("diagnostic watchdog_active",(word&WATCHDOG_ACTIVE)!=0);
    event("diagnostic watchdog_stop",(word&WATCHDOG_STOP)!=0);
    FpsSessionStatus state;
    int available=fps_native_get_status(&state);
    event("diagnostic status_available",available);
    if(available){
        event("diagnostic phase",state.phase);
        event("diagnostic selected",state.selected_target);
        event("diagnostic effective",state.effective_target);
        event("diagnostic request",state.request_id);
        event("diagnostic completed",state.completed_id);
        event("diagnostic outcome",state.outcome);
        event("diagnostic last_result",(unsigned)state.last_result);
    }
    if(owner_fields && owner()){
        event("diagnostic owner_thread",*(uint32_t *)(n.base+0xc7a45c));
        event("diagnostic native_mode",*(uint32_t *)(n.base+0xc7a468));
        event("diagnostic native_pacing",*(uint32_t *)(n.base+0xc48d78));
        event("diagnostic runtime_owned",n.runtime.owned);
        event("diagnostic runtime_fatal",n.runtime.fatal);
        event("diagnostic mode_transition",n.mode_transition);
        event("diagnostic display_suspended",n.display_suspended);
        event("diagnostic hooks_owned",hooks_owned());
        event("diagnostic timing_is_stock",eq(n.base+0xde081,n.runtime.stock,8));
        uintptr_t timer;volatile uint32_t *interval;void *completed;
        int valid=timer_view(&timer,&interval,&completed);
        event("diagnostic timer_valid",valid);
        if(valid)event("diagnostic interval_us",*interval);
    }
}
_Noreturn void aot_fps_session_fail_reason(unsigned reason){
    snapshot(reason,1);
    event("FATAL session ownership/rollback; process stop, no off-thread restore",reason);
    TerminateProcess(GetCurrentProcess(),0xa074f101u);
    __asm__ volatile("int $0x29"::"c"(7):"memory");__builtin_unreachable();
}
_Noreturn void aot_fps_session_fail(void){
    aot_fps_session_fail_reason(AOT_FPS_FAIL_UNSPECIFIED);
}
static int __stdcall session_terminate(void *process,unsigned code){
    snapshot(AOT_FPS_FAIL_BACKEND,1);
    event("backend fatal; stopping without off-thread restoration",code);
    return TerminateProcess(process,code);
}
/* No deadline: focus loss/loading can legally stop the owner indefinitely.
 * The backend queues STOP atomically; only its next verified owner checkpoint
 * can restore timing. A stale heartbeat is never evidence of corrupt bytes. */
static void watchdog_step(unsigned *waiting){
    FpsWatchdogResult result=fps_native_watchdog_poll(10000);
    if(result==FPS_WATCHDOG_STOP_REQUESTED && !*waiting){
        *waiting=1;
        event("watchdog owner stalled; stop queued; waiting safe owner return",0);
        snapshot(0,0);
    }else if(result==FPS_WATCHDOG_INACTIVE && *waiting){
        *waiting=0;
        event("watchdog owner returned; timing inactive after guarded restoration",0);
        snapshot(0,0);
    }
}
static unsigned long __stdcall watchdog(void *unused){
    (void)unused;unsigned waiting=0;
    while(!atomic_load_explicit(&session.stop,memory_order_acquire)){
        watchdog_step(&waiting);
        Sleep(100);
    }
    return 0;
}
static int file_path(void *self,const char *name,uint16_t out[1024]){
    unsigned long count=GetModuleFileNameW(self,out,1024);
    if(!count || count>=1024)return 0;
    while(count && out[count-1]!='\\' && out[count-1]!='/')count--;
    if(!count)return 0;
    for(unsigned i=0;;i++){if(count+i>=1024)return 0;out[count+i]=(uint8_t)name[i];if(!name[i])return 1;}
}
static void owner_events(void){
    if(!session.loaded){
        unsigned target=AOT_FPS_DEFAULT;
        AotSettingsResult loaded=aot_settings_load(session.config,&target);
        if(loaded!=AOT_SETTINGS_OK && loaded!=AOT_SETTINGS_MISSING){
            event("persisted selection refused; retaining stock60",loaded);target=AOT_FPS_DEFAULT;
        }
        uint64_t ticket;
        if(fps_native_set_selected_target(target,FPS_RESUME_WHEN_NATIVE_60,&ticket)!=FPS_OK)
            aot_fps_session_fail_reason(AOT_FPS_FAIL_LOAD_REQUEST);
        session.loaded=1;event("persisted selection queued;0 means Unlimited",target);
    }
    aot_fps_session_commit_poll();
    FpsSessionStatus status;
    if(fps_native_get_status(&status) &&
       (status.phase!=session.last_phase || status.effective_target!=session.last_effective)){
        session.last_phase=status.phase;session.last_effective=status.effective_target;
        event("session phase",(unsigned)status.phase);
        event("effective pacing target;0 uncapped only when ACTIVE",status.effective_target);
        if(status.phase==FPS_SESSION_FATAL)aot_fps_session_fail_reason(AOT_FPS_FAIL_OWNER_STATUS);
    }
}
static unsigned long __stdcall session_wait(void *handle,unsigned long timeout){
    unsigned long result=WaitForSingleObject(handle,timeout);
    if(session.enabled && result==0 && timeout==0xffffffffu && owner() && !n.mode_transition && !n.runtime.fatal)
        owner_events();
    return result;
}
int aot_fps_session_select(unsigned target,uint64_t *ticket){
    if(!session.enabled || !aot_fps_supported(target) || !ticket)return 0;
    FpsResult result=fps_native_set_selected_target(target,FPS_RESUME_WHEN_NATIVE_60,ticket);
    if(result!=FPS_OK)return 0;
    session.loaded=1; /* An explicit menu Apply supersedes startup config load. */
    event("menu selection queued;0 means Unlimited",target);return 1;
}
int aot_fps_session_outcome(uint64_t ticket){
    FpsSessionStatus status;
    if(!session.enabled || !ticket)return -1;
    (void)fps_native_poll_owner();
    if(!fps_native_get_status(&status))return 0;
    if(status.phase==FPS_SESSION_FATAL)aot_fps_session_fail_reason(AOT_FPS_FAIL_OUTCOME_STATUS);
    if(status.request_id!=ticket)return -1;
    if(status.outcome==FPS_REQUEST_FAILED)return -1;
    if(status.completed_id==ticket && status.outcome==FPS_REQUEST_CONFIRMED)return 1;
    return 0;
}
int aot_fps_session_queued(uint64_t ticket){
    FpsSessionStatus status;
    if(!session.enabled || !ticket || !fps_native_get_status(&status) || status.request_id!=ticket)return 0;
    return (status.outcome==FPS_REQUEST_PENDING &&
            (status.phase==FPS_SESSION_PENDING || status.phase==FPS_SESSION_SUSPENDED)) ||
           (status.outcome==FPS_REQUEST_CONFIRMED && status.completed_id==ticket);
}
int aot_fps_session_display_ready(void){
    if(!session.enabled)return 0;
    int result=fps_native_before_display_change();
    if(result<0)aot_fps_session_fail_reason(AOT_FPS_FAIL_DISPLAY_GATE);return result;
}
void aot_fps_session_display_finished(void){
    if(!session.enabled || fps_native_after_display_change()!=FPS_OK)aot_fps_session_fail_reason(AOT_FPS_FAIL_DISPLAY_RELEASE);
}
AotStatus aot_fps_session_start(void *self){
    if(!self || session.enabled || !aot_supported_executable())return AOT_SIGNATURE_MISMATCH;
    uint16_t path[1024];
    if(!file_path(self,"aot-fps-session.log",path) ||
       !file_path(self,"aot-native-fps.cfg",session.config))return AOT_BAD_RANGE;
    session.log=CreateFileW(path,4,3,0,4,0x80,0);
    if(session.log==(void *)(intptr_t)-1){session.log=0;return AOT_BAD_RANGE;}
    event("NEW LAUNCH; native menu session; starts stock60; no hotkey/expiry",(unsigned)GetTickCount64());
    if(!GetModuleHandleExW(5,(const uint16_t *)(uintptr_t)&aot_fps_session_start,&session.pin))return AOT_PROTECTION_FAILED;
    uint8_t *base=GetModuleHandleW(0);
    if(!fps_native_prepare(base,0xfb5000,&fps_private_guards))return AOT_SIGNATURE_MISMATCH;
    for(unsigned i=0;i<AOT_FPS_PRODUCTION_COUNT;i++)if(!fps_runtime_supports_target(aot_fps_targets[i]))return AOT_SIGNATURE_MISMATCH;
    void *thread=CreateThread(0,0,watchdog,0,0,0);
    if(!thread)return AOT_PROTECTION_FAILED;
    CloseHandle(thread);
    AotPatch hooks[FPS_NATIVE_HOOK_COUNT+SESSION_EXTRA_HOOKS];
    if(fps_native_hook_patches(hooks,FPS_NATIVE_HOOK_COUNT)!=FPS_NATIVE_HOOK_COUNT){
        atomic_store_explicit(&session.stop,1,memory_order_release);return AOT_BAD_RANGE;
    }
#ifdef AOT_FPS_TIMING
    AotStatus normalized=aot_timing_prepare(base,0xfb5000,hooks+FPS_NATIVE_HOOK_COUNT);
    if(normalized!=AOT_OK){atomic_store_explicit(&session.stop,1,memory_order_release);return normalized;}
#endif
    AotStatus status=aot_apply(base,0xfb5000,hooks,FPS_NATIVE_HOOK_COUNT+SESSION_EXTRA_HOOKS);
    if(status==AOT_ROLLBACK_FAILED)aot_fps_session_fail_reason(AOT_FPS_FAIL_STARTUP_ROLLBACK);
    if(status!=AOT_OK){atomic_store_explicit(&session.stop,1,memory_order_release);return status;}
    session.enabled=1;
#ifdef AOT_FPS_TIMING
    aot_timing_armed();
    event("elapsed repeat and verified-controller camera hooks armed; total",FPS_NATIVE_HOOK_COUNT+SESSION_EXTRA_HOOKS);
#endif
    event("all13 hooks armed; waiting actual owner checkpoint; default",AOT_FPS_DEFAULT);return AOT_OK;
}

#ifdef AOT_FPS_TIMING
static int rate_permitted(unsigned allow_menu){
    if(!session.enabled || !n.prepared || !n.menu_session ||
       n.selected_target==60 || !owner() || n.busy || n.mode_transition ||
       n.runtime.fatal || !hooks_owned() || *(uint32_t *)(n.base+0xc7a468)!=1 ||
       !eq(n.base+0xde04d,n.guards.clock,5) || !eq(n.base+0xde06b,n.guards.timestamp,4))return 0;
    unsigned pacing=*(uint32_t *)(n.base+0xc48d78);
    return pacing<=1 && (allow_menu || !pacing);
}
/* Only the native local scenario-camera nearpause may bypass the scaled-dt
 * epsilon gate. Keep native pause-zero, scene30 and stock/unowned paths. */
int aot_fps_session_cutin(void *candidate,unsigned slot,float scaled,float unscaled){
    if(!(scaled>0 && scaled<0x1.9p-17f && unscaled>0 && unscaled<=0.05f) ||
       !rate_permitted(0) || !n.runtime.owned || slot>=4)return 0;
    uint8_t *root=*(uint8_t **)(n.base+0xc144c8);
    if(candidate!=root || !region(root,0x14894,0))return 0;
    unsigned mode=*(uint32_t *)(root+0x14890);
    if((mode!=5 && mode!=6) || *(uint32_t *)(root+0x14888)!=0x3a9c4000 ||
       !(*(float *)(root+0x1488c)>0))return 0;
    uint8_t *manager=*(uint8_t **)(root+0x40);
    if(!region(manager,0x16884,0))return 0;
    unsigned index=*(uint32_t *)(root+0x27c+slot*4),count=*(uint32_t *)(manager+0x16880);
    if(index>=count || index>=(UINTPTR_MAX-(uintptr_t)manager-0x20)/0xb430)return 0;
    uint8_t *record=(uint8_t *)((uintptr_t)manager+0x20+(uintptr_t)index*0xb430);
    if(!region(record,0xb430,0))return 0;
    uint8_t *camera=record+0x6fc0;
    int valid=*(void **)(record+0x10)==camera && *(void **)(record+0xb410)==camera &&
        *(uintptr_t *)camera==(uintptr_t)n.base+0x9e2668 &&
        *(uint32_t *)(record+0x7aa0)==1 &&
        *(uint32_t *)(record+0x7aa8)==mode-4;
    static unsigned logged_modes;
    unsigned bit=1u<<(mode-5);
    if(valid && !(logged_modes&bit)){
        logged_modes|=bit;
        event("native scenario-camera positive-delta epsilon exception; mode",mode);
    }
    return valid;
}
int aot_fps_session_rate_clock(double *seconds,unsigned allow_menu){
    if(!seconds || !rate_permitted(allow_menu))return 0;
    float elapsed=*(float *)(n.base+0xc7a424);
    if(!(elapsed>0 && elapsed<=1))return 0;
    *seconds=elapsed>0.05f?0.05:(double)elapsed;return 1;
}
int aot_fps_session_ui_frame(void){
    if(!rate_permitted(1) || !*(uint64_t *)(n.base+0xc7a418))return 0;
    float elapsed=*(float *)(n.base+0xc7a424);
    return elapsed>=0 && elapsed<=1;
}
int aot_fps_session_pad_index(const void *state,const void *device,unsigned *index){
    if(!index)return 0;
    *index=2;
    if(!session.enabled || !owner() || !n.prepared)return 0;
    for(unsigned i=0;i<2;i++)if(state==n.base+0xc9d7f0+0xa0*i)*index=i;
    if(*index>=2 || !device || !region(device,0x288,0))return 0;
    const uint8_t *d=device;
    return *(uintptr_t *)d==(uintptr_t)n.base+0xa6e0b0 &&
           *(uint32_t *)(d+0x284)==*index;
}
int aot_fps_session_camera_index(const void *context,unsigned *index){
    if(!index)return 0;
    *index=2;
    if(!session.enabled || !owner() || !region(context,0x214,0))return 0;
    unsigned id=*(uint32_t *)((const uint8_t *)context+0x1f0);
    if(id>=2 || *(uint32_t *)(n.base+0xc9d7f0+0xa0*id)!=id)return 0;
    *index=id;return 1;
}
#endif
