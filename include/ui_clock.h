#ifndef AOT_UI_CLOCK_H
#define AOT_UI_CLOCK_H
#include <stdint.h>
#if defined(_WIN32)
#define UI_CLOCK_ABI __fastcall
#else
#define UI_CLOCK_ABI
#endif
typedef struct {
    double elapsed_seconds;
    float sim_seconds,time_scale;
    unsigned authorized,owner_thread,current_frame,native_mode,pacing_mode;
} UiClockSample;
typedef int (*UiClockRead)(void *,void *root,UiClockSample *);
typedef int (UI_CLOCK_ABI *UiNativeUpdate)(void *,float);
typedef struct { UiClockRead read;void *context; } UiClockAdapter;
/* Called only at the three proven UI root virtual18 callsites, after /60.
 * Native simulation/scene arguments and global clocks are never modified. */
float ui_clock_seconds(float incoming,const UiClockSample *);
int ui_clock_update(UiClockAdapter *,void *root,float incoming);
#endif
