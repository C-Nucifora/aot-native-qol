#ifndef AOT_FPS_MENU_H
#define AOT_FPS_MENU_H
#include <stddef.h>
#include <stdint.h>
#include "aot_native_fps_state.h"
/* Values only: the callback receives no borrowed menu/config/native pointers.
 * It must copy/queue the event and return, without reentering native UI. The
 * coordinator presents the queued error after update returns, outside teardown. */
typedef struct {
    AotSettingsResult error;
    unsigned requested_target,retained_target;
} AotFpsMenuError;
typedef void (*AotFpsMenuErrorCallback)(AotFpsMenuError);
/* Configure before constructor publication; retain all value_count contiguous
 * immutable0x78 descriptors through last instance and any failed rollback.
 * States18/28/38 mean dim/normal/focused; preserve stock48..77 arrow tail. */
typedef struct {
    uint8_t *image;
    size_t image_size;
    const uint16_t *config_path;
    const uint8_t *value_sprites;
    AotFpsMenuErrorCallback on_error;
    int32_t value_x,value_width;
    unsigned value_count;
} AotFpsMenuBinding;
int aot_fps_menu_bind(const AotFpsMenuBinding *);
/* Replacement for original constructor call ONLY after its exact guard passes.
 * Calls original constructor before publishing per-instance association. */
void *aot_fps_menu_constructor(void *,unsigned);
unsigned aot_fps_menu_instances(void);
AotSettingsResult aot_fps_menu_last_error(void *menu);
#endif
