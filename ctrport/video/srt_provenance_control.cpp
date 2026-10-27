// SPDX-License-Identifier: GPL-2.0-only
#include "srt_provenance_map.hpp"
#include <cstdio>
int main() {
    using namespace CtrPm4;
    std::vector<SrtProvenanceWord> map;
    if (BuildSrtLeafProvenance(16, {{8, {0, 3, 128}}, {10, {0, 4}}}, map) || map.size() != 5) return 1;
    const uint32_t expected[][3] = {{16, 8, 0}, {17, 8, 3}, {18, 8, 128}, {19, 10, 0}, {20, 10, 4}};
    for (size_t i = 0; i < 5; ++i)
        if (map[i].flat_index != expected[i][0] || map[i].root_sgpr != expected[i][1] ||
            map[i].source_dword != expected[i][2])
            return 2;
    // Equal physical bytes from overlapping roots retain distinct compiler identities.
    if (map[2].source_dword * 4 != 512 || map[3].source_dword != 0 || map[2].root_sgpr == map[3].root_sgpr) return 3;
    const auto saved = map;
    unsigned rejected = 0;
    for (const auto& roots : std::vector<std::vector<SrtProvenanceRoot>>{
             {{10, {0}}, {8, {0}}}, {{8, {3, 0}}}, {{8, {0, 0}}}, {{15, {0}}}, {{8, {4194304}}}}) {
        if (BuildSrtLeafProvenance(16, roots, map) != 88 || map.size() != saved.size()) return 4;
        for (size_t i = 0; i < map.size(); ++i)
            if (map[i].flat_index != saved[i].flat_index || map[i].root_sgpr != saved[i].root_sgpr ||
                map[i].source_dword != saved[i].source_dword)
                return 5;
        ++rejected;
    }
    if (BuildSrtLeafProvenance(17, {{8, {0}}}, map) != 60 || BuildSrtLeafProvenance(16, {}, map) != 60) return 6;
    std::printf(
        "SRT provenance metadata sparse5 exact independent triples; overlap distinct; atomic negatives%u; no guest reads; not compiler integrated\n",
        rejected + 2);
}
