// Fatal Frame NTSC-U XDK 4721. The cull setter at E8520 emits
// NV097_SET_CULL_FACE_ENABLE and conditionally NV097_SET_CULL_FACE. Its
// front-face input is the cached word at F9FD8, written by E8590. This
// decoder is independent of guest memory and does not advance the FIFO.
#include "d3d_funcs.h"

extern "C" uint32_t ffr_d3d_cull_face(uint32_t mode, uint32_t front_face)
{
    if (!mode)
        return 0;  // guest writes enable = 0 and leaves the face unchanged
    return mode == front_face ? 0x404 : 0x405;
}

// Observe calls made by the title; original guest execution owns the FIFO and references.
#include "gpu/state.h"
#include "kernel/kernel.h"

namespace gpu {
void ObserveRenderState(const FfrD3dEntry *entry, const FfrD3dArguments *args, void *context)
{
    State& state = *static_cast<State*>(context);
    switch (entry->start) {
    case 0x000E78F0:
        if (args->stack[0] < state.textures.size())
            state.textures[args->stack[0]] = args->stack[1];
        break;
    case 0x000E7B00:
        state.index_buffer = args->stack[0];
        state.base_vertex = args->stack[1];
        state.index_data = state.index_buffer ? kernel::R32(state.index_buffer + 4) : 0;
        break;
    case 0x000EADE0:
    case 0x000EAE80:
        // F28FB..F2901: vertex base = resource Data + stream offset +
        // declaration element offset. Assembly applies the element offset itself.
        for (unsigned i = 0; i < state.streams.size(); ++i) {
            uint32_t record = 0x000FA028 + 12 * i;
            uint32_t buffer = kernel::R32(record + 8);
            auto& stream = state.streams[i];
            stream.stride = kernel::R32(record);
            stream.data_va = buffer ? mem::VirtualFromPhysical(kernel::R32(buffer + 4) +
                                                                kernel::R32(record + 4)) : 0;
        }
        break;
    case 0x000E8150:
        if ((args->ecx & ~0x1ffcu) == 0x40000u)
            state.simple[(args->ecx & 0x1ffcu) >> 2] = args->edx;
        break;
    case 0x000E8490: state.rs[kRsShadowFunc] = args->stack[0]; break;
    case 0x000E84D0: state.rs[kRsFogColor] = args->stack[0]; break;
    case 0x000E87E0: state.rs[kRsLogicOp] = args->stack[0]; break;
    case 0x000E8960: state.rs[kRsVertexBlend] = args->stack[0]; break;
    case 0x000E8B30:
    case 0x000E8B70:
        if (args->stack[0] < state.tss.size()) {
            auto slot = entry->start == 0x000E8B30 ? kTssBorderColor : kTssColorKeyColor;
            state.tss[args->stack[0]][slot] = args->stack[1];
        }
        break;
    case 0x000EBB70:
        // SelectVertexShader(NULL, start) changes the instruction start only.
        if (!args->stack[0]) break;
        [[fallthrough]];
    case 0x000EBE50:
        // EBE56 distinguishes a tagged shader from an FVF. The quad batch at
        // 122C0 binds FVF 144; vertex.cpp derives its stream layout directly.
        // Tagged programs still require declaration and instruction capture.
        state.vs_handle = args->stack[0];
        state.vs.reset();
        break;
    case 0x000EC1B0: {
        state.ps.reset();
        if (uint32_t handle = args->stack[0]) {
            uint32_t definition = kernel::R32(handle + 8);
            if (definition) {
                auto shader = std::make_shared<PixelShader>();
                shader->handle = handle;
                static_assert(sizeof(shader->def) == 0xf0);
                std::memcpy(shader->def, kernel::HostPtr(definition), sizeof(shader->def));
                state.ps = std::move(shader);
            }
        }
        break;
    }
    case 0x000EB850:
    case 0x000EB8B0:
    case 0x000EB960: {
        if (!args->edx || args->ecx >= state.vsc.size()) break;
        unsigned words = entry->start == 0x000EB960 ? args->stack[0] :
                         entry->start == 0x000EB850 ? 4 : 16;
        for (unsigned reg = args->ecx, offset = 0; reg < state.vsc.size() && words; ++reg) {
            unsigned copied = words < 4 ? words : 4;
            std::memcpy(state.vsc[reg].data(), kernel::HostPtr(args->edx + offset), copied * 4);
            offset += copied * 4;
            words -= copied;
        }
        break;
    }
    case 0x000E8520: state.rs[kRsCullMode] = args->stack[0]; break;
    case 0x000E8590: state.rs[kRsFrontFace] = args->stack[0]; break;
    case 0x000E85D0: state.rs[kRsNormalizeNormals] = args->stack[0]; break;
    }
}

static Surface ReadSurface(uint32_t header)
{
    Surface surface{};
    if (header) {
        surface.header = header;
        surface.data = kernel::R32(header + 4);
        surface.format = kernel::R32(header + 12);
        surface.size = kernel::R32(header + 16);
    }
    return surface;
}

void ObserveCompletedState(const FfrD3dEntry *entry, const FfrD3dArguments *args,
                           const FfrD3dResult *, int replaced, void *context)
{
    if (replaced) return;
    State& state = *static_cast<State*>(context);
    if (entry->start == 0x000E9010) {
        // E9026 reuses the current color target on a null argument. Read the
        // actual bindings after E92C2/E92EB, including a detached depth target.
        uint32_t device = kernel::R32(0x000F9B80);
        state.rt = ReadSurface(kernel::R32(device + 0x21B4));
        state.zs = ReadSurface(kernel::R32(device + 0x21B8));
    } else if (entry->start == 0x000E7A70 && args->stack[0] < state.palette_data.size()) {
        // E7AB5 installs the header. E7ADE reads physical Data before adding
        // the palette-size bits for NV097_SET_TEXTURE_PALETTE.
        uint32_t device = kernel::R32(0x000F9B80);
        uint32_t header = kernel::R32(device + 0xB48 + 4 * args->stack[0]);
        state.palette_data[args->stack[0]] = header ? mem::VirtualFromPhysical(kernel::R32(header + 4)) : 0;
    }
}
}  // namespace gpu
