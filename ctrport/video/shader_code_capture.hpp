// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../guest_copy_api.h"
#include "shader_binary_metadata.hpp"
#include "shader_inputs.h"
#include <vector>
#include <utility>
namespace CtrPm4 {
struct OwnedShaderBinary {
    uint64_t guestAddress=0;
    ShaderBinaryMetadata metadata{};
    std::vector<uint32_t> words;
    const uint32_t* Code() const {return words.data();}
    size_t CodeWords() const {return metadata.codeBytes/4;}
};
// The caller supplies an observed bounded capture extent and retains/serializes
// its real memory authority for the entire call. This does not infer extent from
// VirtualQuery, borrow guest code, or assert GPU coherence. Never persist payload.
inline int CaptureShaderBinary(const CtrGuestCopyApi* memory,uint64_t address,
    size_t captureBytes,OwnedShaderBinary& output) {
    if(!memory || memory->size!=sizeof(*memory) || memory->version!=1 || !memory->context || !memory->read ||
       !address || (address&3) || captureBytes<8 || (captureBytes&3) || captureBytes>16*1024*1024)return 60;
    if(address>UINT64_MAX-captureBytes)return 112;
    OwnedShaderBinary prepared;prepared.guestAddress=address;prepared.words.resize(captureBytes/4);
    if(memory->read(memory->context,address,prepared.words.data(),captureBytes))return 112;
    const int status=FindShaderBinaryMetadata(prepared.words.data(),captureBytes,prepared.metadata);
    if(status)return status;
    output=std::move(prepared);return 0;
}
// Capture/release callback authority from the runtime. Every successful capture
// is released on success or validation failure; output is published only after
// successful release. Caller still serializes guest writers for coherent input.
inline int CaptureShaderBinaryInputs(const CtrShaderInputsV1* inputs,uint64_t address,
    size_t bytes,OwnedShaderBinary& output) {
    if(!inputs || inputs->version!=1 || inputs->byte_size!=sizeof(*inputs) ||
       !inputs->context || !inputs->capture || !inputs->release || !address ||
       (address&3) || bytes<8 || (bytes&3) || bytes>16*1024*1024)return 60;
    if(address>UINT64_MAX-bytes)return 112;
    CtrShaderSnapshot snapshot{};
    const int captured=inputs->capture(inputs->context,address,bytes,&snapshot);if(captured)return captured;
    OwnedShaderBinary prepared;prepared.guestAddress=address;int status=0;
    try {
        if(snapshot.guest_address!=address || snapshot.size!=bytes)status=88;
        else {
            prepared.words.resize(bytes/4);
            if(ctr_shader_snapshot_read(&snapshot,address,prepared.words.data(),bytes))status=112;
            else status=FindShaderBinaryMetadata(prepared.words.data(),bytes,prepared.metadata);
        }
    } catch(...) {status=112;}
    const int released=inputs->release(inputs->context,&snapshot);
    if(released)return released;
    if(status)return status;
    output=std::move(prepared);return 0;
}

}
