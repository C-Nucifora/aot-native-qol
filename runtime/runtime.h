#ifndef AOT_FPS_EXPERIMENT_RUNTIME_H
#define AOT_FPS_EXPERIMENT_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
#define FPS_TARGET_UNLIMITED 0u
/* Staged experiment ABI. No installer, hook, default enabling or persistence. */
typedef enum { FPS_OK, FPS_DISABLED, FPS_BAD_ARGUMENT, FPS_NO_CHECKPOINT,
               FPS_CONFLICT, FPS_WRITE_FAILED, FPS_FATAL_HELD } FpsResult;
typedef struct {
    uint8_t *code;             /* validated image base + private manifest RVA */
    const uint8_t *clock_guard; /* validated untouched clock/clamp guard */
    volatile uint32_t *timer;  /* validated writable [[base+c7a460]+8]+16 */
    unsigned scene_mode;       /* clock object's +58, only 1 or 2 */
    uintptr_t lease;            /* backend-owned thread/checkpoint lease */
} FpsCheckpoint;
typedef struct {
    void *context;
    /* Enter serializes writers, verifies module identity, resolves timer safely,
       and proves no thread IP can execute patched region during transaction.
       A main-thread callback alone is insufficient evidence of quiescence. */
    int (*enter)(void *, FpsCheckpoint *);
    void (*leave)(void *, FpsCheckpoint *);
    /* Transaction writes exactly 8 bytes. Success means protect/cache/restore
       passed. 0 means fully restored failure. -1 means uncertain/fatal. */
    int (*patch)(void *, FpsCheckpoint *, const uint8_t *, const uint8_t *);
} FpsBackend;
typedef struct {
    unsigned target, enabled, session, owned, fatal;
    uint32_t owned_interval;
    volatile uint32_t *owned_timer;
    uint64_t deadline_ms, last_ms;
    uint8_t stock[8], experiment[8], clock[5];
} FpsRuntime;
void fps_runtime_init(FpsRuntime *, const uint8_t stock[8], const uint8_t clock[5], const uint8_t experiment[8]);
/* Caller must obtain explicit experimental checkpoint authorization first.
   Bounded 90/120/144/165/240 are accepted. 60 requests rollback.
   Interval is floor(1000000/target) microseconds, not measured present FPS. */
/* Accepted experiment values only; this is not gameplay validation. */
int fps_runtime_supports_target(unsigned target);
FpsResult fps_runtime_request(FpsRuntime *, unsigned, uint64_t now_ms, unsigned duration_ms);
/* Session selection has no duration or renewal.0 is uncapped,60 is stock. */
FpsResult fps_runtime_select(FpsRuntime *,unsigned target,uint64_t now_ms);
FpsResult fps_runtime_checkpoint(FpsRuntime *, const FpsBackend *, uint64_t now_ms);
/* Fatal keeps checkpoint HELD. Supervisor must fail-stop; never continue game,
   unload module or claim restoration. A stopped frame cannot run owner rollback;
   caller must arrange an external watchdog/coordinated shutdown checkpoint. */
#endif
