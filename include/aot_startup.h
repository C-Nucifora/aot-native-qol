#ifndef AOT_STARTUP_H
#define AOT_STARTUP_H
#include "aot_mod.h"
/* External checkpoint contract: outside loader lock, the game initialization
 * thread exclusively owns display/settings/UI code, Steam atlas locale has
 * been resolved, and no UIOption resources or menu objects exist yet.
 * This API does not itself establish that ordering or install a hook. */
AotStatus aot_startup_resolution_at_checkpoint(const uint16_t *texture_path,const uint16_t *layout_path);
/* 0 untouched,1 preparing,2 active,3 safely refused,4 retained after failure. */
unsigned aot_startup_phase(void);
/* Initialization-failure rollback only, before game/menu observers resume. */
AotStatus aot_startup_rollback_at_checkpoint(void);
#endif
