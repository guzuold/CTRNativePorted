// SPDX-License-Identifier: GPL-2.0-or-later
// Registry of ctr_hle functions (see ctr_hle.h).
#include <cstdlib>
#include <cstring>
#include "ctr_hle.h"

namespace ctr_hle {
static Registration* registrations = nullptr;
Registration::Registration(const char* nid_, const char* library_, const void* function_, bool online_only_)
    : nid{nid_}, library{library_}, function{function_}, online_only{online_only_}, next{registrations} {
    registrations = this;
}
bool online_enabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("CTR_ONLINE");
        return value && !std::strcmp(value, "1");
    }();
    return enabled;
}
} // namespace ctr_hle

CTR_HLE_API std::uintptr_t ctr_hle_resolve(const char* nid, const char* library) {
    for (auto* r = ctr_hle::registrations; r; r = r->next) {
        if (r->online_only && !ctr_hle::online_enabled()) {
            continue;
        }
        if (!std::strcmp(r->nid, nid) && (!library || !std::strcmp(r->library, library))) {
            return reinterpret_cast<std::uintptr_t>(r->function);
        }
    }
    return 0;
}

static CtrHleHostV1 g_host{};
const CtrHleHostV1* ctr_hle::host() {
    return g_host.version == 1 ? &g_host : nullptr;
}
CTR_HLE_API int ctr_hle_set_host(const CtrHleHostV1* host) {
    if (!host || host->version != 1 || !host->mount || !host->unmount || !host->title_id) {
        return -1;
    }
    g_host = *host;
    return 0;
}

CTR_HLE_API unsigned ctr_hle_count(void) {
    unsigned count = 0;
    for (auto* r = ctr_hle::registrations; r; r = r->next) {
        if (!r->online_only || ctr_hle::online_enabled()) {
            ++count;
        }
    }
    return count;
}
