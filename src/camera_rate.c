#include "camera_rate.h"
float camera_rate_sensitivity(CameraRateAdapter *a,void *context,unsigned axis,float incoming_seconds){
    float sensitivity=a->original(context,axis);
    CameraRateSample sample={0};
    if(!a->clock || !a->clock(a->clock_context,context,axis,incoming_seconds,&sample) ||
       !sample.authorized || !sample.owner_thread || sample.native_mode!=1 ||
       sample.pacing_mode!=0 || sample.unit!=CAMERA_INPUT_RATE ||
       !(sample.elapsed_seconds>=0))return sensitivity;
    /* The angular step ignored native game time scale at60. Use unscaled
     * measured time, leaving XMM15 and its smoothing/pause semantics intact. */
    double seconds=sample.elapsed_seconds;
    if(seconds>0.05)seconds=0.05;
    return sensitivity*(float)(seconds*60.0);
}
