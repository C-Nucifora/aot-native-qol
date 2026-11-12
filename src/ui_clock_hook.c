#include "ui_clock_hook.h"
#include "aot_owned_files.h"
#include "source-replay-private.h"
AOT_API int __stdcall GetModuleHandleExW(unsigned long,const uint16_t *,void **);
static UiClockHook *active;
static int UI_CLOCK_ABI entry(void *root,float seconds){
    return ui_clock_update(&active->adapter,root,seconds);
}
static int equal(const uint8_t *a,const uint8_t *b,unsigned n){
    for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;
}
AotStatus ui_clock_prepare(UiClockHook *out,uint8_t *image,size_t size,
    const UiClockPrivateGuards *g,unsigned thread,UiClockPermit permit,void *context){
    if(!out || !image || !g || !thread || !permit || active || out->retained ||
       size!=0xfb5000 || image!=GetModuleHandleW(0) || !aot_supported_executable())return AOT_BAD_RANGE;
    const uint32_t starts[3]={0x2dddb6,0x2dddce,0x2dddde};
    const uint32_t sites[3]={0x2dddc8,0x2dddd8,0x2ddde8};
    for(unsigned i=0;i<3;i++)if(!equal(image+starts[i],g->roots[i],32))return AOT_SIGNATURE_MISMATCH;
    if(!equal(image+0x2dddbd,g->divide,16) || !equal(image+0xde0d6,g->clock,32))return AOT_SIGNATURE_MISMATCH;
    const uint8_t first[6]={AOT_UI_FIRST_BYTES};
    const uint8_t later[6]={AOT_UI_LATER_BYTES};
    for(unsigned i=0;i<3;i++)if(!equal(image+sites[i],i?later:first,6))return AOT_SIGNATURE_MISMATCH;
    if(!GetModuleHandleExW(5,(const uint16_t *)(uintptr_t)&entry,&out->pin))return AOT_PROTECTION_FAILED;
    out->retained=1;out->image=image;
    out->relay=aot_allocate_near((uintptr_t)image+sites[0]+5,(uintptr_t)image+sites[2]+5,32);
    if(!out->relay)return AOT_BAD_RANGE;
    /* First bundle displaced MOVAPS XMM1,XMM6; repeat it for every UI root.
     * XMM6 stays unchanged and therefore still feeds the other native callers. */
    const uint8_t jump[9]={AOT_UI_REPLAY_BYTES,0xff,0x25,0,0,0,0};
    for(unsigned j=0;j<9;j++)out->relay[j]=jump[j];
    uintptr_t target=(uintptr_t)&entry;
    for(unsigned j=0;j<8;j++)out->relay[9+j]=(uint8_t)(target>>(8*j));
    for(unsigned i=0;i<3;i++){
        AotPatch *p=&out->calls[i];p->rva=sites[i];p->length=6;
        int32_t delta;
        if(!aot_rel32((uintptr_t)image+sites[i]+5,(uintptr_t)out->relay,&delta))return AOT_BAD_RANGE;
        for(unsigned j=0;j<6;j++)p->expected[j]=image[sites[i]+j];
        p->replacement[0]=0xe8;
        for(unsigned j=0;j<4;j++)p->replacement[1+j]=(uint8_t)((uint32_t)delta>>(8*j));
        p->replacement[5]=0x90;
    }
    unsigned long old;
    if(!VirtualProtect(out->relay,32,0x20,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),out->relay,17))return AOT_PROTECTION_FAILED;
    out->provider=(UiClockProvider){image,thread,permit,context};
    out->adapter=(UiClockAdapter){ui_clock_read_native,&out->provider};
    out->prepared=1;active=out;return AOT_OK;
}
