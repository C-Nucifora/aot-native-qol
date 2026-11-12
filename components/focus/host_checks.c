#define AOT_API
#include "aot_mod.h"
#include "private_guards.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
AotStatus focus_host_arm(void);
void focus_host_reset(void);
int focus_host_show(void *,int);
unsigned int AotFocusStatus(void),AotFocusRollbackCold(void);
static uint8_t *image;
static unsigned protect_calls,cache_calls,allocs,frees,pins,shows;
static unsigned fail_protect,fail_protect2,fail_cache;
static int fail_pin,lock_ok=1,terminate_called,fail_alloc;
static unsigned long thread=1;
static jmp_buf fatal;
static int show(void *w,int c){(void)w;(void)c;shows++;return 7;}
static int visible(void *w){(void)w;return 1;}
void *GetModuleHandleW(const uint16_t *s){return s?(void *)1:image;}
void *GetProcAddress(void *m,const char *name){
    (void)m;return !strcmp(name,"ShowWindow")?(void *)&show:
                   !strcmp(name,"IsWindowVisible")?(void *)&visible:0;
}
int GetModuleHandleExW(unsigned long f,const uint16_t *s,void **p){
    (void)s;assert(f==5);pins++;if(fail_pin)return 0;*p=(void *)1;return 1;
}
unsigned long GetCurrentThreadId(void){return thread;}
int focus_host_outside_lock(void){return lock_ok;}
int VirtualProtect(void *p,size_t n,unsigned long flags,unsigned long *old){
    (void)p;(void)n;(void)flags;protect_calls++;
    if(protect_calls==fail_protect || protect_calls==fail_protect2)return 0;
    *old=0x20;return 1;
}
void *VirtualAlloc(void *p,size_t n,unsigned long a,unsigned long b){
    (void)p;(void)n;(void)a;(void)b;allocs++;return fail_alloc?0:image+0x600000;
}
int VirtualFree(void *p,size_t n,unsigned long x){(void)p;(void)n;(void)x;frees++;return 1;}
void *GetCurrentProcess(void){return (void *)1;}
int FlushInstructionCache(void *p,const void *q,size_t n){(void)p;(void)q;(void)n;return ++cache_calls!=fail_cache;}
int DisableThreadLibraryCalls(void *p){(void)p;return 1;}
void OutputDebugStringA(const char *s){(void)s;}
int TerminateProcess(void *p,unsigned int c){(void)p;assert(c==0xa074f001u);terminate_called=1;longjmp(fatal,1);}
static void start(void){
    focus_host_reset();memset(image,0,0xfb5000);
    protect_calls=cache_calls=allocs=frees=pins=shows=0;
    fail_protect=fail_protect2=fail_cache=0;fail_pin=terminate_called=fail_alloc=0;lock_ok=1;thread=1;
    image[0]='M';image[1]='Z';*(uint32_t *)(image+0x3c)=0x148;
    uint8_t *pe=image+0x148;
    *(uint32_t *)pe=0x4550;*(uint16_t *)(pe+4)=0x8664;*(uint16_t *)(pe+6)=9;
    *(uint32_t *)(pe+8)=0x57ea28d6;*(uint16_t *)(pe+24)=0x20b;
    *(uint32_t *)(pe+40)=0xf91310;*(uint32_t *)(pe+80)=0xfb5000;
    memcpy(image+0x419be2,focus_guard,14);memcpy(image+0x419ae0,wndproc_guard,16);
    *(void **)(image+0x865688)=(void *)&show;
}
static void original(void){assert(!memcmp(image+0x419be2,focus_guard,14));}
int main(void){
    image=aligned_alloc(4096,0xfb5000);assert(image);
    start();assert(focus_host_arm()==AOT_OK);assert(AotFocusStatus()==2 && pins==1);
    assert(image[0x419bea]==0xe8 && image[0x419bef]==0x90);
    int32_t d;memcpy(&d,image+0x419beb,4);
    assert(image+0x419bef+d==image+0x600000);
    *(void **)(image+0xc9d9c8)=(void *)42;
    assert(focus_host_show((void *)42,6)==1 && shows==0);
    assert(focus_host_show((void *)42,9)==7 && shows==1);
    assert(focus_host_show((void *)43,6)==7 && shows==2);
    assert(AotFocusRollbackCold()==AOT_BAD_RANGE);
    *(void **)(image+0xc9d9c8)=0;
    thread=2;assert(AotFocusRollbackCold()==AOT_BAD_RANGE);thread=1;
    assert(AotFocusRollbackCold()==AOT_OK);original();assert(AotFocusStatus()==4 && !frees);
    start();lock_ok=0;assert(focus_host_arm()==AOT_BAD_RANGE);original();assert(!allocs);
    start();image[0x419be2]^=1;assert(focus_host_arm()==AOT_SIGNATURE_MISMATCH && !allocs);
    start();image[0x148+8]^=1;assert(focus_host_arm()==AOT_SIGNATURE_MISMATCH && !allocs);
    start();*(void **)(image+0xc9d9c8)=(void *)42;assert(focus_host_arm()==AOT_SIGNATURE_MISMATCH && !allocs);
    start();fail_pin=1;assert(focus_host_arm()==AOT_PROTECTION_FAILED);original();assert(frees==1);
    start();fail_alloc=1;assert(focus_host_arm()==AOT_BAD_RANGE);original();assert(!pins);
    start();*(void **)(image+0x865688)=0;assert(focus_host_arm()==AOT_SIGNATURE_MISMATCH && !allocs);
    start();fail_protect=1;assert(focus_host_arm()==AOT_PROTECTION_FAILED);original();assert(frees==1 && !pins);
    start();fail_cache=1;assert(focus_host_arm()==AOT_PROTECTION_FAILED);original();assert(frees==1 && !pins);
    start();fail_protect=2;assert(focus_host_arm()==AOT_PROTECTION_FAILED);original();assert(pins==1 && !frees);
    start();fail_cache=2;assert(focus_host_arm()==AOT_CACHE_FAILED);original();assert(pins==1 && !frees);
    start();fail_protect=3;assert(focus_host_arm()==AOT_PROTECTION_FAILED);original();assert(pins==1 && !frees);
    start();fail_protect=3;fail_protect2=4;
    if(!setjmp(fatal)){focus_host_arm();assert(0);}
    assert(terminate_called && AotFocusStatus()==5 && pins==1 && !frees);
    int32_t rel;
    assert(aot_rel32(0x100000000ull,0x17fffffffull,&rel) && rel==INT32_MAX);
    assert(aot_rel32(0x100000000ull,0x80000000ull,&rel) && rel==INT32_MIN);
    assert(!aot_rel32(0x100000000ull,0x180000000ull,&rel));
    assert(!aot_rel32(0x100000000ull,0x7fffffffull,&rel));
    free(image);puts("focus ASI checks passed: guards/cold lock/rel32/pin/publication/cache/protection rollback/uncertain stop");
}
