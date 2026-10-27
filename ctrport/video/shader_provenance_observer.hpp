// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "owned_fetch_context.hpp"
#include "graphics_companion.h"
namespace CtrPm4 {
using ShaderProvenanceRecord = CtrShaderProvenanceV1;
using ShaderProvenanceObserver = void (*)(void*, const ShaderProvenanceRecord*);
inline thread_local ShaderProvenanceObserver provenanceObserver = nullptr;
inline thread_local void* provenanceObserverContext = nullptr;
inline thread_local bool provenanceCallbackActive = false;
inline int CopyProvenanceSpan(uint32_t flat, uint32_t count, ShaderProvenanceRecord* output, uint32_t capacity) {
    if (!output || !count || count > 8 || capacity < count || flat > 65536 - count) return 60;
    if (!provenanceCallbackActive || !activeFetchContext) return 91;
    ShaderProvenanceRecord prepared[8]{};
    for (uint32_t i = 0; i < count; ++i) {
        auto& r = prepared[i];
        r = {0, flat + i, 0, UINT32_MAX, UINT32_MAX, 2};
        if (flat + i < activeFetchContext->declaredWords) {
            r.root_sgpr = flat + i;
            r.source_dword = 0;
            r.source_kind = 0;
        } else
            for (const auto& item : activeFetchContext->srtProvenance)
                if (item.flat_index == flat + i) {
                    r.root_sgpr = item.root_sgpr;
                    r.source_dword = item.source_dword;
                    r.source_kind = 1;
                    break;
                }
    }
    std::copy_n(prepared, count, output);
    return 0;
}
inline void ObserveImageSharp(uint32_t pc, uint32_t flat, uint32_t opcode, bool direct) {
    if (!provenanceObserver) return;
    ShaderProvenanceRecord record{pc, flat, opcode, UINT32_MAX, UINT32_MAX, 2};
    if (direct) {
        record.root_sgpr = flat;
        record.source_dword = 0;
        record.source_kind = 0;
    } else if (activeFetchContext) {
        for (const auto& item : activeFetchContext->srtProvenance)
            if (item.flat_index == flat) {
                record.root_sgpr = item.root_sgpr;
                record.source_dword = item.source_dword;
                record.source_kind = 1;
                break;
            }
    }
    struct CallbackScope {
        bool previous;
        CallbackScope() : previous(provenanceCallbackActive) { provenanceCallbackActive = true; }
        ~CallbackScope() { provenanceCallbackActive = previous; }
    } scope;
    provenanceObserver(provenanceObserverContext, &record);
}
}
