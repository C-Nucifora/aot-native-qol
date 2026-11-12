#ifndef AOT_SIDECAR_H
#define AOT_SIDECAR_H
/* Ownership ledger only. Native hook call signatures remain separately audited. */
typedef enum {
    AOT_SIDECAR_EMPTY, AOT_SIDECAR_ALLOCATED, AOT_SIDECAR_CONSTRUCTED,
    AOT_SIDECAR_INITIALIZED, AOT_SIDECAR_ATTACHED, AOT_SIDECAR_PUBLISHED
} AotSidecarPhase;
typedef struct { void *widget; AotSidecarPhase phase; unsigned borrowed; } AotSidecar;
enum { AOT_SIDECAR_CLEANUP=1, AOT_SIDECAR_DELETE=2, AOT_SIDECAR_FREE_RAW=4 };
int aot_sidecar_allocate(AotSidecar *,void *widget);
int aot_sidecar_advance(AotSidecar *,AotSidecarPhase next);
/* Call only after original main cleanup clears its borrowed child pointer. */
void aot_sidecar_main_cleared(AotSidecar *);
/* Actions must run on the owning UI thread, in bit order, before reuse.
 * CLEANUP includes native detachment. DELETE is the game scalar deleting
 * destructor with flags1 and already frees memory. FREE_RAW is exclusive.
 * Returns0 while native main still borrows widget. Ownership is consumed once. */
unsigned aot_sidecar_take_cleanup(AotSidecar *,void **widget);
#endif
