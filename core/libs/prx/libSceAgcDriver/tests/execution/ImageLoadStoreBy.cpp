#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Height = 8;
constexpr std::uint32_t Levels = 2;
constexpr std::uint32_t Format32x4 = 77; // 32_32_32_32_FLOAT
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;

alignas(256) std::array<std::uint32_t, Threads * 16> OutputBuffer{};
alignas(4096) std::array<std::uint8_t, 8192> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 38> LoadCode{
    0x34060087, 0x36420087, 0x7e3c0300, 0x7e3e0280, 0x7e400281, 0xd5480023,
    0x02050700, 0xbf8c3f70, 0xf0000308, 0x00010a1e, 0xf0000f08, 0x00010c1e,
    0xf0040308, 0x0001101e, 0xf0040f08, 0x0001121e, 0xbf8c3f70, 0xe0701000,
    0x80000a03, 0xe0701004, 0x80000b03, 0xe0701008, 0x80000c03, 0xe070100c,
    0x80000d03, 0xe0701010, 0x80000e03, 0xe0701014, 0x80000f03, 0xe0701018,
    0x80001003, 0xe070101c, 0x80001103, 0xe0701020, 0x80001203, 0xe0701024,
    0x80001303, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 24> StoreCode{
    0x36420087, 0x7e3c0300, 0x7e3e0280, 0x7e400281, 0x7e140281, 0x7e160282,
    0x7e180283, 0x7e1a0284, 0xbf8c3f70, 0xf0200308, 0x00010a1e, 0xf0200f08,
    0x00010a1e, 0xf0240308, 0x00010a1e, 0xf0240f08, 0x00010a1e, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format, std::uint32_t swizzle, std::uint32_t levels) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        swizzle | ((levels - 1u) << 16u) | (Type2D << 28u),
        0u,
        (levels - 1u) << 4u,
        0u,
        0u,
    };
}

void CheckRecompile(const ShaderRecompiler::SpirvTarget& target) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(OutputBuffer.data(), static_cast<std::uint32_t>(OutputBuffer.size() * sizeof(std::uint32_t)));
    const auto texture = TextureDescriptor(Texels.data(), Format32x4, IdentitySwizzle, Levels);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);

    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};

    for (const auto& codeSpan : {std::span<const std::uint32_t>(LoadCode), std::span<const std::uint32_t>(StoreCode)}) {
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(codeSpan.data()), std::as_bytes(codeSpan)}}};
        ShaderRecompiler::RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(codeSpan.data()), codeSpan, 0, {}},
            {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
            target,
            {0, 0, 0, 128}
        };
        request.useCache = false;
        const auto result = ShaderRecompiler::Recompile(request);
        Require(!result.spirv.empty(), "image load/store by: recompile produced empty SPIR-V");
    }
}

void Execute(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(OutputBuffer.data(), static_cast<std::uint32_t>(OutputBuffer.size() * sizeof(std::uint32_t)));
    const auto texture = TextureDescriptor(Texels.data(), Format32x4, IdentitySwizzle, Levels);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);

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
        if (!device) {
            ShaderRecompiler::SpirvTarget fallback{};
            fallback.vulkanVersion = 0x00401000u;
            fallback.spirvVersion = 0x00010300u;
            fallback.subgroupSize = 32u;
            CheckRecompile(fallback);
            return VulkanTestSkipped;
        }

        CheckRecompile(device->Target());
        Execute(*device, LoadCode);
        Execute(*device, StoreCode);

        std::puts("image load and store by2 and by4 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
