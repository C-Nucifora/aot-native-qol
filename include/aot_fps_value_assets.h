/* Original code, no game artwork. Matches private-v3-seven-values/manifest.json.
 * Fill each ZEROED0x78 native descriptor via3780a0 after resource readiness,
 * then replace its first0x48 bytes. Keep48..77, especially arrow tail, intact.
 * A row+f0 title ID is unrelated. FPS heading uses reviewed native ASCII fallback.
 */
#ifndef AOT_FPS_VALUE_ASSETS_H
#define AOT_FPS_VALUE_ASSETS_H
#include <stddef.h>
#include <stdint.h>
#define AOT_FPS_VALUE_ASSET_COUNT 7
static const unsigned aot_fps_asset_targets[7]={60,90,120,144,165,240,0};
/* [target][state0 dim/inactive, state1 normal, state2 focused][x,y,w,h] */
static const uint32_t aot_fps_asset_rects[7][3][4]={
 {{0,2048,192,60},{192,2048,192,60},{384,2048,192,60}},
 {{576,2048,192,60},{768,2048,192,60},{960,2048,192,60}},
 {{1152,2048,192,60},{1344,2048,192,60},{1536,2048,192,60}},
 {{1728,2048,192,60},{0,2112,192,60},{192,2112,192,60}},
 {{384,2112,192,60},{576,2112,192,60},{768,2112,192,60}},
 {{960,2112,192,60},{1152,2112,192,60},{1344,2112,192,60}},
 {{1536,2112,192,60},{1728,2112,192,60},{0,2176,192,60}}
};
_Static_assert(sizeof(void *)==8,"Native sprite descriptors require64-bit pointers");
static int aot_fps_patch_value_asset(void *descriptor,size_t bytes,unsigned index,void *texture){
 if(!descriptor || bytes!=0x78 || index>=7 || !texture)return 0;
 unsigned char *d=descriptor;
 for(size_t state=0;state<3;state++){
  for(size_t b=0;b<8;b++)d[state*8+b]=((const unsigned char *)&texture)[b];
  for(size_t b=0;b<16;b++)d[0x18+state*16+b]=((const unsigned char *)aot_fps_asset_rects[index][state])[b];
 }
 return 1;
}
#endif
