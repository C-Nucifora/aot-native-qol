#ifndef AOT_FPS_NATIVE_EXPERIMENT_H
#define AOT_FPS_NATIVE_EXPERIMENT_H
#include "runtime.h"
#include "aot_mod.h"
typedef struct { uint32_t rva,length;const uint8_t *bytes; } FpsFlowGuard;
typedef struct { uint32_t rva;uint8_t expected[5]; } FpsModeCallGuard;
#define FPS_NATIVE_MODE_CALLS 12u
#define FPS_NATIVE_HOOK_COUNT 13u
typedef struct {
 uint8_t timing[8],clock[5],timestamp[4],wait_call[6],simulation_cap[4];
 FpsFlowGuard flow[6];FpsModeCallGuard mode_calls[FPS_NATIVE_MODE_CALLS];
} FpsPrivateGuards;
/* Prepare builds private near thunk and hook operand only, never patches game.
   Caller verifies original file SHA256 and holds existing module lifetime pin.
   Only the exact reviewed exe/module is supported. */
int fps_native_prepare(uint8_t *base,size_t image_size,const FpsPrivateGuards *);
int fps_native_hook_patch(AotPatch *out);
/* Combined wait+12 observer hooks. No partial output or implicit installation.
   Caller applies/reverts the whole array at its startup/shutdown checkpoint. */
size_t fps_native_hook_patches(AotPatch *out,size_t capacity);
typedef enum {
 FPS_HOLD_AFTER_NATIVE_MODE=0,
 FPS_RESUME_WHEN_NATIVE_60=1
} FpsResumePolicy;
typedef enum {
 FPS_SESSION_STOCK, FPS_SESSION_PENDING, FPS_SESSION_ACTIVE,
 FPS_SESSION_SUSPENDED, FPS_SESSION_FAILED, FPS_SESSION_FATAL
} FpsSessionPhase;
typedef enum {
 FPS_REQUEST_NONE, FPS_REQUEST_PENDING, FPS_REQUEST_CONFIRMED, FPS_REQUEST_FAILED
} FpsRequestOutcome;
typedef struct {
 uint64_t request_id,completed_id,heartbeat_ms;
 unsigned selected_target,effective_target;
 FpsSessionPhase phase;
 FpsRequestOutcome outcome;
 FpsResult last_result;
 FpsResumePolicy resume_policy;
 unsigned watchdog_stop_requested;
} FpsSessionStatus;
/* Owner-thread Apply/load request. No duration or selection expiry.
   Accepted request receives a nonzero ticket; poll status and persist only
   when request_id==completed_id==ticket and outcome==CONFIRMED.
   effective_target is the applied timer policy, never measured present FPS. */
FpsResult fps_native_set_selected_target(unsigned target,FpsResumePolicy,uint64_t *ticket);
/* Owner/menu coordinator poll may CONFIRM stock60 by read-only guard checks
   even when pacing1 skips the timer wait. It never applies a high target.
   High/Unlimited PENDING may close the menu; confirm only after genuinewait0. */
FpsResult fps_native_poll_owner(void);
/* Safe snapshot for menu/counter/watchdog readers;0 means no stable snapshot. */
int fps_native_get_status(FpsSessionStatus *out);
typedef enum {
 FPS_WATCHDOG_INACTIVE, FPS_WATCHDOG_HEALTHY,
 FPS_WATCHDOG_STOP_REQUESTED, FPS_WATCHDOG_BAD_ARGUMENT
} FpsWatchdogResult;
/* Foreign-thread monitor only requests stop after an ACTIVE owner heartbeat
   stalls. Only the next owner checkpoint may restore timing. A permanently
   hung owner still needs supervised process shutdown, never foreign writes. */
FpsWatchdogResult fps_native_watchdog_poll(unsigned stall_ms);
/* Explicit bounded experiment entry. Default initialized target is stock. */
FpsResult fps_native_begin_experiment(unsigned target,unsigned duration_ms);
FpsResult fps_native_request_stock(void);
/* Display Apply gate:1 stock ready,0 rollback pending,-1 ownership/conflict.
   On0, DEFER the display mutation and return to frame loop. Never spin/wait
   inside Apply: the next original pacing checkpoint performs rollback. Retry
   on a later frame before invoking the original display/VSync change. */
int fps_native_before_display_change(void);
/* Final Apply/recovery callback releases the gate. Restores only the selected
   policy explicitly supplied by set_selected_target. Never call while the
   original display mutation/recovery is still in progress. */
FpsResult fps_native_after_display_change(void);
/* Do not free near allocations until hook + timing patches are both removed
   under an independent startup/shutdown quiescence checkpoint. No unloader is
   supplied: retained private allocations are safer than an uncertain free. */
#endif
