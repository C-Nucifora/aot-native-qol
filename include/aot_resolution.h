#ifndef AOT_RESOLUTION_H
#define AOT_RESOLUTION_H
#include "aot_mod.h"
#define AOT_RESOLUTION_COUNT 13
#define AOT_RESOLUTION_DEFAULT 9
#define AOT_RESOLUTION_PATCH_CAPACITY 192
#define AOT_REFERENCE_MODULE_BASE UINT32_MAX
/* Native eight-byte capability entry; base is not its flag interior. */
typedef struct { uint16_t width,height; uint8_t supported,padding[3]; } AotCapability;
typedef struct { uint16_t width,height,alternate_height; } AotResolution;
typedef struct {
    AotResolution sizes[AOT_RESOLUTION_COUNT];
    AotCapability capabilities[AOT_RESOLUTION_COUNT];
} AotResolutionTables;
typedef enum { AOT_TABLE_SIZE, AOT_TABLE_CAPABILITY } AotTableKind;
typedef struct {
    AotPatch guard;
    AotTableKind table;
    uint8_t displacement_offset;
    uint8_t interior;
    /* 0 = instruction end; UINT32_MAX = module base; otherwise context RVA. */
    uint32_t context_rva;
} AotTableReference;
/* Does not patch or allocate. The caller supplies near-backed, process-lifetime storage. */
int aot_resolution_prepare(uintptr_t module, AotResolutionTables *storage,
    const AotResolution original[11],const AotCapability capability[11],
    const AotTableReference *references,size_t count,AotPatch *patches,size_t capacity);
/* Preserve stock mode matching semantics; mode list is width/height uint32 pairs. */
int aot_resolution_capability(AotResolutionTables *,unsigned index,
    const uint32_t *modes,size_t mode_count,uint32_t desktop_width,uint32_t desktop_height);
#endif
