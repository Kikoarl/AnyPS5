#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>

extern "C" int APS5_VABI sceAgcGetIsTrinityMode(bool* isTrinityMode);
extern "C" std::uint32_t APS5_VABI sceAgcGetShaderInstrumentation(void);
extern "C" int APS5_VABI sceAgcSetShaderInstrumentation(std::uint32_t flags);

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        check(error.what()[0] != '\0', "empty exception message");
        return;
    }
    throw std::runtime_error("expected an exception");
}

void testTrinityMode() {
    std::array<bool, 3> flags{true, true, true};
    check(sceAgcGetIsTrinityMode(&flags[1]) == 0, "Trinity mode query did not return 0");
    check(!flags[1], "base PS5 GPU reported as Trinity");
    check(flags[0] && flags[2], "Trinity mode query wrote past its one-byte flag");
}

void testRejections() {
    expectFailure([] { sceAgcGetIsTrinityMode(nullptr); });
}

void testShaderInstrumentation() {
    check(sceAgcGetShaderInstrumentation() == 0, "initial shader instrumentation was not 0");
    check(sceAgcSetShaderInstrumentation(0x12345678u) == 0, "sceAgcSetShaderInstrumentation failed");
    check(sceAgcGetShaderInstrumentation() == 0x12345678u, "sceAgcGetShaderInstrumentation did not return set flags");
    check(sceAgcSetShaderInstrumentation(0) == 0, "resetting shader instrumentation failed");
    check(sceAgcGetShaderInstrumentation() == 0, "shader instrumentation was not reset to 0");
}

}

int main() {
    try {
        testTrinityMode();
        testRejections();
        testShaderInstrumentation();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC platform tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
