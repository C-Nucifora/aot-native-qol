#include "aot_timing.h"
#include "aot_owned_files.h"
#ifdef AOT_NAPE_FIX
#include "nape_hook.h"
#include "nape-private-guards.h"
static int NAPE_ABI nape_permit(void *root,unsigned slot,float scaled,float unscaled);
#endif
#include "repeat_adapter.h"
#include "camera_hook.h"
#include "camera-private-guards.h"
#include "repeat-private-guards.h"
#ifdef AOT_UI_CLOCK
#include "ui_clock_hook.h"
#include "ui-clock-private-guards.h"
AOT_API unsigned long __stdcall GetCurrentThreadId(void);
#endif
AOT_API unsigned long long __stdcall GetTickCount64(void);
static struct {
    uint8_t *image,*relay;
    RepeatAdapter repeat;
    CameraHookStage camera;
#ifdef AOT_UI_CLOCK
    UiClockHook ui;
#endif
    AotPatch calls[AOT_TIMING_CALLS];
    uint64_t pad_seen[2];
    unsigned pad_known[2],prepared,armed;
} timing;
static int owned(void){
    if(!timing.armed || timing.image[0x3f2fff]!=0xe8)return 0;
    for(unsigned i=0;i<AOT_TIMING_CALLS;i++){
        AotPatch *p=&timing.calls[i];
        for(unsigned j=0;j<p->length;j++)if(timing.image[p->rva+j]!=p->replacement[j])return 0;
    }
    return 1;
}
void aot_timing_reset(void){
    repeat_reset(&timing.repeat);
    timing.pad_known[0]=timing.pad_known[1]=0;
}
static int repeat_clock(void *unused,double *seconds){
    (void)unused;
    if(!owned() || !aot_fps_session_rate_clock(seconds,1)){aot_timing_reset();return 0;}
    return 1;
}
static void REPEAT_ABI keyboard(void *object,float stock_ticks){
    repeat_keyboard(&timing.repeat,object,stock_ticks);
}
static int REPEAT_ABI controller(void *state,void *device,unsigned stock_ticks){
    unsigned index=2;
    int known=aot_fps_session_pad_index(state,device,&index);
    if(index<2){timing.pad_known[index]=(unsigned)known;timing.pad_seen[index]=GetTickCount64();}
    return repeat_controller(&timing.repeat,state,device,stock_ticks);
}
static int camera_clock(void *unused,const void *context,unsigned axis,float incoming,CameraRateSample *out){
    (void)unused;(void)incoming;double seconds;unsigned index=2;
    if(axis>1 || !owned() || !aot_fps_session_rate_clock(&seconds,0) ||
       !aot_fps_session_camera_index(context,&index) || index>=2 ||
       !timing.pad_known[index])return 0;
    uint64_t now=GetTickCount64();
    if(now<timing.pad_seen[index] || now-timing.pad_seen[index]>100)return 0;
    /* Proof is the actual live native Pad call argument, not a chosen cap or
     * UI controller hints. Relative mouse/unknown/keyboard-only stay native. */
    *out=(CameraRateSample){seconds,1,1,1,0,CAMERA_INPUT_RATE};return 1;
}
#ifdef AOT_UI_CLOCK
static int ui_permit(void *unused){
    (void)unused;
    /* This permit is reached only from the three guarded UI-root callsites.
     * Their frame-order guards follow the current native clock sample. */
    return owned() && aot_fps_session_ui_frame();
}
#endif
#ifdef AOT_NAPE_FIX
static int NAPE_ABI nape_permit(void *root,unsigned slot,float scaled,float unscaled){
    return owned() && aot_fps_session_cutin(root,slot,scaled,unscaled);
}
#endif
AotStatus aot_timing_prepare(uint8_t *base,size_t size,AotPatch out[AOT_TIMING_CALLS]){
    if(!base || !out || timing.prepared || size!=0xfb5000 || base!=GetModuleHandleW(0) || !aot_supported_executable())return AOT_BAD_RANGE;
    for(unsigned i=0;i<sizeof(repeat_guards)/sizeof(repeat_guards[0]);i++){
        const RepeatPrivateGuard *g=&repeat_guards[i];
        for(unsigned j=0;j<g->length;j++)if(base[g->rva+j]!=g->bytes[j])return AOT_SIGNATURE_MISMATCH;
    }
#ifdef AOT_NAPE_FIX
    for(unsigned i=0;i<sizeof(nape_guards)/sizeof(nape_guards[0]);i++){
        const NapePrivateGuard *g=&nape_guards[i];
        for(unsigned j=0;j<g->length;j++)if(base[g->rva+j]!=g->bytes[j])return AOT_SIGNATURE_MISMATCH;
    }
#endif
    timing.image=base;
    timing.repeat=(RepeatAdapter){(RepeatKeyboard)(base+0x3f3640),(RepeatController)(base+0x3f2990),repeat_clock,0,{{0,0},{0,0}}};
    AotStatus status=camera_hook_prepare(&timing.camera,base,size,&camera_private_guards,camera_clock,0);
    if(status!=AOT_OK)return status;
    const uint32_t sites[2]={0x3f3045,0x3f2fff};
    void *entries[2]={(void *)&keyboard,(void *)&controller};
    timing.relay=aot_allocate_near((uintptr_t)base+0x3f3004,(uintptr_t)base+0x3f304a,32);
    if(!timing.relay)return AOT_BAD_RANGE;
    for(unsigned i=0;i<2;i++){
        uint8_t *relay=timing.relay+16*i;
        relay[0]=0xff;relay[1]=0x25;for(unsigned j=2;j<6;j++)relay[j]=0;
        uintptr_t entry=(uintptr_t)entries[i];
        for(unsigned j=0;j<8;j++)relay[6+j]=(uint8_t)(entry>>(8*j));
        AotPatch *p=&timing.calls[i];
        /* Controller CALL starts at page offsetfff. Keep its guarded E8 byte
         * and publish only the next page's rel32 operand in this transaction. */
        unsigned operand_only=i==1;
        p->rva=sites[i]+operand_only;p->length=operand_only?4:5;
        int32_t delta;if(!aot_rel32((uintptr_t)base+sites[i]+5,(uintptr_t)relay,&delta))return AOT_BAD_RANGE;
        for(unsigned j=0;j<p->length;j++)p->expected[j]=base[p->rva+j];
        if(!operand_only)p->replacement[0]=0xe8;
        for(unsigned j=0;j<4;j++)p->replacement[j+!operand_only]=(uint8_t)((uint32_t)delta>>(j*8));
    }
    unsigned long old;
    if(!VirtualProtect(timing.relay,32,0x20,&old) || !FlushInstructionCache(GetCurrentProcess(),timing.relay,32))return AOT_PROTECTION_FAILED;
    for(unsigned i=0;i<2;i++)timing.calls[2+i]=timing.camera.calls[i];
    for(unsigned i=0;i<4;i++)out[i]=timing.calls[i];
#ifdef AOT_UI_CLOCK
    status=ui_clock_prepare(&timing.ui,base,size,&ui_clock_private_guards,
        (unsigned)GetCurrentThreadId(),ui_permit,0);
    if(status!=AOT_OK)return status;
    for(unsigned i=0;i<3;i++){timing.calls[4+i]=timing.ui.calls[i];out[4+i]=timing.ui.calls[i];}
#endif
#ifdef AOT_NAPE_FIX
    status=nape_hook_prepare(base,size,nape_site,nape_permit,&timing.calls[7]);
    if(status!=AOT_OK)return status;
    out[7]=timing.calls[7];
#endif
    timing.prepared=1;return AOT_OK;
}
void aot_timing_armed(void){timing.armed=timing.prepared;}
