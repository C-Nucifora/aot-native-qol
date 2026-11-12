#include "aot_mod.h"
#include "guards.h"
/* Deliberately read-only staging build. No mutation occurs under loader lock. */
static int matches(uint8_t *base,const AotPatch *p) {
    for (size_t i=0;i<p->length;i++) if (base[p->rva+i]!=p->expected[i]) return 0;
    return 1;
}
__declspec(dllexport) unsigned int AotModBuildStage(void) {
#ifdef AOT_RESOLUTION_CANDIDATE
    return 2;
#else
    return 0;
#endif
}
int __stdcall DllMain(void *self,unsigned long reason,void *reserved) {
    (void)reserved;
    if (reason!=1) return 1;
    DisableThreadLibraryCalls(self);
    uint8_t *base=(uint8_t *)GetModuleHandleW(0);
    if (!base || base[0]!='M' || base[1]!='Z') return 1;
    uint32_t pe=*(const uint32_t *)(base+0x3c);
    if (pe>0x100000 || *(const uint32_t *)(base+pe)!=0x4550 ||
        *(const uint16_t *)(base+pe+4)!=0x8664) return 1;
    uint32_t size=*(const uint32_t *)(base+pe+24+56);
    for (size_t i=0;i<sizeof(resolution_guards)/sizeof(resolution_guards[0]);i++) {
        const AotPatch *p=&resolution_guards[i];
        if (p->rva>size || p->length>size-p->rva || !matches(base,p)) {
            OutputDebugStringA("[aot-native] unsupported executable; no patches applied\n");
            return 1;
        }
    }
    /* Index9 must retain stock1080p; refuse SUWSF's overlapping4K rewrite. */
    static const uint16_t stock_table[11][3]={{640,360,480},{800,450,600},
        {1024,576,768},{1280,720,720},{1280,720,768},{1280,720,960},
        {1280,720,1024},{1600,900,900},{1600,900,1200},
        {1920,1080,1080},{1920,1080,1200}};
    if (size<0x8959b0+sizeof(stock_table)) return 1;
    const uint8_t *expected=(const uint8_t *)stock_table;
    for (size_t i=0;i<sizeof(stock_table);i++) if(base[0x8959b0+i]!=expected[i]) {
        OutputDebugStringA("[aot-native] resolution table conflict; no patches applied\n");
        return 1;
    }
#ifdef AOT_RESOLUTION_CANDIDATE
    OutputDebugStringA("[aot-native] resolution candidate loaded; deferred initializer owns activation\n");
#else
    OutputDebugStringA("[aot-native] audit build only; capability/menu audit incomplete; no patches applied\n");
#endif
    return 1;
}
