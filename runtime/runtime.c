#include "runtime.h"
static int equal(const uint8_t *a,const uint8_t *b,size_t n){for(size_t i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static void copy(uint8_t *a,const uint8_t *b,size_t n){for(size_t i=0;i<n;i++)a[i]=b[i];}
void fps_runtime_init(FpsRuntime *s,const uint8_t stock[8],const uint8_t clock[5],const uint8_t experiment[8]){
 *s=(FpsRuntime){0};copy(s->stock,stock,8);copy(s->clock,clock,5);
 copy(s->experiment,experiment,8);
}
int fps_runtime_supports_target(unsigned target){
 return target==FPS_TARGET_UNLIMITED||target==60||target==90||target==120||target==144||target==165||target==240;
}
FpsResult fps_runtime_request(FpsRuntime *s,unsigned target,uint64_t now,unsigned duration){
 if(!s||s->fatal)return FPS_BAD_ARGUMENT;
 if(target==60){s->target=0;s->enabled=0;s->session=0;s->deadline_ms=now;return FPS_OK;}
 if(!fps_runtime_supports_target(target)||!duration||duration>60000||now>UINT64_MAX-duration||now<s->last_ms)return FPS_BAD_ARGUMENT;
 s->target=target;s->enabled=1;s->session=0;s->deadline_ms=now+duration;s->last_ms=now;return FPS_OK;
}
FpsResult fps_runtime_select(FpsRuntime *s,unsigned target,uint64_t now){
 if(!s||s->fatal||!fps_runtime_supports_target(target)||now<s->last_ms)return FPS_BAD_ARGUMENT;
 s->target=target==60?0:target;s->enabled=target!=60;s->session=1;
 s->deadline_ms=0;s->last_ms=now;return FPS_OK;
}
FpsResult fps_runtime_checkpoint(FpsRuntime *s,const FpsBackend *b,uint64_t now){
 if(!s||!b||!b->enter||!b->leave||!b->patch)return FPS_BAD_ARGUMENT;
 if(s->fatal)return FPS_FATAL_HELD;
 if(!s->owned&&!s->enabled)return FPS_DISABLED;
 FpsCheckpoint c={0};if(!b->enter(b->context,&c))return FPS_NO_CHECKPOINT;
 if(!c.lease||!c.code||!c.clock_guard||!c.timer){s->fatal=1;return FPS_FATAL_HELD;}
 FpsResult bad=s->owned?FPS_FATAL_HELD:FPS_CONFLICT;
 int valid_mode=c.scene_mode==1||c.scene_mode==2;
 const uint8_t *expected=s->owned?s->experiment:s->stock;
 if(!valid_mode||!equal(c.code,expected,8)||!equal(c.clock_guard,s->clock,5)||
    (s->owned&&c.timer!=s->owned_timer)){
  if(s->owned){s->fatal=1;return bad;}b->leave(b->context,&c);return bad;
 }
 uint32_t before=*c.timer;
 uint32_t stock_interval=c.scene_mode==2?33332u:16666u;
 if(before!=stock_interval&&(!s->owned||before!=s->owned_interval)){
  if(s->owned){s->fatal=1;return FPS_FATAL_HELD;}b->leave(b->context,&c);return FPS_CONFLICT;
 }
 /* Clock rollback or expiry disables the experiment instead of extending it. */
 if(now<s->last_ms||(!s->session&&now>=s->deadline_ms)){s->target=0;s->enabled=0;}
 s->last_ms=now;
 int high=s->enabled&&c.scene_mode==1;
 const uint8_t *wanted=high?s->experiment:s->stock;
 uint32_t interval=high?(s->target==FPS_TARGET_UNLIMITED?0u:1000000u/s->target):stock_interval;
 if(!equal(expected,wanted,8)){
  int result=b->patch(b->context,&c,expected,wanted);
  if(result<0){s->fatal=1;return FPS_FATAL_HELD;}
  if(!result){b->leave(b->context,&c);return FPS_WRITE_FAILED;}
 }
 if(!equal(c.code,wanted,8)){s->fatal=1;return FPS_FATAL_HELD;}
 *c.timer=interval;
 /* Backend's enter guarantees valid writable memory. Readback detects a
    broken adapter or competing writer; holding lease prevents continuation. */
 if(*c.timer!=interval){s->fatal=1;return FPS_FATAL_HELD;}
 s->owned=(unsigned)high;s->owned_timer=high?c.timer:0;s->owned_interval=high?interval:0;
 b->leave(b->context,&c);return high?FPS_OK:FPS_DISABLED;
}
