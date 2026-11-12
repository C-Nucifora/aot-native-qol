#ifndef AOT_FPS_TRIAL_H
#define AOT_FPS_TRIAL_H
#include "aot_mod.h"
/* Explicit file opt-in only; no target is persisted by this interface.
 * Startup is the existing supported, unpacked, pinned loader checkpoint. */
AotStatus aot_fps_trial_start(void *self);
/* Returns0 while owner-checkpoint rollback is pending. Never spin in Apply. */
int aot_fps_trial_display_ready(void);
#endif
