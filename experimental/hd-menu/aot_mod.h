#ifndef AOT_MOD_H
#define AOT_MOD_H
#include <stdint.h>
#include <stddef.h>
#define AOT_MAX_PATCH_BYTES 16
#define AOT_PAGE_READWRITE 0x04u
#define AOT_PAGE_EXECUTE_READWRITE 0x40u
#define AOT_MEM_RESERVE_COMMIT 0x3000u
#define AOT_MEM_RELEASE 0x8000u
#ifndef AOT_API
#define AOT_API __declspec(dllimport)
#endif
AOT_API void * __stdcall GetModuleHandleW(const uint16_t *);
AOT_API int __stdcall VirtualProtect(void *, size_t, unsigned long, unsigned long *);
AOT_API void * __stdcall VirtualAlloc(void *, size_t, unsigned long, unsigned long);
AOT_API int __stdcall VirtualFree(void *, size_t, unsigned long);
AOT_API void * __stdcall GetCurrentProcess(void);
AOT_API int __stdcall FlushInstructionCache(void *, const void *, size_t);
AOT_API void __stdcall OutputDebugStringA(const char *);
AOT_API int __stdcall DisableThreadLibraryCalls(void *);

typedef struct {
    uint32_t rva;
    uint8_t length;
    uint8_t expected[AOT_MAX_PATCH_BYTES];
    uint8_t replacement[AOT_MAX_PATCH_BYTES];
} AotPatch;
typedef enum {
    AOT_OK, AOT_BAD_RANGE, AOT_SIGNATURE_MISMATCH,
    AOT_OVERLAPPING_PATCH, AOT_PROTECTION_FAILED, AOT_CACHE_FAILED,
    AOT_ROLLBACK_FAILED
} AotStatus;
/* Caller must quiesce patched code before mutation; never call from DllMain. */
AotStatus aot_apply(uint8_t *base, size_t image_size, const AotPatch *, size_t);
AotStatus aot_revert(uint8_t *base, size_t image_size, const AotPatch *, size_t);
int aot_rel32(uintptr_t next_instruction, uintptr_t target, int32_t *out);
/* Range covers every instruction end that will reference the allocation. */
void *aot_allocate_near(uintptr_t lowest_next, uintptr_t highest_next, size_t bytes);
#endif
