// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "service_abi.h"
#include <cstring>
#include <stdexcept>
namespace ctrport {
inline std::uint64_t call_service_integer_adapter(const void* target, std::uint32_t signature, std::uint64_t a,
                                                  std::uint64_t b, std::uint64_t c) {
    if (!target) throw std::runtime_error("Service adapter target missing");
    if (signature == 1) {
        using Call = std::uint64_t (*)(std::uint64_t, std::uint64_t, std::uint64_t);
        Call call{};
        std::memcpy(&call, &target, sizeof(call));
        return call(a, b, c);
    }
    if (signature == 2) {
        CtrInteger3Transport call{};
        std::memcpy(&call, &target, sizeof(call));
        std::uint64_t result = 0;
        if (call(a, b, c, &result) != 0) throw std::runtime_error("Service adapter transport unsupported");
        return result;
    }
    throw std::runtime_error("Service adapter signature unsupported");
}
inline std::uint32_t service_transport_positive(std::uint64_t a, std::uint64_t b, std::uint64_t c,
                                                std::uint64_t* result) {
    *result = a + b + c;
    return 0;
}
inline std::uint32_t service_transport_failure(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t* result) {
    *result = 123;
    return 1;
}
}
