#ifndef AOT_SETTINGS_H
#define AOT_SETTINGS_H
/* Pure state reducer; no live hooks, timing edits or config writes. */
typedef struct { unsigned committed,pending,original; unsigned applying; } AotFpsState;
void aot_fps_open(AotFpsState *,unsigned persisted);
void aot_fps_defaults(AotFpsState *);
void aot_fps_cancel(AotFpsState *);
void aot_fps_apply_begin(AotFpsState *);
/* Returns1 only when final outcome permits persisting committed target. */
int aot_fps_apply_finish(AotFpsState *,int display_recovery_succeeded);
#endif
