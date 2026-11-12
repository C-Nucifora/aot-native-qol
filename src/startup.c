#include "aot_startup.h"
#include "aot_owned_files.h"
#include "aot_native_assets.h"
#include "aot_resolution_runtime.h"
AOT_API int __stdcall GetModuleHandleExW(unsigned long,const uint16_t *,void **);
/* Module-private static ownership: callers cannot free state or asset copies.
 * Pin before publishing any callback, including when later rollback fails. */
static struct {
    AotOwnedFile texture,layout;
    AotNativeAssets assets;
    AotResolutionRuntime resolution;
    void *pinned_module;
    unsigned phase;
} startup;
unsigned aot_startup_phase(void){return startup.phase;}
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static AotStatus refuse(AotStatus error,uint8_t *base,size_t size){
    /* Never release even one owner if a partial patch may reference it. */
    if(startup.assets.retain_allocation || startup.resolution.retain_allocation){startup.phase=4;return error;}
    if(startup.resolution.prepared && aot_resolution_remove(&startup.resolution,base,size)!=AOT_OK){startup.phase=4;return AOT_ROLLBACK_FAILED;}
    if(startup.assets.thunk && aot_native_assets_remove(&startup.assets,size)!=AOT_OK){startup.phase=4;return AOT_ROLLBACK_FAILED;}
    if(startup.texture.bytes && aot_file_release(&startup.texture)!=AOT_OK){startup.phase=4;return AOT_PROTECTION_FAILED;}
    if(startup.layout.bytes && aot_file_release(&startup.layout)!=AOT_OK){startup.phase=4;return AOT_PROTECTION_FAILED;}
    startup.phase=3;return error;
}
AotStatus aot_startup_resolution_at_checkpoint(const uint16_t *texture_path,const uint16_t *layout_path){
    if(startup.phase)return AOT_BAD_RANGE;
    startup.phase=1;
    /* Full supported on-disk executable identity precedes fixed-address reads.
     * Runtime instruction guards remain necessary for in-memory mod conflicts. */
    if(!aot_supported_executable()){startup.phase=3;return AOT_SIGNATURE_MISMATCH;}
    uint8_t *base=GetModuleHandleW(0);
    if(!base || base[0]!='M' || base[1]!='Z' || word(base+0x3c)!=0x148 ||
       word(base+0x148)!=0x4550 || word(base+0x150)!=0x57ea28d6 ||
       base[0x14c]!=0x64 || base[0x14d]!=0x86 ||
       word(base+0x148+24+56)!=0xfb5000){startup.phase=3;return AOT_SIGNATURE_MISMATCH;}
    size_t size=0xfb5000;
    /* 9 is the pre-initialization sentinel. French/German require other art.
     * A zeroed global alone must never be used to assert locale readiness. */
    if(base[0xc7a480]>8 || base[0xc7a480]==2 || base[0xc7a480]==4 ||
       *(void **)(base+0xc9cae8)){startup.phase=3;return AOT_BAD_RANGE;}
    AotStatus result=aot_file_load_exact(texture_path,56u+2048u*2048u,&startup.texture);
    if(result!=AOT_OK)return refuse(result,base,size);
    result=aot_file_load_exact(layout_path,32u+65u*16u,&startup.layout);
    if(result!=AOT_OK)return refuse(result,base,size);
    result=aot_native_assets_stage(&startup.assets,base,size,startup.texture.bytes,startup.texture.size,startup.layout.bytes,startup.layout.size);
    if(result!=AOT_OK)return refuse(result,base,size);
    result=aot_resolution_stage(&startup.resolution,base,size);
    if(result!=AOT_OK)return refuse(result,base,size);
    if(!GetModuleHandleExW(1u|4u,(const uint16_t *)(uintptr_t)&aot_startup_resolution_at_checkpoint,&startup.pinned_module))
        return refuse(AOT_PROTECTION_FAILED,base,size);
    /* No observer can execute between these calls under the external checkpoint
     * contract. A failure reverses both modules; failed reversal retains all. */
    result=aot_native_assets_activate(&startup.assets,size);
    if(result!=AOT_OK)return refuse(result,base,size);
    result=aot_resolution_activate(&startup.resolution,base,size);
    if(result!=AOT_OK)return refuse(result,base,size);
    startup.phase=2;return AOT_OK;
}

AotStatus aot_startup_rollback_at_checkpoint(void){
    if(startup.phase!=2)return AOT_BAD_RANGE;
    AotStatus result=refuse(AOT_OK,startup.resolution.module,startup.resolution.image_size);
    return startup.phase==4?AOT_ROLLBACK_FAILED:result;
}
