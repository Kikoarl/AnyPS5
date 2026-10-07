#include "prx/libSceAgc/Misc/include/Platform.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::atomic<std::uint32_t> g_shaderInstrumentation{0};

}

extern "C" {

int APS5_VABI sceAgcGetIsTrinityMode(bool* isTrinityMode) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(isTrinityMode), alignof(bool), __func__);
    *isTrinityMode = false;
    return 0;
}

std::uint32_t APS5_VABI sceAgcGetShaderInstrumentation(void) {
    return g_shaderInstrumentation.load(std::memory_order_relaxed);
}

int APS5_VABI sceAgcSetShaderInstrumentation(std::uint32_t flags) {
    g_shaderInstrumentation.store(flags, std::memory_order_relaxed);
    return 0;
}

}

