#ifndef AOT_TIMING_H
#define AOT_TIMING_H
#include "aot_mod.h"
#ifdef AOT_UI_CLOCK
#ifdef AOT_NAPE_FIX
#define AOT_TIMING_CALLS 8u
#else
#define AOT_TIMING_CALLS 7u
#endif
#else
#define AOT_TIMING_CALLS 4u
#endif
AotStatus aot_timing_prepare(uint8_t *,size_t,AotPatch out[AOT_TIMING_CALLS]);
void aot_timing_armed(void);
void aot_timing_reset(void);
/* Backend-owned read-only view; no foreign thread, target inference or QPC. */
int aot_fps_session_rate_clock(double *,unsigned allow_menu);
int aot_fps_session_ui_frame(void);
int aot_fps_session_cutin(void *,unsigned,float,float);
int aot_fps_session_pad_index(const void *state,const void *device,unsigned *);
int aot_fps_session_camera_index(const void *context,unsigned *);
#endif
