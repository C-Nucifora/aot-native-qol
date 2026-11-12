#ifndef AOT_UI_CLOCK_PROVIDER_H
#define AOT_UI_CLOCK_PROVIDER_H
#include "ui_clock.h"
/* Permit must prove authorized high-FPS session AND a current-frame clock
 * sample on the verified owner thread. Timer/menu suspension alone must not
 * revoke permission. It must perform no UI update, input or simulation writes. */
typedef int (*UiClockPermit)(void *);
typedef struct {
    uint8_t *image;
    unsigned owner_thread_id;
    UiClockPermit permit;
    void *permit_context;
} UiClockProvider;
int ui_clock_read_native(void *,void *root,UiClockSample *);
#endif
