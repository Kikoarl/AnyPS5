#ifndef CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_PLATFORM_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_PLATFORM_HPP

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {
int APS5_VABI sceAgcGetIsTrinityMode(bool* isTrinityMode);
std::uint32_t APS5_VABI sceAgcGetShaderInstrumentation(void);
int APS5_VABI sceAgcSetShaderInstrumentation(std::uint32_t flags);
}

#endif

