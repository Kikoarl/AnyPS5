#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_STATE_INCLUDE_STATUS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_STATE_INCLUDE_STATUS_HPP

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {
int APS5_VABI sceAgcDriverSetSubmitValidationMode(uint32_t mode);
int APS5_VABI sceAgcDriverGetSubmitValidationMode(uint32_t* mode);
int APS5_VABI sceAgcDriverSetSubmitValidationConfig(const void* config);
int APS5_VABI sceAgcDriverGetSubmitValidationConfig(void* config);
int APS5_VABI sceAgcDriverSetValidationErrorOutputFrequency(uint32_t frequency);
}

#endif

