// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "owned_fetch_context.hpp"
#include <span>
#include <algorithm>
namespace CtrPm4 {
struct SrtLeafRoot {
    uint32_t sgpr;
    std::vector<uint32_t> offsets;
};
// Public VisitPointer leaf order: roots by SGPR, source offsets sorted, append
// after all16 user DWORDs. This captures only compiler-proven leaf roots;
// callers must reject any pointer-use child before entering this function.
inline int CaptureSrtLeafRoots(const OwnedFetchContext& context, std::span<const uint32_t> userdata,
                               std::span<const SrtLeafRoot> roots, std::vector<uint32_t>& output) {
    if (context.declaredWords > 16 || userdata.size() < context.declaredWords || roots.empty() || roots.size() > 16)
        return 60;
    std::vector<uint32_t> prepared(16, 0);
    std::copy_n(userdata.begin(), context.declaredWords, prepared.begin());
    uint32_t previousRoot = 0;
    bool first = true;
    for (const auto& root : roots) {
        if ((!first && root.sgpr <= previousRoot) || root.sgpr >= context.declaredWords ||
            context.declaredWords - root.sgpr < 2 || root.offsets.empty())
            return 88;
        first = false;
        previousRoot = root.sgpr;
        if (root.offsets.size() > 4096 || prepared.size() > 65536 - root.offsets.size()) return 88;
        for (size_t i = 1; i < root.offsets.size(); ++i)
            if (root.offsets[i] <= root.offsets[i - 1]) return 88;
        const uint64_t bytes = (uint64_t(root.offsets.back()) + 1) * 4;
        if (bytes > 16 * 1024 * 1024) return 88;
        uint64_t address =
            (uint64_t(userdata[root.sgpr]) | (uint64_t(userdata[root.sgpr + 1]) << 32)) & 0xffffffffffffULL;
        std::vector<uint32_t> table;
        const int status = CaptureOwnedNestedRange(context.inputs, context.ranges, address, bytes, bytes, table);
        if (status) return status;
        for (auto offset : root.offsets) prepared.push_back(table[offset]);
    }
    output = std::move(prepared);
    return 0;
}
}
