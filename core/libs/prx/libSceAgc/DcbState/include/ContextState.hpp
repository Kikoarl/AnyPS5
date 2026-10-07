#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_CONTEXTSTATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_CONTEXTSTATE_HPP

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {
std::uint32_t* APS5_VABI sceAgcDcbContextStateOp(CommandBuffer* buf, std::uint32_t operation);
std::uint64_t APS5_VABI sceAgcDcbContextStateOpGetSize(std::uint32_t operation);
std::uint32_t* APS5_VABI sceAgcDcbContextStateAnotherOp(CommandBuffer* buf, std::uint32_t operation);
std::uint32_t APS5_VABI sceAgcDcbSetBaseDispatchIndirectArgsGetSize(void);
std::uint32_t APS5_VABI sceAgcDcbSetBaseDrawIndirectArgsGetSize(void);
std::uint32_t APS5_VABI sceAgcDcbQueueEndOfShaderActionGetSize(void);
std::uint32_t APS5_VABI sceAgcDcbGetLodStatsGetSize(void);
}

#endif
