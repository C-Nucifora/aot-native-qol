#include "native_backend.h"
#include <stdatomic.h>
AOT_API unsigned long __stdcall GetCurrentThreadId(void);
AOT_API unsigned long long __stdcall GetTickCount64(void);
AOT_API unsigned long __stdcall WaitForSingleObject(void *,unsigned long);
AOT_API int __stdcall TerminateProcess(void *,unsigned int);
typedef struct {void *base,*allocation;uint32_t allocation_protect,pad;size_t size;uint32_t state,protect,type,pad2;} FpsMemoryInfo;
AOT_API size_t __stdcall VirtualQuery(const void *,FpsMemoryInfo *,size_t);
_Static_assert(sizeof(FpsMemoryInfo)==48,"Windows x64 memory query ABI");
/* Never enabled by DllMain. Sole state owned by game's saved clock thread. */
static struct {
 uint8_t *base;size_t size;FpsPrivateGuards guards;FpsRuntime runtime;
 uint8_t *timing_thunk,*wait_slot;AotPatch hook,mode_hooks[FPS_NATIVE_MODE_CALLS];
 unsigned prepared,parked,busy,mode_transition;
 void *completed_event;uintptr_t timer_identity;
 unsigned menu_session,selected_target,resume_pending,display_suspended,failure_hold,watchdog_abort;
 unsigned effective_target;
 FpsResumePolicy resume_policy;
 FpsSessionPhase phase;
 FpsRequestOutcome outcome;
 FpsResult last_result;
 uint64_t request_id,completed_id;
} n;
/* Readers never inspect mutable owner state. The short snapshot lock is
    try-only, and watchdog stop/heartbeat share one compare-exchanged word. */
static atomic_flag status_lock=ATOMIC_FLAG_INIT;
static FpsSessionStatus published_status;
static _Atomic unsigned status_ready;
static _Atomic uint64_t watchdog_word;
#define WATCHDOG_ACTIVE (UINT64_C(1)<<63)
#define WATCHDOG_STOP (UINT64_C(1)<<62)
#define HEARTBEAT_MASK (WATCHDOG_STOP-1u)
static int eq(const uint8_t *a,const uint8_t *b,size_t k){for(size_t i=0;i<k;i++)if(a[i]!=b[i])return 0;return 1;}
static void cp(uint8_t *a,const uint8_t *b,size_t k){for(size_t i=0;i<k;i++)a[i]=b[i];}
static int region(const void *p,size_t length,int writable){
 FpsMemoryInfo m={0};uintptr_t address=(uintptr_t)p;
 if(!p||!length||address>UINTPTR_MAX-length||VirtualQuery(p,&m,sizeof(m))!=sizeof(m)||m.state!=0x1000u||
    (m.protect&0x100u)||!m.protect||(m.protect&0xffu)==1u)return 0;
 uintptr_t lo=(uintptr_t)m.base;
 if(address<lo||address-lo>m.size||length>m.size-(address-lo))return 0;
 unsigned prot=m.protect&0xffu;
 if(writable)return prot==4u||prot==8u||prot==0x40u||prot==0x80u;
 return prot==2u||prot==4u||prot==8u||prot==0x20u||prot==0x40u||prot==0x80u;
}
static int owner(void){return n.prepared&&region(n.base+0xc7a45c,4,0)&&
 *(const uint32_t *)(n.base+0xc7a45c)==GetCurrentThreadId();}
static void heartbeat(int active){
 uint64_t old=atomic_load_explicit(&watchdog_word,memory_order_acquire),next;
 do{next=(old&WATCHDOG_STOP)|(active?WATCHDOG_ACTIVE:0)|(GetTickCount64()&HEARTBEAT_MASK);}
 while(!atomic_compare_exchange_weak_explicit(&watchdog_word,&old,next,memory_order_release,memory_order_acquire));
}
static int stop_requested(void){return (atomic_load_explicit(&watchdog_word,memory_order_acquire)&WATCHDOG_STOP)!=0;}
static void publish_session(void){
 uint64_t w=atomic_load_explicit(&watchdog_word,memory_order_acquire);
 if(atomic_flag_test_and_set_explicit(&status_lock,memory_order_acquire))return;
 published_status=(FpsSessionStatus){n.request_id,n.completed_id,w&HEARTBEAT_MASK,
  n.selected_target,n.effective_target,n.phase,n.outcome,n.last_result,n.resume_policy,
  (w&WATCHDOG_STOP)!=0};
 atomic_store_explicit(&status_ready,1,memory_order_release);
 atomic_flag_clear_explicit(&status_lock,memory_order_release);
}
static void complete_request(FpsRequestOutcome outcome,FpsResult result){
 n.outcome=outcome;n.last_result=result;n.completed_id=n.request_id;
}
static int hooks_owned(void){
 if(!n.prepared||!eq(n.base+n.hook.rva,n.hook.replacement,n.hook.length))return 0;
 for(unsigned i=0;i<FPS_NATIVE_MODE_CALLS;i++){
  const AotPatch *p=&n.mode_hooks[i];
  if(!eq(n.base+p->rva,p->replacement,p->length))return 0;
 }
 return 1;
}
static int read_pointer(uintptr_t at,uintptr_t *out){if((at&7u)||!region((const void *)at,8,0))return 0;*out=*(const uintptr_t *)at;return *out!=0;}
static int timer_view(uintptr_t *timer,volatile uint32_t **interval,void **completed){
 uintptr_t data,event;
 if(!read_pointer((uintptr_t)n.base+0xc7a460,timer)||!region((void *)*timer,0x30,0)||
    !read_pointer(*timer+8,&data)||data>UINTPTR_MAX-16||!region((void *)(data+16),4,1)||(data+16)&3u||
    !read_pointer(*timer+0x20,&event)||!region((void *)event,8,0))return 0;
 *completed=*(void **)event;*interval=(volatile uint32_t *)(data+16);return *completed!=0;
}
static int enter(void *context,FpsCheckpoint *c){
 (void)context;uintptr_t timer;volatile uint32_t *interval;void *event;
 if(!owner()||!n.parked||n.busy||n.mode_transition||!region(n.base+0xc48d78,4,0)||*(uint32_t *)(n.base+0xc48d78)!=0||
    !timer_view(&timer,&interval,&event)||timer!=n.timer_identity||event!=n.completed_event||
    !region(n.base+0xde081,8,0)||!region(n.base+0xde04d,5,0)||
    !eq(n.base+0xde06b,n.guards.timestamp,4)||!hooks_owned())return 0;
 n.busy=1;*c=(FpsCheckpoint){n.base+0xde081,n.base+0xde04d,interval,
                           *(uint32_t *)(n.base+0xc7a468),1};return 1;
}
static void leave(void *context,FpsCheckpoint *c){(void)context;(void)c;n.busy=0;}
static int patch(void *context,FpsCheckpoint *c,const uint8_t *before,const uint8_t *after){
 (void)context;AotPatch p={0};if(!n.busy||!c->lease||c->code!=n.base+0xde081)return -1;
 p.rva=0xde081;p.length=8;cp(p.expected,before,8);cp(p.replacement,after,8);
 AotStatus r=aot_apply(n.base,n.size,&p,1);return r==AOT_OK?1:r==AOT_ROLLBACK_FAILED?-1:0;
}
static void fatal(void){
 n.runtime.fatal=1;n.busy=1;
 n.phase=FPS_SESSION_FATAL;complete_request(FPS_REQUEST_FAILED,FPS_FATAL_HELD);
 heartbeat(0);publish_session();
 TerminateProcess(GetCurrentProcess(),0x465053u);
 /* Termination failure cannot return to potentially inconsistent game code. */
 for(;;){__builtin_trap();}
}
static unsigned long __stdcall waited(void *handle,unsigned long timeout){
 unsigned long result=WaitForSingleObject(handle,timeout);
 if(n.runtime.fatal)fatal();
 /* Original mode setters can re-enter frame-end during display recovery.
    They own native timer writes; never start another FPS transaction here. */
 if(n.mode_transition)return result;
 if(!n.prepared)return result;
 if(owner())heartbeat(n.runtime.owned!=0);
 if(n.menu_session&&n.failure_hold&&!stop_requested()){publish_session();return result;}
 if(!n.runtime.enabled&&!n.runtime.owned&&
    !(n.menu_session&&((n.resume_pending&&!n.display_suspended)||n.outcome==FPS_REQUEST_PENDING))){
  publish_session();return result;
 }
 uintptr_t timer;volatile uint32_t *interval;void *event;
 if(result!=0||timeout!=0xffffffffu||!owner()||!timer_view(&timer,&interval,&event)||event!=handle){
  if(n.runtime.owned)fatal();
  return result;
 }
 n.completed_event=event;n.timer_identity=timer;n.parked=1;
 const FpsBackend b={0,enter,leave,patch};
 if(n.menu_session&&stop_requested()){
  n.watchdog_abort=1;n.resume_pending=0;n.failure_hold=0;
  if(fps_runtime_select(&n.runtime,60,GetTickCount64())!=FPS_OK)fatal();
 }else if(n.menu_session&&n.resume_pending&&!n.display_suspended&&
          *(uint32_t *)(n.base+0xc7a468)==1&&*(uint32_t *)(n.base+0xc48d78)==0){
  if(fps_runtime_select(&n.runtime,n.selected_target,GetTickCount64())!=FPS_OK)fatal();
  n.resume_pending=0;
 }
 FpsResult changed;
 if(n.menu_session&&!n.runtime.enabled&&!n.runtime.owned){
  FpsCheckpoint c={0};
  if(!enter(0,&c))changed=FPS_NO_CHECKPOINT;
  else{
   uint32_t native_interval=c.scene_mode==2?33332u:16666u;
   changed=eq(c.code,n.runtime.stock,8)&&eq(c.clock_guard,n.runtime.clock,5)&&
           *c.timer==native_interval?FPS_DISABLED:FPS_CONFLICT;
   leave(0,&c);
  }
 }else changed=fps_runtime_checkpoint(&n.runtime,&b,GetTickCount64());
 n.parked=0;
 if(changed==FPS_FATAL_HELD||(n.runtime.owned&&changed==FPS_NO_CHECKPOINT))fatal();
 if(changed==FPS_OK){
  n.effective_target=n.runtime.target;n.phase=FPS_SESSION_ACTIVE;
  if(n.menu_session&&!n.display_suspended&&n.runtime.enabled&&n.runtime.target==n.selected_target)
   complete_request(FPS_REQUEST_CONFIRMED,FPS_OK);
 }else if(changed==FPS_DISABLED){
  n.effective_target=*(uint32_t *)(n.base+0xc7a468)==2?30:60;
  if(n.menu_session&&n.watchdog_abort){
   n.phase=FPS_SESSION_FAILED;n.failure_hold=1;complete_request(FPS_REQUEST_FAILED,FPS_DISABLED);
  }else if(n.menu_session&&n.selected_target==60&&!n.display_suspended){
   n.phase=FPS_SESSION_STOCK;complete_request(FPS_REQUEST_CONFIRMED,FPS_OK);
  }else n.phase=n.menu_session?FPS_SESSION_SUSPENDED:FPS_SESSION_STOCK;
 }else{
  n.last_result=changed;n.phase=FPS_SESSION_FAILED;n.resume_pending=0;n.failure_hold=1;
  if(n.menu_session)complete_request(FPS_REQUEST_FAILED,changed);
  /* A fully restored write failure retains the known previous effective
     policy until an explicit rollback/new Apply or watchdog request. */
  n.runtime.enabled=n.runtime.owned;n.runtime.target=n.runtime.owned?n.effective_target:0;
 }
 heartbeat(n.runtime.owned!=0);publish_session();
 return result;
}
typedef uintptr_t (__stdcall *FpsOriginalMode)(void *);
static uintptr_t mode_changed(void *clock,unsigned mode,FpsOriginalMode original){
 if(!n.prepared||n.runtime.fatal||!original||(mode!=1&&mode!=2))fatal();
 /* With no experiment the observer is transparent, including startup. */
 int session_interest=n.menu_session!=0;
 if(!n.runtime.enabled&&!n.runtime.owned&&!n.mode_transition&&!session_interest)return original(clock);
 if(!owner()||clock!=n.base+0xc7a410||n.busy||n.mode_transition||
    n.runtime.fatal||!hooks_owned())fatal();
 uintptr_t timer;volatile uint32_t *interval;void *event;
 unsigned old_mode=*(uint32_t *)(n.base+0xc7a468);
 if((old_mode!=1&&old_mode!=2)||!timer_view(&timer,&interval,&event)||
    !eq(n.base+0xde04d,n.guards.clock,5)||!eq(n.base+0xde06b,n.guards.timestamp,4))fatal();
 uint32_t native_before=old_mode==2?33332u:16666u;
 const uint8_t *expected=n.runtime.owned?n.runtime.experiment:n.runtime.stock;
 if(!eq(n.base+0xde081,expected,8)||
    (n.runtime.owned&&(interval!=n.runtime.owned_timer||
      (*interval!=n.runtime.owned_interval&&*interval!=native_before)))||
    (!n.runtime.owned&&*interval!=native_before))fatal();
 n.busy=1;n.mode_transition=1;n.runtime.target=0;n.runtime.enabled=0;
 n.runtime.deadline_ms=GetTickCount64();
 if(n.runtime.owned){
  AotPatch p={0};p.rva=0xde081;p.length=8;
  cp(p.expected,n.runtime.experiment,8);cp(p.replacement,n.runtime.stock,8);
  /* Sole identified clock owner is between timing calls. Do not write the
     worker's interval here: the original mode setter already owns that. */
  if(aot_apply(n.base,n.size,&p,1)!=AOT_OK)fatal();
 }
 if(!eq(n.base+0xde081,n.runtime.stock,8))fatal();
 heartbeat(0);
 if(n.menu_session){n.phase=FPS_SESSION_SUSPENDED;publish_session();}
 uintptr_t result=original(clock);
 uintptr_t after_timer;volatile uint32_t *after_interval;void *after_event;
 if(!owner()||!timer_view(&after_timer,&after_interval,&after_event)||
    after_timer!=timer||after_interval!=interval||after_event!=event||
    *(uint32_t *)(n.base+0xc7a468)!=mode||*after_interval!=(mode==2?33332u:16666u)||
    !eq(n.base+0xde081,n.runtime.stock,8)||!eq(n.base+0xde04d,n.guards.clock,5)||
    !eq(n.base+0xde06b,n.guards.timestamp,4)||!hooks_owned())fatal();
 n.runtime.owned=0;n.runtime.owned_timer=0;n.runtime.owned_interval=0;
 n.mode_transition=0;n.busy=0;n.effective_target=mode==2?30:60;
 if(n.menu_session){
  n.resume_pending=n.selected_target!=60&&n.resume_policy==FPS_RESUME_WHEN_NATIVE_60&&
                    !n.display_suspended&&!n.failure_hold&&!stop_requested();
  n.phase=n.selected_target==60?FPS_SESSION_PENDING:FPS_SESSION_SUSPENDED;
  if(n.selected_target!=60&&!n.resume_pending&&n.outcome==FPS_REQUEST_PENDING&&
     n.resume_policy==FPS_HOLD_AFTER_NATIVE_MODE)
   complete_request(FPS_REQUEST_FAILED,FPS_DISABLED);
 }
 heartbeat(0);publish_session();return result;
}
static uintptr_t __stdcall mode60(void *clock){
 return mode_changed(clock,1,(FpsOriginalMode)(n.base+0xde6a0));
}
static uintptr_t __stdcall mode30(void *clock){
 return mode_changed(clock,2,(FpsOriginalMode)(n.base+0xde760));
}
static void absolute_jump(uint8_t *out,uintptr_t target){
 const uint8_t indirect[6]={0xff,0x25,0,0,0,0};cp(out,indirect,6);
 for(unsigned i=0;i<8;i++)out[6+i]=(uint8_t)(target>>(i*8u));
}
int fps_native_prepare(uint8_t *base,size_t size,const FpsPrivateGuards *guards){
 if(n.prepared||!base||!guards||base!=(uint8_t *)GetModuleHandleW(0)||size<0xc7a46cu||
    !region(base+0xde04d,0x100,0)||!region(base+0xddf40,6,0)||
    !eq(base+0xde081,guards->timing,8)||!eq(base+0xde04d,guards->clock,5)||
    !eq(base+0xde06b,guards->timestamp,4)||!eq(base+0xddf40,guards->wait_call,6)||!region(base+0xa8914c,4,0)||
    !eq(base+0xa8914c,guards->simulation_cap,4)||*(float *)(base+0xa8914c)!=0.05f)return 0;
 const uint32_t flow_rva[6]={0xddf24,0x439c40,0x439de0,0x442600,0xde6a0,0xde760};
 const uint32_t flow_length[6]={47,148,150,128,72,68};
 for(unsigned i=0;i<6;i++){
  const FpsFlowGuard *f=&guards->flow[i];
  if(f->rva!=flow_rva[i]||f->length!=flow_length[i]||!f->bytes||
     f->rva>size||f->length>size-f->rva||!region(base+f->rva,f->length,0)||
     !eq(base+f->rva,f->bytes,f->length))return 0;
 }
 const uint32_t mode_rva[FPS_NATIVE_MODE_CALLS]={0xdcd3a,0xdcd48,0xdd0e8,0xdd472,
  0xde837,0xde843,0xde859,0xde899,0x2db0d2,0x2db3fa,0x2de5e1,0x2de679};
 const unsigned to30[FPS_NATIVE_MODE_CALLS]={1,0,0,0,1,0,0,1,0,0,0,0};
 for(unsigned i=0;i<FPS_NATIVE_MODE_CALLS;i++){
  const FpsModeCallGuard *g=&guards->mode_calls[i];
  uint8_t opcode=(i==4||i==5)?0xe9:0xe8;
  if(g->rva!=mode_rva[i]||g->expected[0]!=opcode||g->rva>size||5u>size-g->rva||
     !region(base+g->rva,5,0)||!eq(base+g->rva,g->expected,5))return 0;
  int32_t displacement=0;for(unsigned j=0;j<4;j++)((uint8_t *)&displacement)[j]=g->expected[1+j];
  if((int64_t)g->rva+5+displacement!=(to30[i]?0xde760:0xde6a0))return 0;
 }
 /* At de081 XMM0 already equals clamped XMM1. A mode1-only high path avoids
    changing native30Hz if its mode setter runs after our frame-end checkpoint.
    pushfq/cmp/jne/popfq affect no surviving flags or stack depth. */
 uint8_t *thunk=aot_allocate_near((uintptr_t)base+0xde086,(uintptr_t)base+0xa8914c,53);
 if(!thunk)return 0;
 const uint8_t head[8]={0x9c,0x83,0x7b,0x58,0x01,0x75,0x17,0x9d};cp(thunk,head,8);
 /* Retain stock pacingmode0 simulation cap .05 seconds across stalls. */
 int32_t cap_relative;
 if(!aot_rel32((uintptr_t)thunk+16,(uintptr_t)base+0xa8914c,&cap_relative))return 0;
 const uint8_t minss[4]={0xf3,0x0f,0x5d,0x05};cp(thunk+8,minss,4);
 for(unsigned i=0;i<4;i++)thunk[12+i]=(uint8_t)((uint32_t)cap_relative>>(i*8u));
 absolute_jump(thunk+16,(uintptr_t)base+0xde126);
 thunk[30]=0x9d;cp(thunk+31,guards->timing,8);absolute_jump(thunk+39,(uintptr_t)base+0xde089);
 unsigned long old;
 if(!VirtualProtect(thunk,53,0x20u,&old)||!FlushInstructionCache(GetCurrentProcess(),thunk,53))return 0;
 uint8_t branch[8]={0xe9,0,0,0,0,0x90,0x90,0x90};int32_t relative;
 if(!aot_rel32((uintptr_t)base+0xde086,(uintptr_t)thunk,&relative))return 0;
 for(unsigned i=0;i<4;i++)branch[1+i]=(uint8_t)((uint32_t)relative>>(i*8u));
 uint8_t *slot=aot_allocate_near((uintptr_t)base+0xddf46,(uintptr_t)base+0xddf46,8);
 if(!slot)return 0;
 uintptr_t callback=(uintptr_t)&waited;for(unsigned i=0;i<8;i++)slot[i]=(uint8_t)(callback>>(i*8u));
 if(!VirtualProtect(slot,8,2u,&old)||!aot_rel32((uintptr_t)base+0xddf46,(uintptr_t)slot,&relative))return 0;
 n.base=base;n.size=size;n.guards=*guards;n.timing_thunk=thunk;n.wait_slot=slot;
 n.hook=(AotPatch){0};n.hook.rva=0xddf40;n.hook.length=6;cp(n.hook.expected,guards->wait_call,6);
 n.hook.replacement[0]=0xff;n.hook.replacement[1]=0x15;
 for(unsigned i=0;i<4;i++)n.hook.replacement[2+i]=(uint8_t)((uint32_t)relative>>(i*8u));
 /* Shared absolute relays preserve each original CALL versus tail-JMP ABI. */
 uint8_t *relays[2];
 for(unsigned mode=0;mode<2;mode++){
  relays[mode]=aot_allocate_near((uintptr_t)base+0xdcd3f,(uintptr_t)base+0x2de67e,14);
  if(!relays[mode])return 0;
  absolute_jump(relays[mode],mode?(uintptr_t)&mode30:(uintptr_t)&mode60);
  if(!VirtualProtect(relays[mode],14,0x20u,&old)||
     !FlushInstructionCache(GetCurrentProcess(),relays[mode],14))return 0;
 }
 for(unsigned i=0;i<FPS_NATIVE_MODE_CALLS;i++){
  AotPatch *p=&n.mode_hooks[i];*p=(AotPatch){0};p->rva=mode_rva[i];p->length=5;
  cp(p->expected,guards->mode_calls[i].expected,5);p->replacement[0]=p->expected[0];
  if(!aot_rel32((uintptr_t)base+p->rva+5,(uintptr_t)relays[to30[i]],&relative))return 0;
  for(unsigned j=0;j<4;j++)p->replacement[1+j]=(uint8_t)((uint32_t)relative>>(j*8u));
 }
 fps_runtime_init(&n.runtime,guards->timing,guards->clock,branch);n.prepared=1;
 n.selected_target=60;n.effective_target=*(uint32_t *)(base+0xc7a468)==2?30:60;
 n.phase=FPS_SESSION_STOCK;heartbeat(0);publish_session();return 1;
}
int fps_native_hook_patch(AotPatch *out){if(!n.prepared||!out)return 0;*out=n.hook;return 1;}
size_t fps_native_hook_patches(AotPatch *out,size_t capacity){
 if(!n.prepared||!out||capacity<FPS_NATIVE_HOOK_COUNT)return 0;
 out[0]=n.hook;for(unsigned i=0;i<FPS_NATIVE_MODE_CALLS;i++)out[i+1]=n.mode_hooks[i];
 return FPS_NATIVE_HOOK_COUNT;
}
FpsResult fps_native_begin_experiment(unsigned target,unsigned duration){
 if(n.menu_session)return FPS_BAD_ARGUMENT;
 if(!owner()||n.busy||n.mode_transition||!hooks_owned()||
    !region(n.base+0xc48d78,4,0)||*(uint32_t *)(n.base+0xc48d78)!=0||
    *(uint32_t *)(n.base+0xc7a468)!=1)return FPS_NO_CHECKPOINT;
 n.display_suspended=0;
 return fps_runtime_request(&n.runtime,target,GetTickCount64(),duration);
}
FpsResult fps_native_request_stock(void){
 if(n.menu_session){uint64_t ticket;return fps_native_set_selected_target(60,FPS_HOLD_AFTER_NATIVE_MODE,&ticket);}
 if(!owner()||n.busy||n.mode_transition)return FPS_NO_CHECKPOINT;
 return fps_runtime_request(&n.runtime,60,GetTickCount64(),0);
}

int fps_native_before_display_change(void){
 if(!n.prepared)return 1;
 if(!owner()||n.busy||n.mode_transition||n.runtime.fatal)return -1;
 n.display_suspended=1;
 if(n.menu_session){n.resume_pending=0;n.failure_hold=0;
  if(fps_runtime_select(&n.runtime,60,GetTickCount64())!=FPS_OK)return -1;
  n.phase=FPS_SESSION_SUSPENDED;publish_session();
 }else if(fps_runtime_request(&n.runtime,60,GetTickCount64(),0)!=FPS_OK)return -1;
 if(n.runtime.owned)return 0;
 return eq(n.base+0xde081,n.runtime.stock,8)?1:-1;
}
FpsResult fps_native_after_display_change(void){
 if(!owner()||n.busy||n.mode_transition||n.runtime.fatal||!hooks_owned())return FPS_NO_CHECKPOINT;
 n.display_suspended=0;
 if(n.menu_session&&!n.failure_hold){
  n.resume_pending=n.selected_target!=60&&n.resume_policy==FPS_RESUME_WHEN_NATIVE_60;
  n.phase=FPS_SESSION_PENDING;publish_session();
 }
 return FPS_OK;
}
FpsResult fps_native_set_selected_target(unsigned target,FpsResumePolicy policy,uint64_t *ticket){
 if(!ticket||!fps_runtime_supports_target(target)||
    (policy!=FPS_HOLD_AFTER_NATIVE_MODE&&policy!=FPS_RESUME_WHEN_NATIVE_60)||n.request_id==UINT64_MAX)
  return FPS_BAD_ARGUMENT;
 if(!owner()||n.busy||n.mode_transition||n.runtime.fatal||!hooks_owned())return FPS_NO_CHECKPOINT;
 uint64_t now=GetTickCount64();
 /* Queue only. Held display gates and native30 remain stock until released;
    the next verified owner checkpoint confirms the new ticket. */
 unsigned queued=n.display_suspended||*(uint32_t *)(n.base+0xc7a468)!=1||
                 *(uint32_t *)(n.base+0xc48d78)!=0?60:target;
 FpsResult result=fps_runtime_select(&n.runtime,queued,now);if(result!=FPS_OK)return result;
 n.menu_session=1;n.selected_target=target;n.resume_policy=policy;n.request_id++;
 n.resume_pending=target!=60;n.failure_hold=0;n.watchdog_abort=0;
 n.phase=FPS_SESSION_PENDING;n.outcome=FPS_REQUEST_PENDING;n.last_result=FPS_OK;
 atomic_store_explicit(&watchdog_word,(n.runtime.owned?WATCHDOG_ACTIVE:0)|(now&HEARTBEAT_MASK),memory_order_release);
 *ticket=n.request_id;publish_session();return FPS_OK;
}
FpsResult fps_native_poll_owner(void){
 if(!owner()||n.busy||n.mode_transition||n.runtime.fatal)return FPS_NO_CHECKPOINT;
 heartbeat(n.runtime.owned!=0);
 if(!n.menu_session||n.display_suspended||n.selected_target!=60||
    n.outcome!=FPS_REQUEST_PENDING||n.runtime.owned||n.runtime.enabled){
  publish_session();return FPS_DISABLED;
 }
 uintptr_t timer;volatile uint32_t *interval;void *event;
 unsigned mode=*(uint32_t *)(n.base+0xc7a468);
 if(!hooks_owned()||(mode!=1&&mode!=2)||!timer_view(&timer,&interval,&event)||
    !eq(n.base+0xde081,n.runtime.stock,8)||!eq(n.base+0xde04d,n.runtime.clock,5)||
    !eq(n.base+0xde06b,n.guards.timestamp,4)||*interval!=(mode==2?33332u:16666u)){
  n.phase=FPS_SESSION_FAILED;n.failure_hold=1;complete_request(FPS_REQUEST_FAILED,FPS_CONFLICT);
  publish_session();return FPS_CONFLICT;
 }
 n.effective_target=mode==2?30:60;n.phase=FPS_SESSION_STOCK;
 complete_request(FPS_REQUEST_CONFIRMED,FPS_OK);publish_session();return FPS_OK;
}
int fps_native_get_status(FpsSessionStatus *out){
 if(!out||!atomic_load_explicit(&status_ready,memory_order_acquire)||
    atomic_flag_test_and_set_explicit(&status_lock,memory_order_acquire))return 0;
 *out=published_status;
 atomic_flag_clear_explicit(&status_lock,memory_order_release);
 uint64_t w=atomic_load_explicit(&watchdog_word,memory_order_acquire);
 out->heartbeat_ms=w&HEARTBEAT_MASK;out->watchdog_stop_requested=(w&WATCHDOG_STOP)!=0;
 return 1;
}
FpsWatchdogResult fps_native_watchdog_poll(unsigned stall_ms){
 if(stall_ms<1000||stall_ms>60000)return FPS_WATCHDOG_BAD_ARGUMENT;
 uint64_t old=atomic_load_explicit(&watchdog_word,memory_order_acquire);
 for(;;){
  if(!(old&WATCHDOG_ACTIVE))return FPS_WATCHDOG_INACTIVE;
  if(old&WATCHDOG_STOP)return FPS_WATCHDOG_STOP_REQUESTED;
  uint64_t now=GetTickCount64(),last=old&HEARTBEAT_MASK;
  if(now<last||now-last<stall_ms)return FPS_WATCHDOG_HEALTHY;
  uint64_t requested=old|WATCHDOG_STOP;
  if(atomic_compare_exchange_weak_explicit(&watchdog_word,&old,requested,memory_order_release,memory_order_acquire))
   return FPS_WATCHDOG_STOP_REQUESTED;
 }
}
