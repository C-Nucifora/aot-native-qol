#include "aot_settings.h"
#include "aot_fps_capabilities.h"
/* Pending edits do not change the owner-thread runtime until final Apply. */
static unsigned normalize(unsigned value) { return aot_fps_normalize(value); }
void aot_fps_open(AotFpsState *s,unsigned persisted) {
    s->committed=normalize(persisted);s->pending=s->committed;s->original=s->committed;s->applying=0;
}
void aot_fps_defaults(AotFpsState *s) { s->pending=60; }
void aot_fps_cancel(AotFpsState *s) { s->pending=s->original;s->applying=0; }
void aot_fps_apply_begin(AotFpsState *s) { s->applying=1; }
int aot_fps_apply_finish(AotFpsState *s,int success) {
    if(!s->applying)return 0;s->applying=0;
    if(!success){s->pending=s->committed;return 0;}
    s->committed=normalize(s->pending);s->pending=s->committed;s->original=s->committed;return 1;
}
