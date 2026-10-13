// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "session_api.h"
#include "../guest_copy_api.h"
#include <array>
#include <vector>
// Owner-thread only. Runtime must serialize guest/GPU mutation for the ENTIRE
// operation: per-read lifetime checks alone do not make a coherent frame.
// Value-only registration provenance: caller supplies live memory authority again
// for future capture. This record does not own guest mappings or imply GPU readiness.
struct CtrVideoLinearRegistration {
    CtrVideoBufferAttributesV1 attributes{};
    std::array<uint64_t,16> addresses{},tokens{};
    uint32_t first=0,count=0,group=0;
};
inline int ctr_video_capture_linear_group(const CtrVideoSessionApiV1& session,
    const CtrGuestCopyApi& memory,uint32_t first,uint32_t count,uint64_t attributesAddress,
    uint64_t addressesAddress,uint32_t& group,uint64_t* tokens,size_t capacity,CtrVideoLinearRegistration* registration=nullptr) {
    if(session.size!=sizeof(session) || session.version!=1 || !session.context || !session.register_linear_group ||
       memory.size!=sizeof(memory) || memory.version!=1 || !memory.context || !memory.read ||
       !count || first>=16 || count>16-first || !tokens || capacity<count) return 60;
    CtrVideoBufferAttributesV1 attributes{};std::array<uint64_t,16> addresses{};
    if(memory.read(memory.context,attributesAddress,&attributes,sizeof(attributes)) ||
       memory.read(memory.context,addressesAddress,addresses.data(),count*sizeof(uint64_t))) return 112;
    if(attributes.tiling!=1 || (attributes.format!=0x80000000U && attributes.format!=0x80002200U)) return 88;
    if(attributes.aspect || attributes.option || attributes.reserved0 || attributes.reserved1 ||
       !attributes.width || !attributes.height || attributes.width>16384 || attributes.height>16384 || attributes.pitch_pixels<attributes.width) return 89;
    const uint64_t pitch=uint64_t(attributes.pitch_pixels)*4;
    const uint64_t bytes=pitch*(attributes.height-1)+uint64_t(attributes.width)*4;
    // Explicit host capture budget, not a claim about PS4 accepted frame sizes.
    if(bytes>16*1024*1024 || bytes*count>64*1024*1024) return 117;
    std::array<std::vector<unsigned char>,16> captured;
    std::array<CtrVideoHostBufferV1,16> inputs{};
    try {
        for(uint32_t i=0;i<count;++i) {
            if(!addresses[i] || addresses[i]>UINT64_MAX-bytes) return 112;
            captured[i].resize(static_cast<size_t>(bytes));
            if(memory.read(memory.context,addresses[i],captured[i].data(),bytes)) return 112;
            inputs[i]={captured[i].data(),captured[i].size(),static_cast<size_t>(pitch)};
        }
        const int result=session.register_linear_group(session.context,first,count,&attributes,inputs.data(),&group,tokens,capacity);
        if(!result && registration) {
            CtrVideoLinearRegistration value{};
            value.attributes=attributes;value.addresses=addresses;value.first=first;value.count=count;value.group=group;
            for(uint32_t i=0;i<count;++i) value.tokens[i]=tokens[i];
            *registration=value;
        }
        return result;
    } catch(...) {return 90;}
}

// Refresh one registered source before enqueue. A failed read never publishes bytes.
// Same entire-operation runtime coherence/lifetime requirement as group capture.
inline int ctr_video_refresh_captured_linear(const CtrVideoSessionApiV1& session,
    const CtrGuestCopyApi& memory,const CtrVideoLinearRegistration& registration,uint32_t index,const uint64_t* submitArgument=nullptr) {
    if(session.size!=sizeof(session) || session.version!=1 || !session.context || (!submitArgument && !session.refresh_linear_frame) || (submitArgument && !session.submit_linear_frame) ||
       memory.size!=sizeof(memory) || memory.version!=1 || !memory.context || !memory.read ||
       !registration.count || registration.first>=16 || registration.count>16-registration.first || index>=registration.count ||
       !registration.tokens[index]) return 60;
    const auto& a=registration.attributes;
    if(a.tiling!=1 || (a.format!=0x80000000U && a.format!=0x80002200U)) return 88;
    if(a.aspect || a.option || a.reserved0 || a.reserved1 || !a.width || !a.height || a.width>16384 || a.height>16384 || a.pitch_pixels<a.width) return 89;
    const uint64_t bytes=uint64_t(a.pitch_pixels)*4*(a.height-1)+uint64_t(a.width)*4;
    if(bytes>16*1024*1024) return 117;
    const uint64_t address=registration.addresses[index];
    if(!address || address>UINT64_MAX-bytes) return 112;
    try {
        std::vector<unsigned char> captured(static_cast<size_t>(bytes));
        if(memory.read(memory.context,address,captured.data(),bytes)) return 112;
        const CtrVideoLinearFrameV1 input{sizeof(CtrVideoLinearFrameV1),a.format,a.width,a.height,a.pitch_pixels,captured.data(),captured.size()};
        return submitArgument ? session.submit_linear_frame(session.context,registration.first+index,registration.tokens[index],&input,*submitArgument)
            : session.refresh_linear_frame(session.context,registration.first+index,registration.tokens[index],&input);
    } catch(...) {return 90;}
}

inline int ctr_video_submit_captured_linear(const CtrVideoSessionApiV1& session,const CtrGuestCopyApi& memory,
    const CtrVideoLinearRegistration& registration,uint32_t index,uint64_t argument) {
    return ctr_video_refresh_captured_linear(session,memory,registration,index,&argument);
}

#include "../owner_dispatch_api.h"
struct CtrVideoLinearSubmitRequest {
    const CtrVideoSessionApiV1* session;const CtrGuestCopyApi* memory;
    const CtrVideoLinearRegistration* registration;uint32_t index;uint64_t argument;
};
inline int ctr_video_linear_submit_owner_action(void* raw) {
    const auto& request=*static_cast<const CtrVideoLinearSubmitRequest*>(raw);
    return ctr_video_submit_captured_linear(*request.session,*request.memory,*request.registration,request.index,request.argument);
}
// Caller holds no runtime locks. Request/provenance/authorities/code must remain
// live and registration immutable until synchronous invoke returns.
inline int ctr_video_dispatch_captured_submit(const CtrOwnerDispatchApi& owner,const CtrVideoSessionApiV1& session,
    const CtrGuestCopyApi& memory,const CtrVideoLinearRegistration& registration,uint32_t index,uint64_t argument) {
    if(owner.size!=sizeof(owner) || owner.version!=1 || !owner.context || !owner.invoke) return 60;
    CtrVideoLinearSubmitRequest request{&session,&memory,&registration,index,argument};int result=90;
    return owner.invoke(owner.context,ctr_video_linear_submit_owner_action,&request,&result) ? 115 : result;
}
