#ifndef AOT_NAPE_HOOK_H
#define AOT_NAPE_HOOK_H
#include "aot_mod.h"
#if defined(_WIN32)
#define NAPE_ABI
#else
#define NAPE_ABI __attribute__((ms_abi))
#endif
typedef int (NAPE_ABI *NapePermit)(void *,unsigned,float,float);
/* Original COMISS/JA ordinary paths are emitted verbatim. Only its ordered
 * skip path may consult permit; all original volatile state/flags restored. */
AotStatus nape_hook_prepare(uint8_t *,size_t,const uint8_t expected[6],NapePermit,AotPatch *);
#endif
