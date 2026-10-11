#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::HtileBytes;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Surface;
using DepthFastClearHarness::Width;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t FormatR8Uint = 5;
constexpr std::uint32_t FormatR32Float = 22;
constexpr std::uint32_t FormatR11G11B10Float = 36;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t BlockBytes = 2u << 20u;

alignas(256) constexpr std::array<std::uint32_t, 5> StoreCode{
    0x7e020280, 0x7e04020c, 0xf0201108, 0x00010200, 0xbf810000,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "cannot allocate guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 8> TextureDescriptor(std::uint64_t address, std::uint32_t format, std::uint32_t width, std::uint32_t height) {
    const auto base = address >> 8u;
    const auto widthMinus1 = width - 1u;
    const auto heightMinus1 = height - 1u;
    return {
        static_cast<std::uint32_t>(base),
        static_cast<std::uint32_t>((base >> 32u) & 0xffu) | ((format & 0x1ffu) << 20u) | ((widthMinus1 & 0x3u) << 30u),
        ((widthMinus1 >> 2u) & 0xfffu) | ((heightMinus1 & 0x3fffu) << 14u),
        0xfacu | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

void Dispatch(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;

        GuestBlock memory;
        const auto depthAddress = reinterpret_cast<std::uint64_t>(memory.Data());
        const auto stencilAddress = depthAddress + Width * Height * sizeof(float);
        const auto htileAddress = stencilAddress + 65536;

        const auto shaders = Compile(*device);
        const Surface surface{depthAddress, stencilAddress, htileAddress, false, VK_FORMAT_D32_SFLOAT_S8_UINT};

        // 1. Initial draw writes 0.5f to the depth surface.
        Require(Draw(*device, shaders, surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "initial draw did not write depth");

        // The surface now has 0.5f; a draw comparing GREATER against 0.5f should fail (0 covered).
        Require(Draw(*device, shaders, surface, {.depthCompare = VK_COMPARE_OP_GREATER}) == 0u, "draw at 0.5f passed GREATER against 0.5f");

        // 2. Refusal test 1: storage access to stencil plane must throw.
        bool stencilRefused = false;
        try {
            std::vector<std::uint32_t> userData(16, 0u);
            const auto texture = TextureDescriptor(stencilAddress, FormatR32Float, Width, Height);
            std::copy(texture.begin(), texture.end(), userData.begin() + 4);
            Dispatch(*device, StoreCode, userData);
        } catch (const std::runtime_error& error) {
            stencilRefused = std::string(error.what()).find("stencil plane") != std::string::npos;
        }
        Require(stencilRefused, "storage access to a depth surface's stencil plane did not throw");

        // 3. Refusal test 2: incompatible storage format (1-byte format over 4-byte depth) must throw.
        bool formatRefused = false;
        try {
            std::vector<std::uint32_t> userData(16, 0u);
            const auto texture = TextureDescriptor(depthAddress, FormatR8Uint, Width, Height);
            std::copy(texture.begin(), texture.end(), userData.begin() + 4);
            Dispatch(*device, StoreCode, userData);
        } catch (const std::runtime_error& error) {
            formatRefused = std::string(error.what()).find("storage image access to depth surface") != std::string::npos;
        }
        Require(formatRefused, "incompatible storage format over depth surface did not throw");

        // 4. Format 122 (B10G11R11) storage access: 4-byte format over 4-byte depth must be accepted.
        {
            std::vector<std::uint32_t> userData(16, 0u);
            const auto texture = TextureDescriptor(depthAddress, FormatR11G11B10Float, Width, Height);
            std::copy(texture.begin(), texture.end(), userData.begin() + 4);
            Dispatch(*device, StoreCode, userData);
        }

        std::puts("depth surface storage tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
