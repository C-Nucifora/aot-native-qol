/* Original staging code only. Fill a ZEROED120-byte template through native
 * helper3780a0 on the UI thread after resource readiness, then call this.
 * Caller owns texture lifetime; no AddRef/release ABI is inferred here. */
#ifndef AOT_SPRITE60_PATCH_H
#define AOT_SPRITE60_PATCH_H
#include <stdint.h>
#include <stddef.h>
_Static_assert(sizeof(void *)==8,"Native descriptor requires64-bit pointers");
static int aot_stage_sprite60(void *descriptor,size_t bytes,void *native_texture){
    if(!descriptor || bytes!=0x78 || !native_texture)return 0;
    unsigned char *d=descriptor;
    const uint32_t rectangles[3][4]={
        {1408,2048,192,60}, /* state0, stock analogue14: inactive/dim */
        {1024,2048,192,60}, /* state1, stock analogue12: normal */
        {1216,2048,192,60}  /* state2, stock analogue13: focused */
    };
    for(size_t i=0;i<3;i++){
        const unsigned char *pointer=(const unsigned char *)&native_texture;
        const unsigned char *rectangle=(const unsigned char *)rectangles[i];
        for(size_t j=0;j<8;j++)d[i*8+j]=pointer[j];
        for(size_t j=0;j<16;j++)d[0x18+i*16+j]=rectangle[j];
    }
    /* Preserve native arrow texture/geometry/source and remaining tail48..77. */
    return 1;
}
#endif
