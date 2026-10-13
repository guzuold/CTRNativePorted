// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
namespace CtrPm4 {
// Merge actual interleaved GPU samples into a detiled prior backing. This does
// not establish GPU identity, perform tiling, publish bytes or remove a guard.
// Only the reached 1920x1080 MS2 D32 plane is admitted. Padding is retained.
inline int MergeDepthMS2Linear(const void* prior,uint64_t priorBytes,
 const void* samples,uint64_t sampleBytes,std::vector<unsigned char>& output) {
 constexpr uint64_t backing=1920ULL*1152*2*4,logical=1920ULL*1080*2*4;
 if(!prior || !samples || priorBytes!=backing || sampleBytes!=logical)return 60;
 std::vector<unsigned char> prepared;
 try {prepared.resize(static_cast<size_t>(backing));}catch(...){return 112;}
 std::memcpy(prepared.data(),prior,static_cast<size_t>(backing));
 std::memcpy(prepared.data(),samples,static_cast<size_t>(logical));
 output.swap(prepared);return 0;
}
}
