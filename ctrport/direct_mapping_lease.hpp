// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "direct_memory_copy.hpp"
#include <future>
namespace ctrport {
// Runtime mapping lifetime for a renderer registration. No raw backing escapes
// without its shared owner. Current runtime has no successful unmap/free service;
// future revocation MUST invalidate/wait these leases before altering mappings.
// This lease does not serialize original guest stores or GPU work and therefore
// is never a frame-ready/coherent-snapshot token.
class DirectMappingLease {
    std::shared_ptr<DirectMemoryBacking> owner;
    std::uint64_t address,bytes;
public:
    DirectMappingLease(std::shared_ptr<DirectMemoryBacking> backing,std::uint64_t start,std::uint64_t size)
        :owner(std::move(backing)),address(start),bytes(size) {
        if(!valid())throw std::runtime_error("Direct mapping lease rejected");
    }
    bool valid() const{return owner && owner->contains_mapped_range(address,bytes);}
    // Actual aligned CPU publication only. Caller must first wait its genuine GPU
    // completion; this operation does not establish or simulate GPU completion.
    int store_u64_release(std::uint64_t offset,std::uint64_t value) const noexcept {
        if(offset>=bytes || 8>bytes-offset || ((address+offset)&7))return 1;
        try {
            auto transaction=owner->copy_transaction();
            if(!owner->contains_mapped_range(address+offset,8))return 1;
            LONG64 signed_value=0;std::memcpy(&signed_value,&value,8);
            InterlockedExchange64(reinterpret_cast<volatile LONG64*>(address+offset),signed_value);
            return 0;
        }catch(...){return 1;}
    }

    // Authority remains bounded to this range even when the backing has others.
    int read(std::uint64_t offset,void* output,std::uint64_t size) const noexcept {
        if(!size || offset>=bytes || size>bytes-offset)return 1;
        return direct_copy_read(owner.get(),address+offset,output,size);
    }
};
}
