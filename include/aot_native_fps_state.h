#ifndef AOT_NATIVE_FPS_STATE_H
#define AOT_NATIVE_FPS_STATE_H
#include <stdint.h>
#include "aot_settings_io.h"
/* Native callback adapter state. Widget allocation, labels and detours are not
 * installed by these functions. Timing is managed separately by the owner-checkpoint session adapter. */
typedef struct {
    void *menu;
    const uint16_t *config_path;
    AotFpsState fps;
    unsigned display_restored;
    AotSettingsResult last_error;
} AotNativeFpsSession;
void aot_native_fps_load(AotNativeFpsSession *,void (*original)(void *));
void aot_native_fps_capture(AotNativeFpsSession *,void (*original)(void *));
void aot_native_fps_restore(AotNativeFpsSession *,void (*original)(void *));
void aot_native_fps_defaults(AotNativeFpsSession *,void (*original)(void *));
void aot_native_fps_commit(AotNativeFpsSession *,void (*original)(void *));
int aot_native_fps_unchanged(AotNativeFpsSession *,int (*original)(void *));
/* Original37cf30 return is not a success boolean. Restore hook records recovery.
 * UI caller must surface last_error after the stock Apply sequence returns. */
uintptr_t aot_native_fps_finish_apply(AotNativeFpsSession *,uintptr_t (*original)(void *));
#endif
