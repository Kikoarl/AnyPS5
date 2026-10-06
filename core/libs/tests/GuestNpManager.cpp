#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceNpCheckNpAvailability(int req_id, const char* user, void* result);
int APS5_VABI sceNpCheckNpReachability(int req_id, int user_id);
int APS5_VABI sceNpGetState(int user_id, std::uint32_t* state);
int APS5_VABI sceNpGetNpReachabilityState(int user_id, std::uint32_t* state);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kSignedOut = static_cast<int>(0x80550006u);
constexpr std::uint32_t kStateSignedOut = 1;
constexpr std::uint32_t kReachabilityUnavailable = 0;

}

int main() {
    Require(sceNpCheckNpAvailability(0, nullptr, nullptr) == kSignedOut);
    Require(sceNpCheckNpReachability(0, 0) == kSignedOut);

    std::uint32_t state = 0;
    Require(sceNpGetState(0, &state) == 0);
    Require(state == kStateSignedOut);

    std::uint32_t reachability = 1;
    Require(sceNpGetNpReachabilityState(0, &reachability) == 0);
    Require(reachability == kReachabilityUnavailable);

    return 0;
}
