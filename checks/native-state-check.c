#include <assert.h>
#include "aot_native_fps_state.h"
static AotNativeFpsSession session;
static unsigned writes,restore_count,commit_count,load_missing,save_failed;
static void original(void *menu){assert(menu==(void *)1);}
static void commit(void *menu){original(menu);commit_count++;}
static void restore(void *menu){original(menu);restore_count++;}
static int unchanged(void *menu){original(menu);return 1;}
static uintptr_t recovered(void *menu){assert(menu==(void *)1);aot_native_fps_restore(&session,restore);aot_native_fps_commit(&session,commit);return 1;}
static uintptr_t success(void *menu){original(menu);return 3;}
AotSettingsResult aot_settings_load(const uint16_t *path,unsigned *target){(void)path;*target=60;return load_missing?AOT_SETTINGS_MISSING:AOT_SETTINGS_OK;}
AotSettingsResult aot_fps_apply_persist(AotFpsState *s,int okay,const uint16_t *path){(void)path;if(okay){writes++;if(save_failed){s->pending=s->committed;s->applying=0;return AOT_SETTINGS_IO_ERROR;}}aot_fps_apply_finish(s,okay);return AOT_SETTINGS_OK;}
int main(void){
 session.menu=(void *)1;load_missing=1;aot_native_fps_load(&session,original);assert(session.last_error==AOT_SETTINGS_OK && !writes);
 aot_native_fps_capture(&session,original);assert(aot_native_fps_unchanged(&session,unchanged));
 aot_native_fps_commit(&session,commit);assert(!writes && session.fps.applying);
 assert(aot_native_fps_finish_apply(&session,recovered)==1 && !writes && !session.fps.applying && restore_count==1 && commit_count==2);
 aot_native_fps_commit(&session,commit);assert(aot_native_fps_finish_apply(&session,success)==3 && writes==1 && !session.fps.applying);
 aot_native_fps_restore(&session,restore);assert(!session.fps.applying);
 save_failed=1;aot_native_fps_commit(&session,commit);aot_native_fps_finish_apply(&session,success);assert(session.last_error==AOT_SETTINGS_IO_ERROR && !session.fps.applying);
 return 0;
}
