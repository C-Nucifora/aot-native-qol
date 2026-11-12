#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "ui_clock.h"
static float received;static unsigned calls;
static int native_update(void *root,float seconds){(void)root;received=seconds;calls++;return 37;}
static int read_clock(void *context,void *root,UiClockSample *out){(void)root;*out=*(UiClockSample *)context;return 1;}
static int near(float a,float b){return fabsf(a-b)<0.000001f;}
int main(void){
    UiClockSample sample={1./240,1.f/60,1,1,1,1,1,1};
    uintptr_t table[4]={0,0,0,(uintptr_t)native_update};void *root=table;
    UiClockAdapter adapter={read_clock,&sample};
    for(unsigned rate=60;rate<=240;rate+=60){
        sample.elapsed_seconds=1./rate;double total=0;
        for(unsigned i=0;i<rate;i++)total+=ui_clock_seconds(1.f/60,&sample);
        assert(fabs(total-1)<0.000001);
    }
    sample.elapsed_seconds=1./240;
    assert(ui_clock_update(&adapter,&root,1.f/60)==37&&calls==1&&near(received,1.f/240));
    /* Already measured / pacing0 / native30 / paused UI remain unchanged. */
    assert(near(ui_clock_seconds(1.f/240,&sample),1.f/240));
    sample.pacing_mode=0;assert(near(ui_clock_seconds(1.f/60,&sample),1.f/60));sample.pacing_mode=1;
    sample.native_mode=2;assert(near(ui_clock_seconds(1.f/30,&sample),1.f/30));sample.native_mode=1;
    sample.time_scale=0;assert(ui_clock_seconds(0,&sample)==0);
    sample.time_scale=.25f;assert(near(ui_clock_seconds(.25f/60,&sample),.25f/240));
    sample.time_scale=1;sample.sim_seconds=1.f/240;
    assert(near(ui_clock_seconds(1.f/240,&sample),1.f/240));sample.sim_seconds=1.f/60;
    sample.current_frame=0;assert(near(ui_clock_seconds(1.f/60,&sample),1.f/60));sample.current_frame=1;
    sample.authorized=0;assert(near(ui_clock_seconds(1.f/60,&sample),1.f/60));sample.authorized=1;
    sample.owner_thread=0;assert(near(ui_clock_seconds(1.f/60,&sample),1.f/60));sample.owner_thread=1;
    sample.elapsed_seconds=NAN;assert(near(ui_clock_seconds(1.f/60,&sample),1.f/60));
    sample.elapsed_seconds=INFINITY;assert(near(ui_clock_seconds(1.f/60,&sample),1.f/60));
    sample.elapsed_seconds=-1;assert(near(ui_clock_seconds(1.f/60,&sample),1.f/60));
    sample.elapsed_seconds=.2;assert(near(ui_clock_seconds(1.f/60,&sample),.05));
    sample.elapsed_seconds=0;assert(ui_clock_seconds(1.f/60,&sample)==0);
    puts("UI clock host cases passed: real-time duration, no double correction, native30, pause/time scale, permission/frame gates");
}
