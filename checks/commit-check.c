#include <assert.h>
#include <string.h>
#include "aot_fps_session.h"
#include "aot_fps_capabilities.h"
static uint64_t current;
static unsigned selected,stored,stores;
static int outcome,store_fail,queued=1;
int aot_fps_session_queued(uint64_t ticket){return queued && ticket==current;}
int aot_fps_session_select(unsigned target,uint64_t *ticket){selected=target;*ticket=++current;outcome=0;return 1;}
int aot_fps_session_outcome(uint64_t ticket){return ticket==current?outcome:-1;}
_Noreturn void aot_fps_session_fail(void){assert(!"unexpected rollback failure");__builtin_trap();}
_Noreturn void aot_fps_session_fail_reason(unsigned reason){(void)reason;aot_fps_session_fail();}
AotSettingsResult aot_settings_store(const uint16_t *path,unsigned target){
 assert(path[0]=='C'&&path[1]==':'&&path[2]=='\\'&&path[3]=='a'&&path[4]==0);
 stores++;if(store_fail)return AOT_SETTINGS_IO_ERROR;stored=target;return AOT_SETTINGS_OK;
}
int main(void){
 uint16_t path[]={'C',':','\\','a',0};AotFpsMenuError e;
 current=1;assert(aot_fps_session_defer(current,0,60,path,1));path[3]='z';
 assert(aot_fps_session_commit_busy());aot_fps_session_commit_poll();assert(!stores);
 outcome=1;aot_fps_session_commit_poll();assert(stores==1&&stored==0&&!aot_fps_session_commit_busy());
 path[3]='a';current++;outcome=0;assert(aot_fps_session_defer(current,240,0,path,1));
 /* New explicit selection replaces an unconfirmed prior ticket. */
 current++;assert(aot_fps_session_defer(current,144,0,path,1));outcome=1;aot_fps_session_commit_poll();assert(stored==144&&stores==2);
 current++;outcome=0;assert(aot_fps_session_defer(current,240,144,path,1));
 store_fail=1;outcome=1;aot_fps_session_commit_poll();assert(stores==3&&selected==144&&aot_fps_session_commit_busy());
 assert(!aot_fps_session_take_error(&e));aot_fps_session_commit_poll();assert(!aot_fps_session_take_error(&e));
 outcome=1;aot_fps_session_commit_poll();assert(!aot_fps_session_commit_busy()&&aot_fps_session_take_error(&e));
 assert(e.error==AOT_SETTINGS_IO_ERROR&&e.requested_target==240&&e.retained_target==144&&!aot_fps_session_take_error(&e));
 store_fail=0;current++;outcome=0;assert(aot_fps_session_defer(current,90,144,path,1));
 outcome=-1;aot_fps_session_commit_poll();assert(selected==144&&stores==3);outcome=1;aot_fps_session_commit_poll();
 assert(aot_fps_session_take_error(&e)&&e.error==AOT_SETTINGS_RUNTIME_ERROR);
 /* Native display recovery confirms retained policy but never rewrites config. */
 current++;outcome=1;assert(aot_fps_session_defer(current,144,144,path,0));aot_fps_session_commit_poll();assert(stores==3);
 queued=0;assert(!aot_fps_session_defer(current,240,144,path,1)&&!aot_fps_session_commit_busy());
 return 0;
}
