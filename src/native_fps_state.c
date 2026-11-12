#include "aot_native_fps_state.h"
void aot_native_fps_load(AotNativeFpsSession *s,void (*original)(void *)){
    original(s->menu);unsigned target=60;
    s->last_error=aot_settings_load(s->config_path,&target);
    if(s->last_error==AOT_SETTINGS_MISSING)s->last_error=AOT_SETTINGS_OK;
    aot_fps_open(&s->fps,target);s->display_restored=0;
}
void aot_native_fps_capture(AotNativeFpsSession *s,void (*original)(void *)){
    original(s->menu);s->fps.original=s->fps.pending;
}
void aot_native_fps_restore(AotNativeFpsSession *s,void (*original)(void *)){
    original(s->menu);
    if(s->fps.applying){s->display_restored=1;s->fps.pending=s->fps.committed;}
    else aot_fps_cancel(&s->fps);
}
void aot_native_fps_defaults(AotNativeFpsSession *s,void (*original)(void *)){
    original(s->menu);aot_fps_defaults(&s->fps);
}
void aot_native_fps_commit(AotNativeFpsSession *s,void (*original)(void *)){
    /* Recovery invokes commit again. Never begin a fresh transaction there. */
    if(!s->fps.applying){aot_fps_apply_begin(&s->fps);s->display_restored=0;}
    original(s->menu);
}
int aot_native_fps_unchanged(AotNativeFpsSession *s,int (*original)(void *)){
    return original(s->menu) && s->fps.pending==s->fps.original;
}
uintptr_t aot_native_fps_finish_apply(AotNativeFpsSession *s,uintptr_t (*original)(void *)){
    uintptr_t result=original(s->menu);
#ifndef AOT_FPS_SESSION
    if(s->fps.applying)
        s->last_error=aot_fps_apply_persist(&s->fps,!s->display_restored,s->config_path);
#endif
    return result;
}
