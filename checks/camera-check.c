#include <assert.h>
#include <math.h>
#include "camera_rate.h"
static CameraRateSample sample;
static unsigned calls;
static float incoming;
static float original(void *ctx,unsigned axis){assert(ctx==(void *)42&&axis<2);calls++;return axis?1.25f:2.0f;}
static int clock_sample(void *u,const void *ctx,unsigned axis,float dt,CameraRateSample *out){assert(u==(void *)7&&ctx==(void *)42&&axis<2);incoming=dt;*out=sample;return 1;}
static double angle(CameraRateAdapter *a,unsigned hz,unsigned axis,float native_game_delta){
 sample.elapsed_seconds=1.0/hz;double sum=0;
 for(unsigned i=0;i<hz;i++)sum+=camera_rate_sensitivity(a,(void *)42,axis,native_game_delta);
 return sum;
}
int main(void){
 CameraRateAdapter a={original,clock_sample,(void *)7};
 sample=(CameraRateSample){1.0/60,1,1,1,0,CAMERA_INPUT_RATE};
 for(unsigned axis=0;axis<2;axis++){
  double baseline=angle(&a,60,axis,1.f/60);
  const unsigned fps[]={120,240,4096}; /*4096 models measured Unlimited, not a cap. */
  for(unsigned i=0;i<3;i++)assert(fabs(angle(&a,fps[i],axis,1.f/fps[i])-baseline)<0.0001);
  sample.elapsed_seconds=1.0/60;
  assert(camera_rate_sensitivity(&a,(void *)42,axis,1.f/60)==original((void *)42,axis));
  /* Camera angular input was independent of native pause/slowmo at60. */
  assert(fabs(angle(&a,240,axis,0)-baseline)<0.0001&&incoming==0);
  assert(fabs(angle(&a,240,axis,0.5f/240)-baseline)<0.0001);
 }
 sample.elapsed_seconds=1.0/240;sample.unit=CAMERA_INPUT_RELATIVE_DELTA;
 assert(camera_rate_sensitivity(&a,(void *)42,0,1.f/240)==2);
 sample.unit=CAMERA_INPUT_UNKNOWN;assert(camera_rate_sensitivity(&a,(void *)42,0,1.f/240)==2);
 sample.unit=CAMERA_INPUT_RATE;sample.native_mode=2;assert(camera_rate_sensitivity(&a,(void *)42,0,1.f/30)==2);
 sample.native_mode=1;sample.authorized=0;assert(camera_rate_sensitivity(&a,(void *)42,0,1.f/240)==2);
 sample.authorized=1;sample.owner_thread=0;assert(camera_rate_sensitivity(&a,(void *)42,0,1.f/240)==2);
 sample.owner_thread=1;sample.elapsed_seconds=1;assert(fabs(camera_rate_sensitivity(&a,(void *)42,0,1)-6)<0.0001);
 sample.elapsed_seconds=NAN;assert(camera_rate_sensitivity(&a,(void *)42,0,1)==2);
 assert(calls);return 0;
}
