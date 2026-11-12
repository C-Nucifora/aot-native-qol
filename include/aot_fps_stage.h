#ifndef AOT_FPS_STAGE_H
#define AOT_FPS_STAGE_H
#include "aot_mod.h"
#include "aot_fps_menu.h"
/* All calls are owning-UI-thread/quiescent only, outside DllMain/loader lock.
 * start installs load/teardown call hooks BEFORE UI parent construction; load
 * chains native3dfde0 then activates before native2e03c0 creates child menus.
 * Errors use the verified embedded Retry dialog, never a borrowed global menu.
 * Remove stops new construction then requires zero associations before release.
 * Uncertain rollback pins all owners and fail-stops, with no native continuation. */
AotStatus aot_fps_stage_prepare(void *self);
AotStatus aot_fps_stage_activate(void);
AotStatus aot_fps_stage_start(void *self);
AotStatus aot_fps_stage_remove(void);
unsigned aot_fps_stage_phase(void);
void aot_fps_stage_instance_released(void);
#endif
