// SPDX-License-Identifier: GPL-2.0-only
// Public layout/reference shadPS4 kernel/time.h/cpp at e3ce810f3a653f43ac64ebab63023de281a4103a,
// GPL-2.0-or-later. Own CRT timestamp-aware conversion, seconds (not Windows bias minutes).
#pragma once
#include "kernel_time.hpp"
namespace ctrport {
struct LocalTimeInfo {
    std::int64_t utc;
    std::uint32_t offset, dst;
};
static_assert(sizeof(LocalTimeInfo) == 16);
inline int convert_local(std::int64_t utc, std::int64_t* local, LocalTimeInfo* info, std::uint64_t* dst) {
    if (!local) return static_cast<int>(0x8002000eU);
    static std::once_flag initialized;
    std::call_once(initialized, [] { _tzset(); });
    std::tm broken{};
    const __time64_t source = utc;
    if (_localtime64_s(&broken, &source)) return static_cast<int>(0x80020016U);
    auto copy = broken;
    const auto result = _mkgmtime64(&copy);
    if (result == -1) return static_cast<int>(0x80020016U);
    long dst_bias = 0;
    if (_get_dstbias(&dst_bias)) throw std::runtime_error("Host DST query failed");
    const auto daylight = broken.tm_isdst > 0 ? static_cast<std::uint64_t>(-dst_bias) : 0;
    const auto offset = result - utc;
    require_time_output(local, 8);
    require_time_output(info, 16);
    require_time_output(dst, 8);
    *local = result;
    if (info) *info = {utc, static_cast<std::uint32_t>(offset), static_cast<std::uint32_t>(daylight)};
    if (dst) *dst = daylight;
    return 0;
}
}
