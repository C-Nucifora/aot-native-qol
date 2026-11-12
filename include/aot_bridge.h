#ifndef AOT_BRIDGE_H
#define AOT_BRIDGE_H
#include "aot_mod.h"
/* Diagnostic only:0 inactive,1 armed,2 stock loading,3 extended active,
 * 4 stock fallback with saved key held,5 retained fatal failure. No timing changes. */
unsigned aot_bridge_phase(void);
#endif
