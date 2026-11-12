#ifndef AOT_RESOLUTION_RUNTIME_H
#define AOT_RESOLUTION_RUNTIME_H
#include "aot_resolution.h"
typedef struct {
    AotResolutionTables *tables;
    uint8_t *module;
    size_t image_size;
    AotPatch patches[AOT_RESOLUTION_PATCH_CAPACITY];
    size_t count;
    unsigned prepared,active,retain_allocation;
} AotResolutionRuntime;
/* State must be zero-initialized, process-lifetime storage. All calls require
 * quiescent startup before display initialization, or a coordinated teardown.
 * No thread suspension, startup hooks or menu asset substitution happens here. */
AotStatus aot_resolution_stage(AotResolutionRuntime *,uint8_t *module,size_t image_size);
/* Caller must have installed and verified the native 65-record label layout
 * and matching texture first. Not called/exported by the audit ASI. */
AotStatus aot_resolution_activate(AotResolutionRuntime *,uint8_t *module,size_t image_size);
AotStatus aot_resolution_remove(AotResolutionRuntime *,uint8_t *module,size_t image_size);
#endif
