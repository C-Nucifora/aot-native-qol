#include "ui_clock.h"
static float absolute(float x){return x<0?-x:x;}
float ui_clock_seconds(float incoming,const UiClockSample *s){
    const float fixed60=1.0f/60.0f;
    if(!s || !s->authorized || !s->owner_thread || !s->current_frame ||
       s->native_mode!=1 || s->pacing_mode!=1 ||
       !(s->elapsed_seconds>=0 && s->elapsed_seconds<=1) || !(s->time_scale>0 && s->time_scale<=16) ||
       !(incoming>0) || !(absolute(s->sim_seconds-fixed60)<=0.0000001f))return incoming;
    float expected=s->sim_seconds*s->time_scale;
    /* Already-corrected or independently transformed UI deltas pass through. */
    if(!(absolute(incoming-expected)<=0.0000001f*(1+s->time_scale)))return incoming;
    double seconds=s->elapsed_seconds;
    if(seconds>0.05)seconds=0.05;
    return (float)(seconds*s->time_scale);
}
int ui_clock_update(UiClockAdapter *a,void *root,float incoming){
    UiClockSample sample={0};
    if(a->read && a->read(a->context,root,&sample))incoming=ui_clock_seconds(incoming,&sample);
    /* Preserve actual virtual target and original invocation count. */
    uintptr_t *vtable=*(uintptr_t **)root;
    return ((UiNativeUpdate)vtable[0x18/8])(root,incoming);
}
