// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "owned_nested_capture.hpp"
#include "srt_provenance_map.hpp"
#include "shader_recompiler/frontend/fetch_shader.h"
#include <stdexcept>
namespace CtrPm4 {
struct OwnedFetchContext {
    const CtrShaderInputsV1* inputs;
    const CtrShaderOwnedRangesV1* ranges;
    uint32_t declaredWords;
    uint64_t codeAddress=0;
    std::vector<uint32_t> code;
    std::vector<SrtProvenanceWord> srtProvenance;
    bool allowSrtLeaf=false; // experimental host controls until extended sink exists
};
inline thread_local OwnedFetchContext* activeFetchContext=nullptr;
class ScopedOwnedFetchContext {
    OwnedFetchContext* previous;
public:
    explicit ScopedOwnedFetchContext(OwnedFetchContext& context):previous(activeFetchContext) {
        if(previous)throw std::runtime_error("Nested owned fetch compiler context rejected");
        activeFetchContext=&context;
    }
    ~ScopedOwnedFetchContext(){activeFetchContext=previous;}
    ScopedOwnedFetchContext(const ScopedOwnedFetchContext&)=delete;
    ScopedOwnedFetchContext& operator=(const ScopedOwnedFetchContext&)=delete;
};
inline const std::vector<uint32_t>& CaptureFetchCode(const Shader::Info& info,uint32_t index) {
    auto* context=activeFetchContext;
    if(!context || context->declaredWords>16 || context->declaredWords>info.user_data.size() ||
        index>=context->declaredWords || context->declaredWords-index<2)
        throw std::runtime_error("Owned bounded fetch shader snapshot missing; raw userdata code pointer rejected");
    // Public fetch pointer uses the complete pair (descriptor roots mask48).
    const uint64_t address=uint64_t(info.user_data[index]) | (uint64_t(info.user_data[index+1])<<32);
    if(!context->code.empty()) {
        if(context->codeAddress!=address)throw std::runtime_error("Fetch snapshot identity changed");
        return context->code;
    }
    if(CaptureOwnedNestedRange(context->inputs,context->ranges,address,65536,0,context->code))
        throw std::runtime_error("Owned bounded fetch code capture missing");
    context->codeAddress=address;return context->code;
}
inline AmdGpu::Buffer CaptureFetchAttribute(const Shader::Info& info,
    const Shader::Gcn::VertexAttribute& attribute) {
    auto* context=activeFetchContext;
    if(!context || context->declaredWords>info.user_data.size())
        throw std::runtime_error("Owned fetch descriptor context missing");
    uint32_t words[4]{};
    if(CaptureOwnedFetchDescriptor(context->inputs,context->ranges,info.user_data.data(),
        context->declaredWords,attribute.sgpr_base,attribute.dword_offset,words))
        throw std::runtime_error("Owned fetch descriptor capture missing");
    AmdGpu::Buffer buffer{};static_assert(sizeof(buffer)==sizeof(words));
    memcpy(&buffer,words,sizeof(buffer));
    if(uint64_t(buffer.base_address)+attribute.inst_offset>0xffffffffffULL)
        throw std::runtime_error("Fetch descriptor instruction offset overflow");
    buffer.base_address+=attribute.inst_offset;
    if(attribute.data_format){buffer.data_format=attribute.data_format;buffer.num_format=attribute.num_format;}
    return buffer;
}
}
