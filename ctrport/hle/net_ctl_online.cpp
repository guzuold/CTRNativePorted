// SPDX-License-Identifier: GPL-2.0-or-later
// libSceNetCtl in the online runtime (R8, active only with CTR_ONLINE=1): the console is
// connected (IP obtained) with the host's address on the route to the project server.
// Behaviour of pinned shadPS4 v0.18.0 src/core/libraries/network/netctl.cpp and
// net_ctl_codes.h (GPL-2.0-or-later). The NpToolkit callbacks receive no events, as in
// shadPS4: the toolkit reads the state with sceNetCtlGetState.
#include <winsock2.h>
#include <array>
#include <cstring>
#include <mutex>
#include <string>
#include "ctr_hle.h"
#include "online_config.h"

namespace {
constexpr int kInvalidAddr = static_cast<int>(0x80412107u);
constexpr int kInvalidSize = static_cast<int>(0x80412111u);
constexpr int kCallbackMax = static_cast<int>(0x80412103u);
constexpr int kInvalidId = static_cast<int>(0x80412105u);
constexpr int kNotAvail = static_cast<int>(0x80412109u);
constexpr int kStateIpObtained = 3;

struct NatInfo {
    std::uint32_t size;
    std::int32_t stun_status;
    std::int32_t nat_type;
    std::uint32_t mapped_addr;
};
static_assert(sizeof(NatInfo) == 0x10);

using ToolkitCallback = void(CTR_SYSV*)(int event, void* argument);
struct ToolkitEntry {
    ToolkitCallback function;
    void* argument;
};
std::mutex toolkit_mutex;
std::array<ToolkitEntry, 8> toolkit_callbacks{};

void log_netctl(const char* library, const char* function, long long result, const std::string& extra = "") {
    ctr_hle::log_call(library, function, result, extra.c_str());
}

void copy_text(char* destination, std::size_t capacity, const std::string& text) {
    std::memset(destination, 0, capacity);
    std::memcpy(destination, text.data(), text.size() < capacity - 1 ? text.size() : capacity - 1);
}
} // namespace

CTR_SYSV int sceNetCtlInit() {
    log_netctl("libSceNetCtl", "sceNetCtlInit", 0);
    return 0;
}
CTR_HLE_REGISTER_ONLINE("gky0+oaNM4k", "libSceNetCtl", sceNetCtlInit);

CTR_SYSV void sceNetCtlTerm() {
    log_netctl("libSceNetCtl", "sceNetCtlTerm", 0);
}
CTR_HLE_REGISTER_ONLINE("Z4wwCFiBELQ", "libSceNetCtl", sceNetCtlTerm);

CTR_SYSV int sceNetCtlGetState(int* state) {
    if (!state) {
        return kInvalidAddr;
    }
    *state = kStateIpObtained;
    return 0;
}
CTR_HLE_REGISTER_ONLINE("uBPlr0lbuiI", "libSceNetCtl", sceNetCtlGetState);

// OrbisNetCtlInfo is a 256-byte union; each code writes one member.
CTR_SYSV int sceNetCtlGetInfo(int code, void* info) {
    if (!info) {
        return kInvalidAddr;
    }
    auto* bytes = static_cast<char*>(info);
    const auto write_u32 = [&](std::uint32_t value) { std::memcpy(bytes, &value, 4); };
    const std::string local = ctr_hle::online::format_address(ctr_hle::online::local_address());
    int result = 0;
    switch (code) {
    case 1: write_u32(0); break; // device: wired
    case 2: {                    // ether address: locally administered, fixed
        const unsigned char mac[6] = {0x02, 0x43, 0x54, 0x52, 0x00, 0x01};
        std::memcpy(bytes, mac, 6);
        break;
    }
    case 3: write_u32(1500); break; // MTU
    case 4: write_u32(1); break;    // link connected
    case 11: write_u32(1); break;   // ip config: static
    case 12: copy_text(bytes, 256, ""); break;
    case 14: copy_text(bytes, 16, local); break;
    case 15: copy_text(bytes, 16, "255.255.255.0"); break;
    case 16: copy_text(bytes, 16, local); break;
    case 17:
    case 18: copy_text(bytes, 16, ctr_hle::online::format_address(ctr_hle::online::server_address())); break;
    case 19: write_u32(0); break; // http proxy off
    case 20: copy_text(bytes, 256, ""); break;
    case 21: std::memset(bytes, 0, 2); break;
    default: result = kNotAvail; break; // wireless-only codes on a wired device
    }
    // The game polls some codes every frame: log the first call of each code only.
    static std::mutex logged_mutex;
    static std::array<bool, 32> logged{};
    {
        std::scoped_lock lock{logged_mutex};
        const bool first = code < 0 || code >= static_cast<int>(logged.size()) || !logged[code];
        if (code >= 0 && code < static_cast<int>(logged.size())) {
            logged[code] = true;
        }
        if (!first) {
            return result;
        }
    }
    log_netctl("libSceNetCtl", "sceNetCtlGetInfo", result, ",\"code\":" + std::to_string(code));
    return result;
}
CTR_HLE_REGISTER_ONLINE("obuxdTiwkF8", "libSceNetCtl", sceNetCtlGetInfo);

CTR_SYSV int sceNetCtlGetNatInfo(NatInfo* info) {
    if (!info) {
        return kInvalidAddr;
    }
    if (info->size != sizeof(NatInfo)) {
        return kInvalidSize;
    }
    // NAT type 2 (moderate) with the local address until the project server reports the
    // mapped address (phase 3, STUN).
    info->stun_status = 1;
    info->nat_type = 2;
    info->mapped_addr = ctr_hle::online::local_address();
    log_netctl("libSceNetCtl", "sceNetCtlGetNatInfo", 0, ",\"nat_type\":2");
    return 0;
}
CTR_HLE_REGISTER_ONLINE("JO4yuTuMoKI", "libSceNetCtl", sceNetCtlGetNatInfo);

CTR_SYSV int sceNetCtlRegisterCallbackForNpToolkit(ToolkitCallback function, void* argument, int* id) {
    if (!function || !id) {
        return kInvalidAddr;
    }
    std::scoped_lock lock{toolkit_mutex};
    for (std::size_t i = 0; i < toolkit_callbacks.size(); ++i) {
        if (!toolkit_callbacks[i].function) {
            toolkit_callbacks[i] = {function, argument};
            *id = static_cast<int>(i);
            log_netctl("libSceNetCtlForNpToolkit", "sceNetCtlRegisterCallbackForNpToolkit", 0,
                       ",\"id\":" + std::to_string(i));
            return 0;
        }
    }
    return kCallbackMax;
}
CTR_HLE_REGISTER_ONLINE("wIsKy+TfeLs", "libSceNetCtlForNpToolkit", sceNetCtlRegisterCallbackForNpToolkit);

CTR_SYSV int sceNetCtlUnregisterCallbackForNpToolkit(int id) {
    std::scoped_lock lock{toolkit_mutex};
    if (id < 0 || id >= static_cast<int>(toolkit_callbacks.size()) || !toolkit_callbacks[id].function) {
        return kInvalidId;
    }
    toolkit_callbacks[id] = {};
    return 0;
}
CTR_HLE_REGISTER_ONLINE("2oUqKR5odGc", "libSceNetCtlForNpToolkit", sceNetCtlUnregisterCallbackForNpToolkit);

CTR_SYSV int sceNetCtlCheckCallbackForNpToolkit() {
    return 0; // the connection state never changes while the runtime is online
}
CTR_HLE_REGISTER_ONLINE("u5oqtlIP+Fw", "libSceNetCtlForNpToolkit", sceNetCtlCheckCallbackForNpToolkit);
