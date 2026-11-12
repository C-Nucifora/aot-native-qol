#ifndef AOT_CAMERA_RATE_H
#define AOT_CAMERA_RATE_H
#include <stdint.h>
#if defined(_WIN32)
#define CAMERA_ABI __fastcall
#else
#define CAMERA_ABI
#endif
typedef enum { CAMERA_INPUT_UNKNOWN,CAMERA_INPUT_RELATIVE_DELTA,CAMERA_INPUT_RATE } CameraInputUnit;
typedef struct {
    double elapsed_seconds; /* Unscaled measured c7a424, before menu quantization. */
    unsigned authorized,owner_thread,native_mode,pacing_mode;
    CameraInputUnit unit;
} CameraRateSample;
typedef float (CAMERA_ABI *CameraSensitivity)(void *,unsigned);
/* Must classify the actual context's axes, never infer rate units from target.
 * UNKNOWN/relative mouse units must pass through. incoming_seconds is the
 * original XMM15 game-scaled delta, diagnostic only, NEVER the rate clock. */
typedef int (*CameraRateClock)(void *,const void *,unsigned,float,CameraRateSample *);
typedef struct { CameraSensitivity original;CameraRateClock clock;void *clock_context; } CameraRateAdapter;
float camera_rate_sensitivity(CameraRateAdapter *,void *,unsigned,float incoming_seconds);
#endif
