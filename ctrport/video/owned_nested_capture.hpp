// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../shader_owned_ranges.h"
#include "shader_inputs.h"
#include "pair_compile_timing.hpp"
#include <vector>
#include <utility>
namespace CtrPm4 {
// Pointer pairs are interpreted only when the compiler has identified a fetch
// descriptor root. No arbitrary user-data scan or host-pointer dereference.
inline int CaptureOwnedFetchDescriptor(const CtrShaderInputsV1* inputs,
    const CtrShaderOwnedRangesV1* ranges, const uint32_t* userdata,
    uint32_t declaredWords, uint32_t pointerIndex, uint32_t dwordOffset,
    uint32_t* output);
// Read only within an owner-established, same-thread mapping transaction. This
// is an immutable diagnostic copy, not guest-writer or GPU coherence.
inline int CaptureOwnedNestedRange(const CtrShaderInputsV1* inputs,
    const CtrShaderOwnedRangesV1* ranges, uint64_t address, uint64_t cap,
    uint64_t exactBytes, std::vector<uint32_t>& output) {
    ScopedNestedCaptureTimer pairTimer;
    if (!inputs || inputs->version!=1 || inputs->byte_size!=sizeof(*inputs) ||
        !inputs->context || !inputs->capture || !inputs->release ||
        !ranges || ranges->version!=1 || ranges->size!=sizeof(*ranges) ||
        !ranges->context || !ranges->begin || !ranges->end || !address ||
        (address&3) || cap<4 || cap>16*1024*1024 || (cap&3) ||
        exactBytes>cap || (exactBytes&3)) return 60;
    if (address>UINT64_MAX-cap) return 112;
    CtrShaderRangeLeaseV1 lease{};
    int status=ranges->begin(ranges->context,address,cap,&lease);
    if(status) return status;
    CtrShaderSnapshot snapshot{};
    bool captured=false;
    std::vector<uint32_t> prepared;
    try {
        if(!lease.token || lease.guest_address!=address || !lease.bytes ||
            lease.bytes>cap || address>UINT64_MAX-lease.bytes) status=88;
        const uint64_t bytes=exactBytes ? exactBytes : (lease.bytes&~uint64_t{3});
        if(!status && (!bytes || bytes>lease.bytes)) status=88;
        if(!status) {
            status=inputs->capture(inputs->context,address,bytes,&snapshot);
            captured=status==0;
            if(!status && (snapshot.guest_address!=address || snapshot.size!=bytes)) status=88;
            if(!status) {
                prepared.resize(static_cast<size_t>(bytes/4));
                if(ctr_shader_snapshot_read(&snapshot,address,prepared.data(),static_cast<size_t>(bytes))) status=112;
            }
        }
    } catch(...) { status=112; }
    // Snapshot storage must be released before ending the mapping transaction.
    if(captured) { const int released=inputs->release(inputs->context,&snapshot); if(released)status=released; }
    const int ended=ranges->end(ranges->context,&lease); if(ended)status=ended;
    if(status)return status;
    if(activePairCaptureTiming)activePairCaptureTiming->nestedInputs.push_back({address,uint64_t(prepared.size())*4,
        ExactInputSha256(prepared.data(),prepared.size()*4)});
    output=std::move(prepared);return 0;
}
inline int CaptureOwnedFetchDescriptor(const CtrShaderInputsV1* inputs,
    const CtrShaderOwnedRangesV1* ranges, const uint32_t* userdata,
    uint32_t declaredWords, uint32_t pointerIndex, uint32_t dwordOffset,
    uint32_t* output) {
    if(!userdata || !output || declaredWords>16)return 60;
    if(pointerIndex>=declaredWords || declaredWords-pointerIndex<2)return 88;
    uint64_t address=uint64_t(userdata[pointerIndex]) |
        (uint64_t(userdata[pointerIndex+1])<<32);
    address&=0xffffffffffffULL; // exact public ReadUdReg address convention
    const uint64_t offset=uint64_t(dwordOffset)*4;
    if(!address || (address&3))return 88;
    if(address>UINT64_MAX-offset || address+offset>UINT64_MAX-16)return 112;
    std::vector<uint32_t> captured;
    const int status=CaptureOwnedNestedRange(inputs,ranges,address+offset,16,16,captured);
    if(status)return status;
    memcpy(output,captured.data(),16);return 0;
}
}
