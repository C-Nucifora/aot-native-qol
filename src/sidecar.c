#include "aot_sidecar.h"
int aot_sidecar_allocate(AotSidecar *s,void *widget) {
    if(!s || !widget || s->phase!=AOT_SIDECAR_EMPTY || s->widget)return 0;
    s->widget=widget;s->phase=AOT_SIDECAR_ALLOCATED;s->borrowed=0;return 1;
}
int aot_sidecar_advance(AotSidecar *s,AotSidecarPhase next) {
    if(!s || !s->widget || s->phase<AOT_SIDECAR_ALLOCATED ||
       s->phase>=AOT_SIDECAR_PUBLISHED || next!=s->phase+1)return 0;
    s->phase=next;
    if(next==AOT_SIDECAR_PUBLISHED)s->borrowed=1;
    return 1;
}
void aot_sidecar_main_cleared(AotSidecar *s) { if(s)s->borrowed=0; }
unsigned aot_sidecar_take_cleanup(AotSidecar *s,void **widget) {
    if(!s || !widget || !s->widget || s->borrowed)return 0;
    unsigned actions=s->phase>=AOT_SIDECAR_CONSTRUCTED?AOT_SIDECAR_DELETE:AOT_SIDECAR_FREE_RAW;
    if(s->phase>=AOT_SIDECAR_INITIALIZED)actions|=AOT_SIDECAR_CLEANUP;
    *widget=s->widget;s->widget=0;s->phase=AOT_SIDECAR_EMPTY;s->borrowed=0;
    return actions;
}
