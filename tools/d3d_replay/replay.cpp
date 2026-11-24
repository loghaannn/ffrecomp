// Compile the real backend into this standalone test so a probe can read its
// private targets. Only the NV2A device provider is substituted; no presenter.
#include "vk/backend.cpp"
#include <iostream>
#include <fstream>
#include <stdexcept>

namespace {
nv2a_vk_shared shared{};
void check(VkResult r) {
    if (r != VK_SUCCESS) throw std::runtime_error("Vulkan result " + std::to_string(r));
}
void require(bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
}
uint32_t word() {
    uint32_t result;
    if (!(std::cin >> result)) throw std::runtime_error("truncated capture");
    return result;
}
void open_device() {
    check(volkInitialize());
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    const char* layer = "VK_LAYER_KHRONOS_validation";
    if (std::getenv("FFR_REPLAY_VALIDATE")) {
        ci.enabledLayerCount = 1;
        ci.ppEnabledLayerNames = &layer;
    }
    check(vkCreateInstance(&ci, nullptr, &shared.instance));
    volkLoadInstance(shared.instance);
    uint32_t n = 0;
    check(vkEnumeratePhysicalDevices(shared.instance, &n, nullptr));
    std::vector<VkPhysicalDevice> devices(n);
    check(vkEnumeratePhysicalDevices(shared.instance, &n, devices.data()));
    for (auto phys : devices) {
        vkGetPhysicalDeviceQueueFamilyProperties(phys, &n, nullptr);
        std::vector<VkQueueFamilyProperties> queues(n);
        vkGetPhysicalDeviceQueueFamilyProperties(phys, &n, queues.data());
        for (unsigned i = 0; i < n; ++i) {
            if (!(queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
            float priority = 1;
            VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            qi.queueFamilyIndex = i; qi.queueCount = 1; qi.pQueuePriorities = &priority;
            VkPhysicalDeviceVulkan13Features features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
            features.dynamicRendering = features.synchronization2 = true;
            const char* extension = VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME;
            VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            VkPhysicalDeviceFeatures supported{};
            vkGetPhysicalDeviceFeatures(phys, &supported);
            if (!supported.occlusionQueryPrecise) continue;
            VkPhysicalDeviceFeatures enabled{};
            enabled.occlusionQueryPrecise = true;
            dc.pEnabledFeatures = &enabled;
            dc.pNext = &features; dc.pQueueCreateInfos = &qi; dc.queueCreateInfoCount = 1;
            dc.ppEnabledExtensionNames = &extension; dc.enabledExtensionCount = 1;
            if (vkCreateDevice(phys, &dc, nullptr, &shared.device) != VK_SUCCESS) continue;
            volkLoadDevice(shared.device);
            shared.physical_device = phys; shared.queue_family = i; shared.features13 = true;
            vkGetDeviceQueue(shared.device, i, 0, &shared.queue);
            return;
        }
    }
    throw std::runtime_error("no compatible graphics queue");
}
VkQueryPool query;
uint32_t draw_calls;
PFN_vkCmdDraw draw;
VKAPI_ATTR void VKAPI_CALL counted_draw(VkCommandBuffer cmd, uint32_t n, uint32_t instances, uint32_t first, uint32_t instance) {
    require(n == 4 && instances == 1 && first == 0 && instance == 0, "unexpected draw");
    vkCmdBeginQuery(cmd, query, 0, VK_QUERY_CONTROL_PRECISE_BIT);
    draw(cmd, n, instances, first, instance);
    vkCmdEndQuery(cmd, query, 0);
    ++draw_calls;
}
}

extern "C" {
ptrdiff_t g_xbox_mem_offset;
int nv2a_vk_share(nv2a_vk_shared* out) { *out = shared; return 1; }
void nv2a_vk_queue_lock() {}
void nv2a_vk_queue_unlock() {}
void nv2a_quality_get(int* scale, int* aniso, int* shadow, int* msaa) {
    if (scale) *scale = 1;
    if (aniso) *aniso = 0;
    if (shadow) *shadow = 1;
    if (msaa) *msaa = 1;
}
}

namespace gpu {
namespace {
struct CaptureReplay {
    static void run(bool reject_depth) {
        State state;
        state.vs_handle = word();
        uint32_t prim = word(), stride = word();
        std::array<uint32_t,20> payload;
        for (auto& v : payload) v = word();
        Assembled a;
        require(AssembleInlineStrip(state.vs_handle, prim, 4, stride,
                reinterpret_cast<const uint8_t*>(payload.data()), sizeof(payload), a), "inline assembly failed");
        state.rt.header = word(); state.zs.header = word();
        for (Surface* s : {&state.rt, &state.zs}) {
            word(); s->data = word(); word(); s->format = word(); s->size = word();
        }
        state.vs = std::make_shared<VertexShader>();
        unsigned length = word();
        require(length == 48, "unexpected VP size");
        for (unsigned i = 0; i < length; ++i) state.vs->program.push_back(word());
        for (unsigned i = word(); i; --i) {
            uint32_t k = word(), v = word();
            state.vsc.at(k/4)[k%4] = Bits(v);
        }
        for (unsigned i = word(); i; --i) {
            uint32_t k = word(), v = word();
            state.simple.at(k/4) = v;
        }
        auto method = [&](unsigned m) {return state.simple.at(m/4);};
        state.viewport = {method(0x200)&65535, method(0x204)&65535, method(0x200)>>16, method(0x204)>>16,0,1};
        require(state.viewport.w==640 && state.viewport.h==480 && method(0x208)==0x124, "unexpected surfaces");
        state.zscale = Bits(method(0xaf8));
        state.rs[kRsZEnable] = method(0x30c);
        state.rs[kRsStencilEnable] = method(0x32c);
        state.rs[kRsFrontFace] = method(0x3a0);
        state.rs[kRsCullMode] = method(0x308) ? method(0x39c) : 0;
        state.rs[kRsFillMode] = method(0x38c);
        // Feed effective FIFO combiners to the existing programmable combiner
        // translator even though the high-level guest PS handle was zero.
        state.ps = std::make_shared<PixelShader>();
        auto& def = state.ps->def;
        for (unsigned i=0; i<8; ++i) {
            def[i]=method(0x260+4*i); def[26+i]=method(0xaa0+4*i);
            def[34+i]=method(0xac0+4*i); def[45+i]=method(0x1e40+4*i);
        }
        def[8]=method(0x288); def[9]=method(0x28c); def[53]=method(0x1e60);
        require(!method(0x1e70), "textured replay unsupported");

        VulkanBackend b;
        require(b.Init(640,480), "backend initialization failed");
        require(b.ds_format_ == VK_FORMAT_D24_UNORM_S8_UINT, "D24S8 required for this comparison");
        VkQueryPoolCreateInfo qp{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        qp.queryType=VK_QUERY_TYPE_OCCLUSION; qp.queryCount=1;
        check(vkCreateQueryPool(shared.device,&qp,nullptr,&query));
        vkCmdResetQueryPool(b.cmd_,query,0,1);
        // Diagnostic attachment contents only. Guest state and inputs stay captured.
        b.Clear(state,0,nullptr,kClearTarget|kClearZ|kClearStencil,0xffcc3399,reject_depth ? 0.0f : 1.0f,0xa5);
        draw=b.d_.vkCmdDraw; b.d_.vkCmdDraw=counted_draw;
        b.DrawPrepared(state,a,prim,4,true);
        require(draw_calls==1,"backend did not record the draw");
        b.EndRendering();
        constexpr size_t pixels=640*480;
        VkBufferCreateInfo bc{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bc.size=pixels*9; bc.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VmaAllocationCreateInfo ac{};
        ac.usage=VMA_MEMORY_USAGE_AUTO;
        ac.flags=VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT|VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VkBuffer buffer; VmaAllocation allocation; VmaAllocationInfo info;
        check(vmaCreateBuffer(b.vma_,&bc,&ac,&buffer,&allocation,&info));
        auto copy=[&](const Surface& s,VkImageAspectFlagBits aspect,VkDeviceSize offset) {
            Image& img=b.EnsureTarget(s,aspect!=VK_IMAGE_ASPECT_COLOR_BIT)->img;
            b.Barrier(img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkBufferImageCopy region{};
            region.bufferOffset=offset; region.imageSubresource={aspect,0,0,1};
            region.imageExtent={640,480,1};
            vkCmdCopyImageToBuffer(b.cmd_,img.image,img.layout,buffer,1,&region);
        };
        copy(state.rt,VK_IMAGE_ASPECT_COLOR_BIT,0);
        copy(state.zs,VK_IMAGE_ASPECT_DEPTH_BIT,pixels*4);
        copy(state.zs,VK_IMAGE_ASPECT_STENCIL_BIT,pixels*8);
        b.Submit();
        check(vmaInvalidateAllocation(b.vma_,allocation,0,VK_WHOLE_SIZE));
        uint64_t passed=0;
        check(vkGetQueryPoolResults(shared.device,query,0,1,sizeof(passed),&passed,8,VK_QUERY_RESULT_64_BIT));
        const auto* data=static_cast<const uint32_t*>(info.pMappedData);
        unsigned covered=0,black=0,stencil_same=0,mask_errors=0;
        uint32_t zmin=0xffffff,zmax=0;
        for(size_t i=0;i<pixels;++i){
            uint32_t z=data[pixels+i]&0xffffff;
            bool hit=z!=(reject_depth ? 0u : 0xffffffu), rgb=(data[i]&0xffffff)==0;
            covered+=hit; black+=rgb;
            mask_errors+=hit!=rgb || (!hit && data[i]!=0xffcc3399);
            stencil_same+=static_cast<const uint8_t*>(info.pMappedData)[pixels*8+i]==0xa5;
            if(hit && data[i]!=0xff000000) ++mask_errors;
            zmin = std::min(zmin, z);
            zmax = std::max(zmax, z);
        }
        std::printf("initial_depth=%d draws=%u modules=%zu pipelines=%zu occlusion=%llu black=%u depth_changed=%u z24=[%06x,%06x] stencil_preserved=%u mask_errors=%u\n",
                reject_depth ? 0 : 1,draw_calls,b.modules_.size(),b.pipelines_.size(),(unsigned long long)passed,black,covered,zmin,zmax,stencil_same,mask_errors);
        vkDestroyQueryPool(shared.device,query,nullptr);vmaDestroyBuffer(b.vma_,buffer,allocation);
        const size_t expected = reject_depth ? 0 : pixels;
        // Uniform z, RHW=1 throughout the strip. Compare one D24 rounding
        // unit about the payload value, independently of the translated VP.
        float input_depth = Bits(payload[2]);
        uint32_t expected_z = uint32_t(std::llround(double(input_depth) * 0xffffff));
        require(passed == expected && covered == expected && !mask_errors &&
                stencil_same == pixels, "coverage, color or stencil mismatch");
        require(reject_depth || (zmin == expected_z && zmax == expected_z), "depth value mismatch");
        require(b.spirv_by_src_.size() == 2 && b.pipelines_.size() == 1, "missing real pipeline");
        for (const auto& [key, code] : b.spirv_by_src_) {
            std::ofstream out(std::to_string(key) + ".spv", std::ios::binary);
            require(bool(out.write(reinterpret_cast<const char*>(code.data()), code.size()*4)), "SPIR-V output failed");
        }
    }
};
}
}
int main(int argc, char** argv){
    try {
        require(argc == 3, "pre_low.bin and initial depth required");
        std::vector<uint8_t> guest(64u << 20);
        std::ifstream low(argv[1], std::ios::binary);
        require(bool(low.read(reinterpret_cast<char*>(guest.data()), guest.size())), "incomplete low memory");
        g_xbox_mem_offset = reinterpret_cast<ptrdiff_t>(guest.data());
        open_device(); gpu::CaptureReplay::run(std::string(argv[2]) == "0"); vkDestroyDevice(shared.device,nullptr);
          vkDestroyInstance(shared.instance,nullptr); return 0; }
    catch(const std::exception& error){ std::fprintf(stderr,"%s\n",error.what()); return 1; }
}
