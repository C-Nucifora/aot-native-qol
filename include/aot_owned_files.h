#ifndef AOT_OWNED_FILES_H
#define AOT_OWNED_FILES_H
#include "aot_mod.h"
typedef struct { uint8_t *bytes;size_t size; } AotOwnedFile;
/* Exact length, no trailing bytes; returned pages are read-only. No C runtime IO. */
AotStatus aot_file_load_exact(const uint16_t *path,size_t expected,AotOwnedFile *);
AotStatus aot_file_release(AotOwnedFile *);
/* Hash the file named by the actual main module, while denying writes/deletes. */
int aot_supported_executable(void);
void aot_sha256(const uint8_t *,size_t,uint8_t digest[32]);
#endif
