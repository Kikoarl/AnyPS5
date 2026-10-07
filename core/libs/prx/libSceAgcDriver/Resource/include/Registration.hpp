#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_RESOURCE_INCLUDE_REGISTRATION_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_RESOURCE_INCLUDE_REGISTRATION_HPP

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {
int APS5_VABI sceAgcDriverRegisterDefaultOwner(std::uint32_t* owner_handle);
int APS5_VABI sceAgcDriverRegisterMultipleResources(void);
}

#endif
