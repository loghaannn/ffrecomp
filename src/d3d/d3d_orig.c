#include "recomp_funcs.h"
#include "d3d_funcs.h"

#ifdef FFR_D3D_WRAPPERS
#define FFR_ENTRY(va, end, name, words) void ffr_orig_##va(void);
#include "../../tools/d3d_symbols.txt"
#undef FFR_ENTRY
#define ORIGINAL(va) ffr_orig_##va
#else
#define ORIGINAL(va) sub_##va
#endif

const FfrD3dEntry ffr_d3d_entries[FFR_D3D_COUNT] = {
#define FFR_ENTRY(va, end, name, words) {0x##va, 0x##end, #name, words, ORIGINAL(va)},
#include "../../tools/d3d_symbols.txt"
#undef FFR_ENTRY
};

const FfrD3dEntry *ffr_d3d_find(uint32_t address)
{
    for (unsigned i = 0; i < FFR_D3D_COUNT; ++i)
        if (ffr_d3d_entries[i].start == address)
            return &ffr_d3d_entries[i];
    return 0;
}
