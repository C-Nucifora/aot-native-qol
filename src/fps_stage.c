#include "aot_fps_stage.h"
#include "aot_owned_files.h"
#include "aot_fps_value_assets.h"
#include "aot_fps_capabilities.h"
_Static_assert(AOT_FPS_VALUE_ASSET_COUNT==AOT_FPS_PRODUCTION_COUNT,"Target/asset count mismatch");
#define SPRITE_BYTES (0x78*AOT_FPS_VALUE_ASSET_COUNT)
#define LAYOUT_RECORDS (255+3*AOT_FPS_VALUE_ASSET_COUNT)
#define LAYOUT_BYTES (32+16*LAYOUT_RECORDS)
#include "fps_stage_guards.h"
#include "source-replay-private.h"
int _fltused;
#ifndef AOT_FPS_NATIVE
#define AOT_FPS_NATIVE(r,t) ((t)(stage.image+(r)))
#endif
AOT_API unsigned long __stdcall GetCurrentThreadId(void);
AOT_API int __stdcall TerminateProcess(void *,unsigned);
AOT_API unsigned long __stdcall GetModuleFileNameW(void *,uint16_t *,unsigned long);
AOT_API int __stdcall GetModuleHandleExW(unsigned long,const uint16_t *,void **);
static struct {
    uint8_t *image,*relay,*sprite;
    void *pin,*texture,*arrow,*renderer;
    AotOwnedFile raw,layout;
    AotPatch call,bootstrap[3];
    unsigned thread,phase,armed,boot_armed,ready,retained,error_count,retiring,reset_pending;
    uint16_t config[1024];
} stage;
static _Noreturn void fatal_uncertain(void){
    stage.phase=5;stage.retained=1;
    OutputDebugStringA("[aot-fps] FATAL: uncertain constructor relay rollback\n");
    TerminateProcess(GetCurrentProcess(),0xa0740001u);
#ifdef AOT_FPS_STAGE_HOST_CHECK
    __builtin_trap();
#else
    __asm__ volatile("int $0x29"::"c"(7):"memory");
#endif
    __builtin_unreachable();
}
static int same_thread(void){return stage.thread==GetCurrentThreadId();}
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int guard(uint32_t rva,const uint8_t *bytes,unsigned n){
    if(rva>0xfb5000u || n>0xfb5000u-rva)return 0;
    for(unsigned i=0;i<n;i++)if(stage.image[rva+i]!=bytes[i])return 0;
    return 1;
}
static int guards(void){
    for(unsigned i=0;i<sizeof(fps_stage_guards)/sizeof(fps_stage_guards[0]);i++){
        uint32_t rva=fps_stage_guards[i].rva;
        uint8_t expected[16];
        for(unsigned j=0;j<16;j++)expected[j]=fps_stage_guards[i].bytes[j];
        if(stage.boot_armed)for(unsigned k=0;k<3;k++)
            if(rva==stage.bootstrap[k].rva)
                for(unsigned j=0;j<5;j++)expected[j]=stage.bootstrap[k].replacement[j];
        if(!guard(rva,expected,16))return 0;
    }
    return 1;
}
static int path(void *self,const char *suffix,uint16_t out[1024]){
    unsigned long n=GetModuleFileNameW(self,out,1024);
    if(!n || n>=1024)return 0;
    while(n && out[n-1]!='\\' && out[n-1]!='/')--n;
    if(!n)return 0;
    for(unsigned i=0;;i++){
        if(n+i>=1024)return 0;
        out[n+i]=(uint8_t)suffix[i];if(!suffix[i])return 1;
    }
}
static int hash_matches(const AotOwnedFile *file,const uint8_t expected[32]){
    uint8_t hash[32];aot_sha256(file->bytes,file->size,hash);
    for(unsigned i=0;i<32;i++)if(hash[i]!=expected[i])return 0;
    return 1;
}
static int assets_valid(void){
    if(!hash_matches(&stage.raw,fps_texture_hash) || !hash_matches(&stage.layout,fps_layout_hash))return 0;
    /* Immutable supported single BC3, base level only,2048x4096; exact header. */
    static const uint32_t g[14]={0x47315447,0x30303630,8388664,32,1,10,0,0,4,0x00cb5b10,0x10211000,12,0,0x01000000};
    static const uint32_t h[8]={0x58595748,32,0,0,16,LAYOUT_RECORDS,16,4};
    for(unsigned i=0;i<14;i++)if(word(stage.raw.bytes+4*i)!=g[i])return 0;
    for(unsigned i=0;i<8;i++)if(word(stage.layout.bytes+4*i)!=h[i])return 0;
    for(unsigned i=0;i<LAYOUT_RECORDS;i++){
        const uint8_t *r=stage.layout.bytes+32+16*i;
        uint32_t x=word(r),y=word(r+4),w=word(r+8),height=word(r+12);
        if(x>2048 || y>4096 || w>2048-x || height>4096-y)return 0;
    }
    for(unsigned i=0;i<AOT_FPS_VALUE_ASSET_COUNT;i++)for(unsigned state=0;state<3;state++){
        const uint8_t *record=stage.layout.bytes+32+16*(255+3*i+state);
        for(unsigned field=0;field<4;field++)
            if(word(record+4*field)!=aot_fps_asset_rects[i][state][field])return 0;
    }
    return 1;
}
static void queue_error(AotFpsMenuError e){
    (void)e;stage.error_count++;
    /* The instance owns the copied event and native retry dialog, not a
     * deferred global borrowed-menu pointer. This is diagnostic only. */
    OutputDebugStringA("[aot-fps] persistence failed; native Retry dialog retains settings screen\n");
}
static int prepare_sprite(void){
    if(stage.ready)return 1;
    if(stage.retained || !same_thread() || !guards())return 0;
    uint8_t *base=stage.image;
    /* Real UIOption/CmnParts native owners and GPU loader must already exist.
     * This is the constructor checkpoint, not D3D Present or early startup. */
    uint8_t *option=*(uint8_t **)(base+0xc9cae8);
    void *arrow=*(void **)(base+0xbf2938);
    if(!option || !*(void **)(option+8) || *(uint32_t *)(option+32)<15 ||
       !*(void **)(option+24) || !arrow || !*(void **)(base+0xbf2928) ||
       *(uint32_t *)(base+0xbf2940)<86 || !*(void **)(base+0xc492c8))return 0;
    uint32_t *refs=(uint32_t *)((uint8_t *)arrow+8);
    if(!*refs || *refs==UINT32_MAX)return 0;
    /* Native3e6b7f..87 decrements this reference count and calls vtable+10
     * at zero. Retain one UI-thread reference for the preserved arrow tail. */
    ++*refs;stage.arrow=arrow;
    stage.texture=AOT_FPS_NATIVE(0x418fb0,void *(*)(const void *,unsigned))(stage.raw.bytes,(unsigned)stage.raw.size);
    if(!stage.texture){--*refs;stage.arrow=0;return 0;}
    /* Native raw-resource loader transfers one texture reference to its caller.
     * Keep both that reference and raw immutable bytes until final teardown. */
    if(!*(uint32_t *)((uint8_t *)stage.texture+8)){stage.retained=1;return 0;}
    for(unsigned i=0;i<SPRITE_BYTES;i++)stage.sprite[i]=0;
    for(unsigned i=0;i<AOT_FPS_VALUE_ASSET_COUNT;i++){
        uint8_t *sprite=stage.sprite+0x78*i;
        if(aot_fps_asset_targets[i]!=aot_fps_targets[i]){stage.retained=1;return 0;}
        AOT_FPS_NATIVE(0x3780a0,void (*)(unsigned,void *))(0,sprite);
        if(*(void **)(sprite+0x48)!=arrow ||
           !aot_fps_patch_value_asset(sprite,0x78,i,stage.texture)){stage.retained=1;return 0;}
    }
    unsigned long old;
    if(!VirtualProtect(stage.sprite,SPRITE_BYTES,0x02,&old)){stage.retained=1;return 0;}
    stage.renderer=*(void **)(base+0xc492c8);stage.ready=1;return 1;
}
static void *constructor(void *menu,unsigned device){
    if(!same_thread() || stage.retained || stage.reset_pending || !stage.ready ||
       stage.renderer!=*(void **)(stage.image+0xc492c8))
        return AOT_FPS_NATIVE(0x37bef0,void *(*)(void *,unsigned))(menu,device);
    return aot_fps_menu_constructor(menu,device);
}
unsigned aot_fps_stage_phase(void){return stage.phase;}
static int resources_loaded(void *owner);
static void children_released(void *parent);
static void release_texture(void **slot);
static void reset_generation(void){
    if(aot_fps_menu_instances())fatal_uncertain();
    if(stage.armed){
        AotStatus result=aot_revert(stage.image,0xfb5000,&stage.call,1);
        if(result!=AOT_OK)fatal_uncertain();
        stage.armed=0;
    }
    /* Only after the final association has disposed its extra widget: native
     * parent teardown, deferred final scalar, or a zero-instance generation
     * change. All native textures stay referenced while any instance remains. */
    release_texture(&stage.texture);release_texture(&stage.arrow);
    if(stage.sprite){unsigned long old;
        if(!VirtualProtect(stage.sprite,SPRITE_BYTES,AOT_PAGE_READWRITE,&old)){
            stage.retained=1;stage.phase=6;return;
        }
    }
    stage.ready=0;stage.renderer=0;stage.reset_pending=0;stage.phase=2;
}
static int resources_loaded(void *owner){
    int result=AOT_FPS_NATIVE(0x3dfde0,int (*)(void *))(owner);
    if(!result || !same_thread() || owner!=stage.image+0xc9cae8 || stage.retained || stage.retiring)return result;
    if(stage.phase==3){
        if(!stage.reset_pending && stage.renderer==*(void **)(stage.image+0xc492c8) &&
           stage.arrow==*(void **)(stage.image+0xbf2938))return result;
        if(aot_fps_menu_instances()){
            stage.reset_pending=1;return result; /* Retain the entire old generation. */
        }
        reset_generation();
    }
    if(stage.phase==2){
        AotStatus status=aot_fps_stage_activate();
        if(status!=AOT_OK)OutputDebugStringA("[aot-fps] menu resource activation refused safely\n");
    }
    return result;
}
static void children_released(void *parent){
    AOT_FPS_NATIVE(0x2e0520,void (*)(void *))(parent);
    if(!same_thread())fatal_uncertain();
    if(!stage.retained && stage.ready){
        if(aot_fps_menu_instances())stage.reset_pending=1;
        else reset_generation();
    }
}
void aot_fps_stage_instance_released(void){
    /* Worker calls after extra-widget disposal and association removal, before
     * original scalar frees native menu memory. No borrowed sprite remains. */
    if(same_thread() && stage.reset_pending && !stage.retained &&
       !aot_fps_menu_instances())reset_generation();
}
AotStatus aot_fps_stage_prepare(void *self){
    if(stage.phase || !self || !aot_supported_executable())return AOT_SIGNATURE_MISMATCH;
    stage.image=GetModuleHandleW(0);stage.thread=GetCurrentThreadId();
    if(!stage.image || !guards())return AOT_SIGNATURE_MISMATCH;
    if(!GetModuleHandleExW(5,(const uint16_t *)(uintptr_t)&aot_fps_stage_prepare,&stage.pin))return AOT_PROTECTION_FAILED;
    stage.phase=1;
    uint16_t filename[1024];AotStatus result=AOT_BAD_RANGE;
    if(!path(self,"aot-native-assets\\CmnParts.g1t",filename))return result;
    result=aot_file_load_exact(filename,8388664,&stage.raw);if(result!=AOT_OK)return result;
    if(!path(self,"aot-native-assets\\CmnPartsLayout.bin",filename))return AOT_BAD_RANGE;
    result=aot_file_load_exact(filename,LAYOUT_BYTES,&stage.layout);if(result!=AOT_OK)return result;
    if(!assets_valid() || !path(self,"aot-native-fps.cfg",stage.config))return AOT_BAD_RANGE;
    stage.sprite=VirtualAlloc(0,SPRITE_BYTES,AOT_MEM_RESERVE_COMMIT,AOT_PAGE_READWRITE);
    if(!stage.sprite)return AOT_BAD_RANGE;
    uintptr_t origin=(uintptr_t)stage.image+0x2e27c8;
    stage.relay=aot_allocate_near((uintptr_t)stage.image+0x2df8c8,origin,64);if(!stage.relay)return AOT_BAD_RANGE;
    stage.relay[0]=0xff;stage.relay[1]=0x25;
    for(unsigned i=2;i<6;i++)stage.relay[i]=0;
    uintptr_t destination=(uintptr_t)&constructor;
    for(unsigned i=0;i<8;i++)stage.relay[6+i]=(uint8_t)(destination>>(8*i));
    void *targets[3]={(void *)&resources_loaded,(void *)&resources_loaded,(void *)&children_released};
    uint32_t sites[3]={0x2df8c3,0x2e0e58,0x2df97d};
    for(unsigned i=0;i<3;i++){
        uint8_t *relay=stage.relay+16+16*i;
        relay[0]=0xff;relay[1]=0x25;for(unsigned j=2;j<6;j++)relay[j]=0;
        uintptr_t address=(uintptr_t)targets[i];
        for(unsigned j=0;j<8;j++)relay[6+j]=(uint8_t)(address>>(j*8));
        stage.bootstrap[i].rva=sites[i];stage.bootstrap[i].length=5;
        int32_t delta;
        if(!aot_rel32((uintptr_t)stage.image+sites[i]+5,(uintptr_t)relay,&delta))return AOT_BAD_RANGE;
        for(unsigned j=0;j<5;j++)stage.bootstrap[i].expected[j]=stage.image[sites[i]+j];
        stage.bootstrap[i].replacement[0]=0xe8;
        for(unsigned j=0;j<4;j++)stage.bootstrap[i].replacement[1+j]=(uint8_t)((uint32_t)delta>>(8*j));
    }
    unsigned long old;
    if(!VirtualProtect(stage.relay,64,0x20,&old) || !FlushInstructionCache(GetCurrentProcess(),stage.relay,64))return AOT_PROTECTION_FAILED;
    stage.call=(AotPatch){0x2e27c3,5,{AOT_FPS_CONSTRUCTOR_BYTES},{0xe8}};
    int32_t delta;if(!aot_rel32(origin,(uintptr_t)stage.relay,&delta))return AOT_BAD_RANGE;
    for(unsigned i=0;i<4;i++)stage.call.replacement[1+i]=(uint8_t)((uint32_t)delta>>(8*i));
    stage.phase=2;return AOT_OK;
}
AotStatus aot_fps_stage_activate(void){
    if(stage.phase!=2 || stage.retiring || !same_thread() || !guards())return AOT_BAD_RANGE;
    /* GPU assets are created/frozen BEFORE bind and constructor publication.
     * An early startup checkpoint without real native resources refuses. */
    if(!prepare_sprite())return AOT_BAD_RANGE;
    AotFpsMenuBinding binding={stage.image,0xfb5000,stage.config,stage.sprite,queue_error,744,192,AOT_FPS_VALUE_ASSET_COUNT};
    if(!aot_fps_menu_bind(&binding))return AOT_SIGNATURE_MISMATCH;
    /* Native per-instance Retry presentation is integrated in fps_menu.c. */
    AotStatus result=aot_apply(stage.image,0xfb5000,&stage.call,1);
    if(result==AOT_OK){stage.armed=1;stage.phase=3;
        OutputDebugStringA("[aot-fps] resource-ready native menu active; guarded session targets60..240/Unlimited\n");}
    else if(result==AOT_ROLLBACK_FAILED)fatal_uncertain();
    return result;
}
AotStatus aot_fps_stage_start(void *self){
    AotStatus result=aot_fps_stage_prepare(self);
    if(result!=AOT_OK)return result;
    result=aot_apply(stage.image,0xfb5000,stage.bootstrap,3);
    if(result==AOT_ROLLBACK_FAILED)fatal_uncertain();
    if(result==AOT_OK){stage.boot_armed=1;
        OutputDebugStringA("[aot-fps] startup armed; awaiting native UIOption load\n");}
    return result;
}
static void release_texture(void **slot){
    uint8_t *texture=*slot;if(!texture)return;
    if(--*(uint32_t *)(texture+8)==0){void **v=*(void ***)texture;((void (*)(void *))v[2])(texture);}
    *slot=0;
}
AotStatus aot_fps_stage_remove(void){
    if(!stage.phase || !same_thread() || stage.retained)return AOT_BAD_RANGE;
    stage.retiring=1;
    if(stage.armed){
        AotStatus result=aot_revert(stage.image,0xfb5000,&stage.call,1);
        if(result==AOT_ROLLBACK_FAILED)fatal_uncertain();
        if(result!=AOT_OK){stage.retained=1;stage.phase=6;return result;}
        stage.armed=0;
    }
    if(aot_fps_menu_instances())return AOT_BAD_RANGE;
    if(stage.boot_armed){
        AotStatus result=aot_revert(stage.image,0xfb5000,stage.bootstrap,3);
        if(result==AOT_ROLLBACK_FAILED)fatal_uncertain();
        if(result!=AOT_OK){stage.retained=1;stage.phase=6;return result;}
        stage.boot_armed=0;
    }
    release_texture(&stage.texture);release_texture(&stage.arrow);
    if(stage.sprite && !VirtualFree(stage.sprite,0,AOT_MEM_RELEASE))return AOT_PROTECTION_FAILED;
    stage.sprite=0;
    if(stage.relay && !VirtualFree(stage.relay,0,AOT_MEM_RELEASE))return AOT_PROTECTION_FAILED;
    stage.relay=0;
    AotStatus result=AOT_OK;
    if(stage.raw.bytes)result=aot_file_release(&stage.raw);
    if(result==AOT_OK && stage.layout.bytes)result=aot_file_release(&stage.layout);
    if(result==AOT_OK)stage.phase=4;return result;
}
