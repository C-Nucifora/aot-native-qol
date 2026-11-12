#ifndef AOT_FPS_SESSION_H
#define AOT_FPS_SESSION_H
#include "aot_mod.h"
#include "aot_fps_menu.h"
/* Owner/UI checkpoint only.0 pending,1 stock ready; conflicts fail-stop. */
int aot_fps_session_display_ready(void);
/* Accepted request returns a nonzero ticket. No selection deadline. */
int aot_fps_session_select(unsigned target,uint64_t *ticket);
/*0 pending,1 matching confirmed,-1 matching failed or superseded. */
int aot_fps_session_outcome(uint64_t ticket);
void aot_fps_session_display_finished(void);
/* Validated pending/suspended/confirmed ticket, never an inferred activation. */
int aot_fps_session_queued(uint64_t ticket);
/* Process-lifetime handoff: no native menu/config pointer is borrowed. A newer
 * explicit Apply may supersede an older pending ticket on the same owner. */
int aot_fps_session_defer(uint64_t,unsigned requested,unsigned retained,const uint16_t *path,int publish);
int aot_fps_session_defer_rollback(uint64_t,unsigned requested,unsigned retained,AotSettingsResult);
void aot_fps_session_commit_poll(void);
int aot_fps_session_commit_busy(void);
int aot_fps_session_take_error(AotFpsMenuError *);
/* Stable fatal-site identifiers. Age of an owner heartbeat is not fatal. */
enum {
 AOT_FPS_FAIL_UNSPECIFIED=1, AOT_FPS_FAIL_LOAD_REQUEST=2,
 AOT_FPS_FAIL_OWNER_STATUS=3, AOT_FPS_FAIL_OUTCOME_STATUS=4,
 AOT_FPS_FAIL_DISPLAY_GATE=5, AOT_FPS_FAIL_DISPLAY_RELEASE=6,
 AOT_FPS_FAIL_STARTUP_ROLLBACK=7, AOT_FPS_FAIL_COMMIT_ROLLBACK=8,
 AOT_FPS_FAIL_COMMIT_REQUEST=9, AOT_FPS_FAIL_TEARDOWN_REQUEST=10,
 AOT_FPS_FAIL_MENU_REQUEST=11, AOT_FPS_FAIL_MENU_ROLLBACK=12,
 AOT_FPS_FAIL_BACKEND=100
};
_Noreturn void aot_fps_session_fail(void);
_Noreturn void aot_fps_session_fail_reason(unsigned reason);
AotStatus aot_fps_session_start(void *self);
#endif
