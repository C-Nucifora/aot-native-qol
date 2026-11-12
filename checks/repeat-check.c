#include "repeat_adapter.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int enabled = 1;
static double dt;
static uint32_t input;
static float keyboard_step;
static uint32_t u32(const unsigned char *p) { uint32_t v; memcpy(&v,p,4); return v; }
static void put(unsigned char *p, uint32_t v) { memcpy(p,&v,4); }
static int sample(void *ctx, double *out) { (void)ctx; *out=dt; return enabled; }
static void keyboard(void *p, float f) { (void)p; keyboard_step=f; }
static int controller(void *p, void *device, unsigned advance) {
    static const uint32_t m[16]={0x400,0x800,0x200000,0x800000,0x1000000,0x400000,2,4,1,8,0x40,0x80,0x10,0x20,0x100,0x200};
    unsigned char *s=p; unsigned c,i; uint32_t old=u32(s+4);
    (void)device;
    put(s+4,input); put(s+8,input & ~old); put(s+12,input & ~old);
    for(i=0;i<16;++i) if (input & m[i]) {
        c=(unsigned char)(s[0x34+i]+advance); s[0x34+i]=(unsigned char)c;
        if (c>=20) { put(s+12,u32(s+12)|m[i]); s[0x34+i]=16; }
        else if(c==16) put(s+12,u32(s+12)|m[i]);
    } else s[0x34+i]=0;
    return 7;
}
static RepeatAdapter fresh(void) {
    RepeatAdapter a={0}; a.keyboard=keyboard; a.controller=controller; a.clock=sample;
    enabled=1; input=0; return a;
}
static void fixed(unsigned fps) {
    RepeatAdapter a=fresh(); unsigned char state[0xa0]={0};
    unsigned f,pulses=0; double first=-1,last=-1;
    dt=1./fps; input=0x400;
    for(f=0;f<fps*2;++f) {
        assert(repeat_controller(&a,state,0,1)==7);
        if(u32(state+12)&0x400) {
            double t=(f+1)*dt;
            if(pulses==1) first=t;
            if(pulses>1) assert(fabs((t-last)-4./60)<dt+1e-8);
            last=t; ++pulses;
        }
    }
    assert(fabs(first-16./60)<dt+1e-8);
    assert(pulses==28);
    input=0; repeat_controller(&a,state,0,1); assert(state[0x34]==0);
    assert(!(u32(state+12)&0x400));
    input=0x400; repeat_controller(&a,state,0,1); assert(u32(state+12)&0x400);
    repeat_keyboard(&a,0,1); assert(fabs(keyboard_step-60./fps)<1e-6);
}
int main(void) {
    unsigned rates[]={60,90,120,144,165,240,1000},i;
    for(i=0;i<sizeof rates/sizeof *rates;++i) fixed(rates[i]);
    { RepeatAdapter a=fresh(); unsigned char s[0xa0]={0}, native[0xa0]={0};
      dt=1./60;
      for(i=0;i<1000;++i) {
          input=(i%37<30)?0x3c00fff:0;
          repeat_controller(&a,s,0,1); controller(native,0,1);
          assert(memcmp(s,native,sizeof s)==0);
      }
    }
    { RepeatAdapter a=fresh(); unsigned char s[0xa0]={0};
      dt=1./240; input=0x400;
      repeat_controller(&a,s,0,1); assert(s[0x34]==0);
      enabled=0; repeat_controller(&a,s,0,2); assert(s[0x34]==2);
      repeat_keyboard(&a,0,2); assert(keyboard_step==2);
      enabled=1; dt=NAN; repeat_keyboard(&a,0,1); assert(keyboard_step==1);
      dt=.06; repeat_keyboard(&a,0,1); assert(keyboard_step==1);
      dt=0; repeat_keyboard(&a,0,1); assert(keyboard_step==1);
    }
    puts("repeat checks passed: 60/90/120/144/165/240/1000 Hz; edges, release, repeat spacing, 30 Hz passthrough, invalid clocks");
}
