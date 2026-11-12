#ifndef AOT_FPS_CAPABILITIES_H
#define AOT_FPS_CAPABILITIES_H
/*0 means uncapped, never a substitute high numeric timer target.
 * Shared native menu, config and guarded experimental session targets.
 * A target is a pacing request, not a measured presentation rate. */
#define AOT_FPS_PRODUCTION_COUNT 7u
#define AOT_FPS_DEFAULT 60u
#define AOT_FPS_REQUESTED_MAX 240u
static const unsigned aot_fps_targets[AOT_FPS_PRODUCTION_COUNT]={60,90,120,144,165,240,0};
static inline unsigned aot_fps_index(unsigned value){
    for(unsigned i=0;i<AOT_FPS_PRODUCTION_COUNT;i++)if(aot_fps_targets[i]==value)return i;
    return 0;
}
static inline int aot_fps_supported(unsigned value){return aot_fps_targets[aot_fps_index(value)]==value;}
static inline unsigned aot_fps_normalize(unsigned value){return aot_fps_targets[aot_fps_index(value)];}
#endif
