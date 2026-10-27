// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
#include <vector>
namespace CtrPm4 {
struct SrtProvenanceRoot {
    uint32_t sgpr;
    std::vector<uint32_t> offsets;
};
struct SrtProvenanceWord {
    uint32_t flat_index, root_sgpr, source_dword;
};
// Metadata only. Mirrors public leaf VisitPointer order, never reads guest data.
// A compacted flat index is not a source byte offset; preserve both identities.
inline int BuildSrtLeafProvenance(uint32_t declaredWords, const std::vector<SrtProvenanceRoot>& roots,
                                  std::vector<SrtProvenanceWord>& output) {
    if (declaredWords > 16 || roots.empty() || roots.size() > 16) return 60;
    std::vector<SrtProvenanceWord> prepared;
    uint32_t previous = 0;
    bool first = true;
    for (const auto& root : roots) {
        if ((!first && root.sgpr <= previous) || root.sgpr >= declaredWords || declaredWords - root.sgpr < 2 ||
            root.offsets.empty() || root.offsets.size() > 4096)
            return 88;
        previous = root.sgpr;
        first = false;
        if (prepared.size() > 65520 - root.offsets.size()) return 88;
        for (size_t i = 0; i < root.offsets.size(); ++i) {
            if (root.offsets[i] >= 4194304 || (i && root.offsets[i] <= root.offsets[i - 1])) return 88;
            prepared.push_back({static_cast<uint32_t>(16 + prepared.size()), root.sgpr, root.offsets[i]});
        }
    }
    output = std::move(prepared);
    return 0;
}
}
