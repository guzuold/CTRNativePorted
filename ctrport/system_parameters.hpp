// SPDX-License-Identifier: GPL-2.0-only
// Own virtual-console configuration; public param ABI from shadPS4 systemservice.h/cpp
// e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
#pragma once
#include "kernel_time.hpp"
#include <cstdlib>
#include <array>
#include <climits>
#include "original_call.hpp"
namespace ctrport {
inline int system_parameter(int id, int* output) {
    if (!output) return static_cast<int>(0x80a10003U);
    int value = 0;
    if (id == 1) {
        // Deliberate runtime default US English; no host/emulator profile copied.
        value = 1;
        char setting[32]{};
        auto length = GetEnvironmentVariableA("CTR_SYSTEM_LANGUAGE", setting, 32);
        if (length >= 32) throw std::runtime_error("CTR_SYSTEM_LANGUAGE too long");
        if (length) {
            char* end = nullptr;
            auto parsed = std::strtol(setting, &end, 10);
            // Public OpenOrbis language ABI; Arabic (21) is present in CUSA14876 EU.
            if (!*setting || *end || parsed < 0 || (parsed > 18 && parsed != 21))
                throw std::runtime_error("CTR_SYSTEM_LANGUAGE must be integer0..18 or21");
            value = static_cast<int>(parsed);
        }
    } else if (id == 1000) {
        // Runtime-owned virtual-console preference: Cross=1, Circle=0.
        // Public enum OrbisSystemParamEnterButtonAssign, not a fabricated
        // controller/query result. No host or emulator profile imported.
        value = 1;
        char setting[16]{};
        const auto length = GetEnvironmentVariableA("CTR_ENTER_BUTTON", setting, 16);
        if (length >= 16) throw std::runtime_error("CTR_ENTER_BUTTON too long");
        if (length) {
            if (std::strcmp(setting, "0") && std::strcmp(setting, "1"))
                throw std::runtime_error("CTR_ENTER_BUTTON must be0(Circle) or1(Cross)");
            value = setting[0] - '0';
        }
    } else
        throw std::runtime_error("System parameter not configured");
    require_time_output(output, 4);
    *output = value;
    return 0;
}
// Public systemservice.h86 and systemservice.cpp1777: status prefix ABI.
// Runtime-owned foreground execution policy: no shell overlay, broadcast or VR
// session exists. CPU virtual affinity domain is seven cores. Host focus does
// not suspend guest execution. Events are counted from the owned queue only.
inline std::mutex system_event_lock;
inline std::vector<std::uint32_t> system_pending_events;
inline int system_status(void* output) {
    if (!output) return static_cast<int>(0x80a10003U);
    require_time_output(output, 9);
    std::lock_guard<std::mutex> guard(system_event_lock);
    if (system_pending_events.size() > static_cast<std::size_t>(INT_MAX))
        throw std::runtime_error("System event count overflow");
    const auto count = static_cast<std::int32_t>(system_pending_events.size());
    auto* bytes = static_cast<unsigned char*>(output);
    std::memcpy(bytes, &count, 4);
    bytes[4] = 0;
    bytes[5] = 0;
    bytes[6] = 1;
    bytes[7] = 0;
    bytes[8] = 0;
    return 0;
}
extern "C" int ctr_system_status(void* output, std::uint64_t caller) {
    try {
        const int result = system_status(output);
        std::printf("{\"scope\":\"owned_system_status_return\",\"caller\":%llu,\"result\":%d}\n",
                    static_cast<unsigned long long>(caller), result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=rPo6tV8D9bM library=libSceSystemService caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_system_status();

}
