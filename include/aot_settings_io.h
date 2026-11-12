#ifndef AOT_SETTINGS_IO_H
#define AOT_SETTINGS_IO_H
#include <stddef.h>
#include <stdint.h>
#include "aot_settings.h"
typedef enum {
    AOT_SETTINGS_OK, AOT_SETTINGS_MISSING, AOT_SETTINGS_INVALID,
    AOT_SETTINGS_IO_ERROR,AOT_SETTINGS_RUNTIME_ERROR
} AotSettingsResult;
/* Absolute mod-owned config path; temporary file is path + ".tmp". */
AotSettingsResult aot_settings_load(const uint16_t *path,unsigned *fps);
AotSettingsResult aot_settings_store(const uint16_t *path,unsigned fps);
/* Final Apply only. Failed IO leaves committed state unchanged and cancels pending. */
AotSettingsResult aot_fps_apply_persist(AotFpsState *,int display_success,const uint16_t *path);
#endif
