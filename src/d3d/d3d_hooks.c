#include "d3d_funcs.h"

/* Enable only with corresponding private lifted definitions. All generated
 * call sites, including same-chunk calls, keep their public sub_ name. */
#ifdef FFR_D3D_WRAPPERS
#define FFR_ENTRY(va, end, name, words) \
    void sub_##va(void) { ffr_d3d_dispatch(FFR_D3D_##name); }
#include "../../tools/d3d_symbols.txt"
#undef FFR_ENTRY
#endif
