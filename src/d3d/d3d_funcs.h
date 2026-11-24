#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*FfrGuestRoutine)(void);
typedef struct {
    uint32_t start, end;
    const char *name;
    unsigned stack_words;
    FfrGuestRoutine original;
} FfrD3dEntry;

typedef enum {
#define FFR_ENTRY(va, end, name, words) FFR_D3D_##name,
#include "../../tools/d3d_symbols.txt"
#undef FFR_ENTRY
    FFR_D3D_COUNT
} FfrD3dId;

extern const FfrD3dEntry ffr_d3d_entries[FFR_D3D_COUNT];
const FfrD3dEntry *ffr_d3d_find(uint32_t address);
uint32_t ffr_d3d_cull_face(uint32_t mode, uint32_t front_face);

/* Host diagnostic only. Unknown entries return zero for ordinary dispatch.
 * For known entries the generated routine owns the guest return and all
 * register, stack, FIFO and interrupt effects. */
typedef struct {
    uint32_t ecx, edx, edi, eax, ebx, esi, ebp, esp, return_va;
    uint32_t stack[6];
} FfrD3dArguments;
typedef void (*FfrD3dObserver)(const FfrD3dEntry *, const FfrD3dArguments *, void *);
int ffr_d3d_observe_call(uint32_t address, FfrD3dObserver observer, void *context);

typedef struct {
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
} FfrD3dResult;

typedef struct {
    FfrD3dObserver before;
    void (*after)(const FfrD3dEntry *, const FfrD3dArguments *, const FfrD3dResult *,
                  int replaced, void *context);
    /* A handler may decline before changing state. On success it owns the
     * entire guest ABI including RET and every guest-visible side effect. */
    int (*replace[FFR_D3D_COUNT])(const FfrD3dArguments *, void *context);
    void *context;
} FfrD3dHooks;

/* Install per guest thread before execution. hooks must outlive its binding.
 * Null hooks and disabled tracing take the original-only path. */
void ffr_d3d_bind_thread(const FfrD3dHooks *hooks);
void ffr_d3d_dispatch(FfrD3dId id);

#ifdef __cplusplus
}
#endif
