#include "aot_resolution.h"
_Static_assert(sizeof(AotCapability)==8,"native capability stride");
_Static_assert(sizeof(AotResolution)==6,"native resolution stride");
static const AotResolution original_sizes[11]={
    {640,360,480},{800,450,600},{1024,576,768},{1280,720,720},
    {1280,720,768},{1280,720,960},{1280,720,1024},{1600,900,900},
    {1600,900,1200},{1920,1080,1080},{1920,1080,1200}};
int aot_resolution_prepare(uintptr_t base,AotResolutionTables *storage,
    const AotResolution original[11],const AotCapability capability[11],
    const AotTableReference *refs,size_t count,AotPatch *patches,size_t capacity) {
    if(!storage || !original || !capability || !refs || !patches || count>capacity) return 0;
    for(size_t i=0;i<11;i++) {
        if(original[i].width!=original_sizes[i].width || original[i].height!=original_sizes[i].height ||
           original[i].alternate_height!=original_sizes[i].alternate_height) return 0;
    }
    /* Validate every displacement before publishing any output patch. */
    for(size_t i=0;i<count;i++) {
        const AotTableReference *r=&refs[i];int32_t displacement;
        if(r->guard.length>16 || r->displacement_offset+4u>r->guard.length) return 0;
        if(r->table!=AOT_TABLE_SIZE && r->table!=AOT_TABLE_CAPABILITY) return 0;
        if(r->interior>(r->table==AOT_TABLE_SIZE?4u:7u)) return 0;
        uintptr_t origin=base+(r->context_rva==AOT_REFERENCE_MODULE_BASE?0:r->context_rva?r->context_rva:r->guard.rva+r->guard.length);
        uintptr_t target=(uintptr_t)(r->table==AOT_TABLE_SIZE?(void *)storage->sizes:(void *)storage->capabilities)+r->interior;
        if(origin<base || !aot_rel32(origin,target,&displacement)) return 0;
    }
    for(size_t i=0;i<11;i++) { storage->sizes[i]=original[i];storage->capabilities[i]=capability[i]; }
    storage->sizes[11]=(AotResolution){2560,1440,1440};
    storage->sizes[12]=(AotResolution){3840,2160,2160};
    /* Extra modes start unsupported; native display enumeration must populate them. */
    storage->capabilities[11]=(AotCapability){0};storage->capabilities[12]=(AotCapability){0};
    for(size_t i=0;i<count;i++) {
        const AotTableReference *r=&refs[i];int32_t displacement=0;patches[i]=r->guard;
        uintptr_t origin=base+(r->context_rva==AOT_REFERENCE_MODULE_BASE?0:r->context_rva?r->context_rva:r->guard.rva+r->guard.length);
        uintptr_t target=(uintptr_t)(r->table==AOT_TABLE_SIZE?(void *)storage->sizes:(void *)storage->capabilities)+r->interior;
        aot_rel32(origin,target,&displacement);
        for(size_t j=0;j<r->guard.length;j++) patches[i].replacement[j]=r->guard.expected[j];
        uint32_t encoded=(uint32_t)displacement;
        for(unsigned j=0;j<4;j++) patches[i].replacement[r->displacement_offset+j]=(uint8_t)(encoded>>(8*j));
    }
    return 1;
}
int aot_resolution_capability(AotResolutionTables *tables,unsigned index,
    const uint32_t *modes,size_t mode_count,uint32_t desktop_width,uint32_t desktop_height) {
    if(!tables || index>=AOT_RESOLUTION_COUNT || !modes || mode_count>256) return 0;
    AotResolution r=tables->sizes[index];AotCapability *c=&tables->capabilities[index];
    c->width=0;c->height=0;c->supported=0;
    for(size_t i=0;i<mode_count;i++) {
        if(modes[2*i]==r.width && modes[2*i+1]==r.alternate_height &&
           desktop_width>=r.width && desktop_height>=r.alternate_height) {
            c->width=r.width;c->height=r.alternate_height;c->supported=1;return 1;
        }
    }
    return 0;
}
