#include "aot_native_assets.h"
#include "asset_guards.h"
typedef void *(*ReadResource)(unsigned,void *,unsigned *);
typedef void *(*Allocate)(void *,size_t,unsigned,const void *);
typedef void (*Release)(void *,void *);
/* Single UI resource pair, process-lifetime state. Activation requires quiescence. */
static AotNativeAssets *active_assets;
static uint32_t word(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void *read_resource(unsigned id,void *allocator,unsigned *size) {
    AotNativeAssets *s=active_assets;
    if(!s)return 0;
    ReadResource original=(ReadResource)(s->module+0x40ffd0);
    unsigned original_size=0;
    void *stock=original(id,allocator,&original_size);
    if(id!=1702 && id!=1703){if(size && stock)*size=original_size;return stock;}
    if(!stock || !allocator)return stock;
    void **vtable=*(void ***)allocator;
    const uint8_t *bytes=id==1702?s->texture:s->layout;
    size_t count=id==1702?s->texture_size:s->layout_size;
    /* Match native aligned allocation and free ABI at410023..410058. */
    uintptr_t debug[2]={0x2c,0};
    void *replacement=((Allocate)vtable[0x38/8])(allocator,(count+2047u)&~(size_t)2047u,16,debug);
    if(replacement)for(size_t i=0;i<count;i++)((uint8_t *)replacement)[i]=bytes[i];
    ((Release)vtable[0x58/8])(allocator,stock);
    if(size)*size=replacement?(unsigned)count:0;
    return replacement;
}
AotStatus aot_native_assets_stage(AotNativeAssets *s,uint8_t *base,size_t image_size,
    const uint8_t *g,size_t ng,const uint8_t *h,size_t nh) {
    if(!s || s->thunk || !base || !g || !h || ng!=56u+2048u*2048u || nh!=32+65*16)
        return AOT_BAD_RANGE;
    /* Exact supported format variant: single BC3 texture, base level only,
     * fixed payload offset56; every header field is checked before parsing. */
    static const uint32_t texture_header[14]={0x47315447,0x30303630,
        56u+2048u*2048u,32,1,10,0,0,4,0x00bb5b10,0x10211000,12,0,0x01000000};
    static const uint32_t layout_header[8]={0x58595748,32,0,0,16,65,16,4};
    for(unsigned i=0;i<14;i++)if(word(g+4*i)!=texture_header[i])return AOT_BAD_RANGE;
    for(unsigned i=0;i<8;i++)if(word(h+4*i)!=layout_header[i])return AOT_BAD_RANGE;
    /* Those two locale branches use different original atlases/layouts. */
    if(image_size<=0xc7a480 || base[0xc7a480]==2 || base[0xc7a480]==4)return AOT_BAD_RANGE;
    for(unsigned i=0;i<65;i++){
        const uint8_t *r=h+32+16*i;uint32_t x=word(r),y=word(r+4),w=word(r+8),height=word(r+12);
        if(x>2048 || y>2048 || w>2048-x || height>2048-y)return AOT_BAD_RANGE;
        if(i>=61 && (w!=348 || height!=60))return AOT_BAD_RANGE;
    }
    for(unsigned i=0;i<2;i++){
        const AotPatch *p=&asset_call_guards[i];
        if(p->rva>image_size || p->length>image_size-p->rva)return AOT_BAD_RANGE;
        for(unsigned j=0;j<p->length;j++)if(base[p->rva+j]!=p->expected[j])return AOT_SIGNATURE_MISMATCH;
    }
    for(size_t i=0;i<sizeof(asset_preserved_guards)/sizeof(asset_preserved_guards[0]);i++){
        const AotPatch *p=&asset_preserved_guards[i];
        if(p->rva>image_size || p->length>image_size-p->rva)return AOT_BAD_RANGE;
        for(unsigned j=0;j<p->length;j++)if(base[p->rva+j]!=p->expected[j])return AOT_SIGNATURE_MISMATCH;
    }
    uintptr_t low=(uintptr_t)base+asset_call_guards[0].rva+5;
    uintptr_t high=(uintptr_t)base+asset_call_guards[1].rva+5;
    uint8_t *thunk=aot_allocate_near(low,high,14);
    if(!thunk)return AOT_BAD_RANGE;
    thunk[0]=0xff;thunk[1]=0x25;
    for(unsigned i=2;i<6;i++)thunk[i]=0;
    uintptr_t address=(uintptr_t)&read_resource;
    for(unsigned i=0;i<8;i++)thunk[6+i]=(uint8_t)(address>>(i*8));
    unsigned long old;
    if(!VirtualProtect(thunk,14,0x20,&old) || !FlushInstructionCache(GetCurrentProcess(),thunk,14)){
        VirtualFree(thunk,0,AOT_MEM_RELEASE);return AOT_PROTECTION_FAILED;
    }
    for(unsigned i=0;i<2;i++){
        s->calls[i]=asset_call_guards[i];int32_t delta;
        if(!aot_rel32((uintptr_t)base+s->calls[i].rva+5,(uintptr_t)thunk,&delta)){
            VirtualFree(thunk,0,AOT_MEM_RELEASE);return AOT_BAD_RANGE;
        }
        s->calls[i].replacement[0]=0xe8;
        for(unsigned j=0;j<4;j++)s->calls[i].replacement[j+1]=(uint8_t)((uint32_t)delta>>(j*8));
    }
    s->texture=g;s->texture_size=ng;s->layout=h;s->layout_size=nh;s->module=base;s->image_size=image_size;s->thunk=thunk;
    return AOT_OK;
}
AotStatus aot_native_assets_activate(AotNativeAssets *s,size_t size) {
    if(!s || !s->thunk || size!=s->image_size || s->active || s->retain_allocation || active_assets)return AOT_BAD_RANGE;
    /* Stage and activation may be separated; verify the native ABI and locale
     * again in the caller's quiescent interval before publishing the callback. */
    if(size<=0xc7a480 || s->module[0xc7a480]==2 || s->module[0xc7a480]==4)return AOT_BAD_RANGE;
    for(size_t i=0;i<sizeof(asset_preserved_guards)/sizeof(asset_preserved_guards[0]);i++){
        const AotPatch *p=&asset_preserved_guards[i];
        if(p->rva>size || p->length>size-p->rva)return AOT_BAD_RANGE;
        for(unsigned j=0;j<p->length;j++)if(s->module[p->rva+j]!=p->expected[j])return AOT_SIGNATURE_MISMATCH;
    }
    active_assets=s;
    AotStatus result=aot_apply(s->module,size,s->calls,2);
    if(result==AOT_OK)s->active=1;
    else if(result==AOT_ROLLBACK_FAILED)s->retain_allocation=1;
    else active_assets=0;
    return result;
}
AotStatus aot_native_assets_remove(AotNativeAssets *s,size_t size) {
    if(!s || !s->thunk || size!=s->image_size || s->retain_allocation)return AOT_BAD_RANGE;
    if(s->active){
        AotStatus result=aot_revert(s->module,size,s->calls,2);
        if(result!=AOT_OK){if(result==AOT_ROLLBACK_FAILED)s->retain_allocation=1;return result;}
        s->active=0;active_assets=0;
    }
    if(!VirtualFree(s->thunk,0,AOT_MEM_RELEASE))return AOT_PROTECTION_FAILED;
    s->thunk=0;return AOT_OK;
}
