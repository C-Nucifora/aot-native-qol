#ifndef AOT_REPEAT_ADAPTER_H
#define AOT_REPEAT_ADAPTER_H
#include <stdint.h>
#if defined(_WIN32)
#define REPEAT_ABI __fastcall
#else
#define REPEAT_ABI
#endif
/* Original functions remain callable; adapter installs no hooks. */
typedef void (REPEAT_ABI *RepeatKeyboard)(void *, float);
typedef int (REPEAT_ABI *RepeatController)(void *, void *, unsigned);
/* Return true ONLY on the verified owner thread, authorized high-FPS session,
 * native mode 1 and verified pacing 0/1. Menu suspension of the timer target
 * must not disable repeat normalization if the menu still presents above 60.
 * Supply measured c7a424 seconds for this engine frame, capped at 50 ms;
 * c7a420 can be forced to 1/60 by stock menu pacing and is unsuitable here.
 * Selected-target state alone does not establish thread/clock/pacing validity. */
typedef int (*RepeatClock)(void *, double *);
typedef struct {
    RepeatKeyboard keyboard;
    RepeatController controller;
    RepeatClock clock;
    void *clock_context;
    struct { void *state; double fraction; } lanes[2];
} RepeatAdapter;
void repeat_keyboard(RepeatAdapter *, void *, float);
int repeat_controller(RepeatAdapter *, void *, void *, unsigned);
void repeat_reset(RepeatAdapter *);
#endif
