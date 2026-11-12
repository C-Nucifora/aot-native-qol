#include "aot_resolution_runtime.h"
#include "resolution_manifest.h"
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static int matches(const uint8_t *base,size_t size,const AotPatch *p) {
    if(p->rva>size || p->length>size-p->rva)return 0;
    for(size_t i=0;i<p->length;i++)if(base[p->rva+i]!=p->expected[i])return 0;
    return 1;
}
AotStatus aot_resolution_stage(AotResolutionRuntime *s,uint8_t *base,size_t size) {
    if(!s || !base || s->prepared || s->tables || s->active ||
       size<0xc9e2cc || size<0x8959b0+66)return AOT_BAD_RANGE;
    for(size_t i=0;i<COUNT(native_table_references);i++)
        if(!matches(base,size,&native_table_references[i].guard))return AOT_SIGNATURE_MISMATCH;
    for(size_t i=0;i<COUNT(native_fixed_patches);i++)
        if(!matches(base,size,&native_fixed_patches[i]))return AOT_SIGNATURE_MISMATCH;
    for(size_t i=0;i<COUNT(native_preserved_guards);i++)
        if(!matches(base,size,&native_preserved_guards[i]))return AOT_SIGNATURE_MISMATCH;
    uintptr_t low=UINTPTR_MAX,high=0;
    for(size_t i=0;i<COUNT(native_table_references);i++){
        const AotTableReference *r=&native_table_references[i];
        uintptr_t origin=(uintptr_t)base+(r->context_rva==AOT_REFERENCE_MODULE_BASE?0:
            r->context_rva?r->context_rva:r->guard.rva+r->guard.length);
        if(origin<(uintptr_t)base)return AOT_BAD_RANGE;
        if(origin<low)low=origin;
        if(origin>high)high=origin;
    }
    AotResolutionTables *tables=aot_allocate_near(low,high,sizeof(*tables));
    if(!tables)return AOT_BAD_RANGE;
    if(!aot_resolution_prepare((uintptr_t)base,tables,
        (const AotResolution *)(base+0x8959b0),(const AotCapability *)(base+0xc9e274),
        native_table_references,COUNT(native_table_references),s->patches,AOT_RESOLUTION_PATCH_CAPACITY)){
        VirtualFree(tables,0,AOT_MEM_RELEASE);return AOT_SIGNATURE_MISMATCH;
    }
    size_t count=COUNT(native_table_references);
    _Static_assert(COUNT(native_table_references)+COUNT(native_fixed_patches)<=AOT_RESOLUTION_PATCH_CAPACITY,"manifest capacity");
    for(size_t i=0;i<COUNT(native_fixed_patches);i++)s->patches[count++]=native_fixed_patches[i];
    s->tables=tables;s->module=base;s->image_size=size;s->count=count;s->prepared=1;return AOT_OK;
}
AotStatus aot_resolution_activate(AotResolutionRuntime *s,uint8_t *base,size_t size) {
    if(!s || !s->prepared || s->module!=base || s->image_size!=size || s->active || s->retain_allocation)return AOT_BAD_RANGE;
    for(size_t i=0;i<COUNT(native_preserved_guards);i++)
        if(!matches(base,size,&native_preserved_guards[i]))return AOT_SIGNATURE_MISMATCH;
    const uint8_t *original=(const uint8_t *)s->tables->sizes;
    for(size_t i=0;i<66;i++)if(base[0x8959b0+i]!=original[i])return AOT_SIGNATURE_MISMATCH;
    AotStatus result=aot_apply(base,size,s->patches,s->count);
    if(result==AOT_OK)s->active=1;
    if(result==AOT_ROLLBACK_FAILED)s->retain_allocation=1;
    return result;
}
AotStatus aot_resolution_remove(AotResolutionRuntime *s,uint8_t *base,size_t size) {
    if(!s || !s->prepared || s->module!=base || s->image_size!=size || s->retain_allocation)return AOT_BAD_RANGE;
    if(s->active){
        AotStatus result=aot_revert(base,size,s->patches,s->count);
        if(result!=AOT_OK){if(result==AOT_ROLLBACK_FAILED)s->retain_allocation=1;return result;}
        s->active=0;
    }
    if(!VirtualFree(s->tables,0,AOT_MEM_RELEASE))return AOT_PROTECTION_FAILED;
    s->tables=0;s->module=0;s->image_size=0;s->count=0;s->prepared=0;return AOT_OK;
}
