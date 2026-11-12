#ifndef AOT_UI_CLOCK_HOOK_H
#define AOT_UI_CLOCK_HOOK_H
#include "aot_mod.h"
#include "ui_clock_provider.h"
typedef struct {uint8_t roots[3][32],divide[16],clock[32];} UiClockPrivateGuards;
typedef struct {
    UiClockAdapter adapter;
    UiClockProvider provider;
    uint8_t *image,*relay;
    void *pin;
    AotPatch calls[3];
    unsigned prepared,retained;
} UiClockHook;
/* Static process-lifetime owner, pinned DLL and immutable permit context.
 * Prepare only: caller reviews and applies all3 calls atomically while quiesced.
 * Retain stage/relay/DLL after any failure until caller proves safe removal. */
AotStatus ui_clock_prepare(UiClockHook *,uint8_t *,size_t,const UiClockPrivateGuards *,unsigned owner_thread,UiClockPermit,void *);
#endif
