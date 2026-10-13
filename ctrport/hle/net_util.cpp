// SPDX-License-Identifier: GPL-2.0-or-later
// libSceNet address conversion utilities (no network access). Behaviour of pinned
// shadPS4 v0.18.0 src/core/libraries/network/net.cpp1241-1256: the host inet_pton with
// the PS4 family numbers (AF_INET 2, AF_INET6 28) translated to Windows values.
#include <winsock2.h>
#include <ws2tcpip.h>
#include "ctr_hle.h"
#pragma comment(lib, "ws2_32.lib")

namespace {
int host_family(int af) {
    return af == 2 ? AF_INET : af == 28 ? AF_INET6 : -1;
}
} // namespace

CTR_SYSV int sceNetInetPton(int af, const char* src, void* dst) {
    const int family = host_family(af);
    int result = -1;
    if (family >= 0 && src && dst) {
        result = InetPtonA(family, src, dst);
    }
    char extra[64];
    std::snprintf(extra, sizeof(extra), ",\"af\":%d", af);
    ctr_hle::log_call("libSceNet", "sceNetInetPton", result, extra);
    return result;
}
CTR_HLE_REGISTER("8Kcp5d-q1Uo", "libSceNet", sceNetInetPton);

CTR_SYSV int sceNetInetPtonEx(int af, const char* src, void* dst, int) {
    return sceNetInetPton(af, src, dst);
}
CTR_HLE_REGISTER("Xn2TA2QhxHc", "libSceNet", sceNetInetPtonEx);
