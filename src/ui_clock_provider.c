#include "ui_clock_provider.h"
#include "aot_mod.h"
AOT_API unsigned long __stdcall GetCurrentThreadId(void);
#define FIELD(o,t,n) (*(t *)((uint8_t *)(o)+(n)))
int ui_clock_read_native(void *context,void *root,UiClockSample *out){
    UiClockProvider *p=context;
    if(!p || !out || !p->image || !p->permit || !p->permit(p->permit_context) ||
       GetCurrentThreadId()!=p->owner_thread_id)return 0;
    /* Read only after prepare proves this exact mapped executable identity. */
    if(root!=FIELD(p->image,void *,0xc209f0) &&
       root!=FIELD(p->image,void *,0xc209e8) &&
       root!=FIELD(p->image,void *,0xc20a08))return 0;
    out->authorized=out->owner_thread=out->current_frame=1;
    out->native_mode=FIELD(p->image,unsigned,0xc7a468);
    out->pacing_mode=FIELD(p->image,unsigned,0xc48d78);
    out->sim_seconds=FIELD(p->image,float,0xc7a420);
    out->elapsed_seconds=FIELD(p->image,float,0xc7a424);
    out->time_scale=FIELD(p->image,float,0xc7a428);
    return 1;
}
