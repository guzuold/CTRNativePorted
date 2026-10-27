// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "owner_dispatch.hpp"
#include "owner_dispatch_api.h"
namespace ctrport {
inline int owner_dispatch_invoke(void* context, CtrOwnerAction action, void* user, int* output) noexcept {
    if (!context || !action || !output) return 1;
    try {
        const auto result = static_cast<OwnerDispatch*>(context)->invoke([=] { return action(user); });
        *output = result;
        return 0;
    } catch (...) {
        return 1;
    }
}
inline CtrOwnerDispatchApi make_owner_dispatch_api(OwnerDispatch& dispatcher) {
    return {sizeof(CtrOwnerDispatchApi), 1, &dispatcher, &owner_dispatch_invoke};
}
struct OwnerApiFixture {
    std::thread::id owner;
    bool observed = false;
};
}
