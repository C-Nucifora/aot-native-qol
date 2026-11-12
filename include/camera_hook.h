#ifndef AOT_CAMERA_HOOK_H
#define AOT_CAMERA_HOOK_H
#include "camera_rate.h"
#include "aot_mod.h"
typedef struct { uint8_t yaw[32],pitch[32],sensitivity[16]; } CameraPrivateGuards;
typedef struct {
    CameraRateAdapter adapter;
    uint8_t *image,*relay;
    void *pin;
    AotPatch calls[2];
    unsigned prepared,retained;
} CameraHookStage;
/* One process-lifetime STATIC owner; startup quiescence required. No patch is
 * installed here. Caller atomically applies both returned calls with aot_apply,
 * retains everything on rollback failure and never unloads this staged module.
 * Clock/provider context must also have immutable process lifetime. */
AotStatus camera_hook_prepare(CameraHookStage *,uint8_t *,size_t,const CameraPrivateGuards *,CameraRateClock,void *);
#endif
