#include "recomp_types.h"
#include "platform/vcpu.h"
#include "d3d_funcs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if XBOX_HAS_VCPU
void compat_host_enter(void);
void compat_host_leave(void);
#else
static inline void compat_host_enter(void) {}
static inline void compat_host_leave(void) {}
#endif

#define FFR_ENTRY(va, end, name, words) \
    _Static_assert(words <= sizeof(((FfrD3dArguments *)0)->stack) / sizeof(uint32_t), "stack snapshot too small");
#include "../../tools/d3d_symbols.txt"
#undef FFR_ENTRY

static RECOMP_TLS const FfrD3dHooks *bound;
static RECOMP_TLS int trace;

void ffr_d3d_bind_thread(const FfrD3dHooks *hooks)
{
    bound = hooks;
    const char *value = getenv("FFR_D3D_TRACE");
    trace = value && strcmp(value, "1") == 0;
}

static void run(const FfrD3dEntry *entry, const FfrD3dHooks *hooks)
{
    if (!hooks && !trace) {
        entry->original();
        return;
    }
    FfrD3dArguments args = {
        .eax = g_eax, .ebx = g_ebx, .ecx = g_ecx, .edx = g_edx, .esp = g_esp,
        .ebp = g_ebp, .esi = g_esi, .edi = g_edi, .return_va = MEM32(g_esp)
    };
    for (unsigned i = 0; i < entry->stack_words; ++i)
        args.stack[i] = MEM32(g_esp + 4 + 4 * i);

    compat_host_enter();
    if (trace) {
        fprintf(stderr, "[FFR D3D] %s from=%08x ecx=%08x edx=%08x edi=%08x",
                entry->name, args.return_va, args.ecx, args.edx, args.edi);
        for (unsigned i = 0; i < entry->stack_words; ++i)
            fprintf(stderr, " %08x", args.stack[i]);
        fputc('\n', stderr);
    }
    if (hooks && hooks->before)
        hooks->before(entry, &args, hooks->context);
    FfrD3dId id = (FfrD3dId)(entry - ffr_d3d_entries);
    int replaced = hooks && hooks->replace[id] && hooks->replace[id](&args, hooks->context);
    compat_host_leave();
    if (!replaced)
        entry->original();

    if (hooks && hooks->after) {
        FfrD3dResult result = {g_eax, g_ecx, g_edx, g_ebx, g_esp, g_ebp, g_esi, g_edi};
        compat_host_enter();
        hooks->after(entry, &args, &result, replaced, hooks->context);
        compat_host_leave();
    }
}

void ffr_d3d_dispatch(FfrD3dId id)
{
    run(&ffr_d3d_entries[id], bound);
}

int ffr_d3d_observe_call(uint32_t address, FfrD3dObserver observer, void *context)
{
    const FfrD3dEntry *entry = ffr_d3d_find(address);
    if (!entry)
        return 0;
    FfrD3dHooks hooks = {.before = observer, .context = context};
    run(entry, observer ? &hooks : NULL);
    return 1;
}
