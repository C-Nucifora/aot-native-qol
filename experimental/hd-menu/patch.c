#include "aot_mod.h"
static int equal(const uint8_t *a, const uint8_t *b, size_t n) {
    for (size_t i=0;i<n;i++) if (a[i]!=b[i]) return 0;
    return 1;
}
int aot_rel32(uintptr_t next, uintptr_t target, int32_t *out) {
    if (target>=next) {
        uintptr_t d=target-next;
        if (d>INT32_MAX) return 0;
        *out=(int32_t)d;
    } else {
        uintptr_t d=next-target;
        if (d>(uintptr_t)INT32_MAX+1u) return 0;
        *out=d==(uintptr_t)INT32_MAX+1u ? INT32_MIN : -(int32_t)d;
    }
    return 1;
}
void *aot_allocate_near(uintptr_t low, uintptr_t high, size_t bytes) {
    if (!bytes || low>high || high>UINTPTR_MAX-INT32_MAX) return 0;
    const uintptr_t step=0x10000u;
    uintptr_t start=high>(uintptr_t)INT32_MAX ? high-INT32_MAX : step;
    start=(start+step-1u)&~(step-1u);
    uintptr_t end=low+INT32_MAX;
    if (end<low || bytes-1u>end || start>end-(bytes-1u)) return 0;
    end-=bytes-1u;
    for (uintptr_t p=start;p<=end;) {
        void *v=VirtualAlloc((void *)p,bytes,AOT_MEM_RESERVE_COMMIT,AOT_PAGE_READWRITE);
        if (v) {
            int32_t d;
            uintptr_t first=(uintptr_t)v,last=first+bytes-1u;
            if (last>=first && aot_rel32(low,last,&d) && aot_rel32(high,first,&d)) return v;
            VirtualFree(v,0,AOT_MEM_RELEASE);
        }
        if (end-p<step) break;
        p+=step;
    }
    return 0;
}
static AotStatus write_bytes(uint8_t *p,const uint8_t *data,size_t n, unsigned long *original_protection) {
    unsigned long old;
    *original_protection=0;
    if (!VirtualProtect(p,n,AOT_PAGE_EXECUTE_READWRITE,&old)) return AOT_PROTECTION_FAILED;
    *original_protection=old;
    for (size_t i=0;i<n;i++) p[i]=data[i];
    int cache=FlushInstructionCache(GetCurrentProcess(),p,n);
    unsigned long ignored;
    if (!VirtualProtect(p,n,old,&ignored)) return AOT_PROTECTION_FAILED;
    return cache ? AOT_OK : AOT_CACHE_FAILED;
}
static AotStatus transaction(uint8_t *base,size_t size,const AotPatch *p,size_t count,int reverse) {
    if (count>192) return AOT_BAD_RANGE;
    unsigned long protections[192];
    for (size_t i=0;i<count;i++) {
        size_t n=p[i].length;
        if (!n || n>AOT_MAX_PATCH_BYTES || p[i].rva>size || n>size-p[i].rva) return AOT_BAD_RANGE;
        if (((uintptr_t)(base+p[i].rva)&4095u)+n>4096u) return AOT_BAD_RANGE;
        if (!equal(base+p[i].rva,reverse?p[i].replacement:p[i].expected,n)) return AOT_SIGNATURE_MISMATCH;
        for (size_t j=0;j<i;j++)
            if ((uint64_t)p[i].rva+p[i].length>p[j].rva &&
                (uint64_t)p[j].rva+p[j].length>p[i].rva) return AOT_OVERLAPPING_PATCH;
    }
    for (size_t i=0;i<count;i++) {
        AotStatus result=write_bytes(base+p[i].rva,reverse?p[i].expected:p[i].replacement,p[i].length,&protections[i]);
        if (result!=AOT_OK) {
            /* Current write can have succeeded before protection/cache failure. */
            for (size_t k=i+1;k>0;k--) {
                size_t j=k-1;
                if (!protections[j]) continue;
                unsigned long ignored;
                if (write_bytes(base+p[j].rva,reverse?p[j].replacement:p[j].expected,p[j].length,&ignored)!=AOT_OK)
                    return AOT_ROLLBACK_FAILED;
                if (!VirtualProtect(base+p[j].rva,p[j].length,protections[j],&ignored))
                    return AOT_ROLLBACK_FAILED;
            }
            return result;
        }
    }
    return AOT_OK;
}
AotStatus aot_apply(uint8_t *b,size_t n,const AotPatch *p,size_t count) {
    return transaction(b,n,p,count,0);
}
AotStatus aot_revert(uint8_t *b,size_t n,const AotPatch *p,size_t count) {
    return transaction(b,n,p,count,1);
}
