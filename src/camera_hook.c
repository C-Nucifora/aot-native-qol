#include "camera_hook.h"
#include "aot_owned_files.h"
AOT_API int __stdcall GetModuleHandleExW(unsigned long,const uint16_t *,void **);
static CameraHookStage *active;
static float CAMERA_ABI camera_entry(void *context,unsigned axis,float incoming_seconds){
    return camera_rate_sensitivity(&active->adapter,context,axis,incoming_seconds);
}
static int equal(const uint8_t *a,const uint8_t *b,unsigned count){
    for(unsigned i=0;i<count;i++)if(a[i]!=b[i])return 0;
    return 1;
}
AotStatus camera_hook_prepare(CameraHookStage *out,uint8_t *image,size_t size,
                             const CameraPrivateGuards *g,CameraRateClock clock,void *clock_context){
    if(!out || !image || !g || !clock || active || size!=0xfb5000 ||
       image!=GetModuleHandleW(0) || !aot_supported_executable())return AOT_BAD_RANGE;
    if(!equal(image+0xf453a,g->yaw,32) || !equal(image+0xf463b,g->pitch,32) ||
       !equal(image+0x1101a0,g->sensitivity,16))return AOT_SIGNATURE_MISMATCH;
    const uint32_t sites[2]={0xf4541,0xf4642};
    for(unsigned i=0;i<2;i++){
        uint8_t *call=image+sites[i];uint32_t u=(uint32_t)call[1]|(uint32_t)call[2]<<8|
            (uint32_t)call[3]<<16|(uint32_t)call[4]<<24;
        if(call[0]!=0xe8 || (uintptr_t)(call+5)+(intptr_t)(int32_t)u!=(uintptr_t)image+0x1101a0)
            return AOT_SIGNATURE_MISMATCH;
    }
    if(!GetModuleHandleExW(5,(const uint16_t *)(uintptr_t)&camera_entry,&out->pin))return AOT_PROTECTION_FAILED;
    out->retained=1;out->image=image;
    out->relay=aot_allocate_near((uintptr_t)image+sites[0]+5,(uintptr_t)image+sites[1]+5,32);
    if(!out->relay)return AOT_BAD_RANGE;
    /* Win64: original RCX=context,EDX=axis. Preserve both and copy original
     * callee-saved XMM15 game delta into third argument XMM2, then tail-jump.
     * Never overwrite XMM15 or pass scaled delta into native smoothing. */
    const uint8_t prefix[10]={0x41,0x0f,0x28,0xd7,0xff,0x25,0,0,0,0};
    for(unsigned i=0;i<10;i++)out->relay[i]=prefix[i];
    uintptr_t entry=(uintptr_t)&camera_entry;
    for(unsigned i=0;i<8;i++)out->relay[10+i]=(uint8_t)(entry>>(8*i));
    for(unsigned i=0;i<2;i++){
        AotPatch *p=&out->calls[i];p->rva=sites[i];p->length=5;
        int32_t delta;
        if(!aot_rel32((uintptr_t)image+sites[i]+5,(uintptr_t)out->relay,&delta))return AOT_BAD_RANGE;
        for(unsigned j=0;j<5;j++)p->expected[j]=image[sites[i]+j];
        p->replacement[0]=0xe8;
        for(unsigned j=0;j<4;j++)p->replacement[1+j]=(uint8_t)((uint32_t)delta>>(8*j));
    }
    unsigned long old;
    if(!VirtualProtect(out->relay,32,0x20,&old) || !FlushInstructionCache(GetCurrentProcess(),out->relay,18))return AOT_PROTECTION_FAILED;
    out->adapter=(CameraRateAdapter){(CameraSensitivity)(image+0x1101a0),clock,clock_context};
    out->prepared=1;active=out;return AOT_OK;
}
