// SPDX-License-Identifier: GPL-2.0-only
// Independent offline connection query. Public shadPS4 netctl.cpp342-349,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
// Runtime owns no NetCtl interface/session; host connectivity is not guest connectivity.
#pragma once
namespace ctrport {
inline int net_connection_state(int* output) {
    if (!output) return int(0x80412107U);
    require_time_output(output, 4);
    *output = 0;
    return 0; // DISCONNECTED; all connect APIs remain missing.
}
extern "C" int ctr_net_connection_state(int* output, std::uint64_t caller) {
    try {
        const int result = net_connection_state(output);
        std::printf(
            "{\"scope\":\"owned_netctl_connection_state\",\"nid\":\"uBPlr0lbuiI\",\"library\":\"libSceNetCtl\",\"caller\":%llu,\"result\":%d,\"state\":%d,\"guest_connections\":0}\n",
            static_cast<unsigned long long>(caller), result, result ? -1 : *output);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=uBPlr0lbuiI library=libSceNetCtl caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_net_connection_state();
}
