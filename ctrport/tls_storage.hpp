// SPDX-License-Identifier: GPL-2.0-only
// New bounded storage; public pinned linker.cpp454-491 TLS copy/zero behavior.
#pragma once
#include "kernel_mutex.hpp"
#include <atomic>
namespace ctrport {
struct TlsTemplate {std::vector<unsigned char> initialized;std::size_t size=0,alignment=1;};
struct TlsBlock {
    unsigned char* data=nullptr;
    explicit TlsBlock(const TlsTemplate& value,bool primary=false) {
        data=static_cast<unsigned char*>(_aligned_malloc((std::max)(value.size+(primary?64:0),std::size_t(1)),(std::max)(value.alignment,sizeof(void*))));
        if(!data) throw std::bad_alloc();
        std::memset(data,0,value.size+(primary?64:0));
        if(primary) {void* self=data+value.size;std::memcpy(data+value.size,&self,sizeof(self));}
        if(!value.initialized.empty()) std::memcpy(data,value.initialized.data(),value.initialized.size());
    }
    ~TlsBlock(){_aligned_free(data);}
};
class TlsStorage {
    inline static std::atomic<std::uint64_t> next{1};
    const std::uint64_t id=next.fetch_add(1);
    const std::vector<TlsTemplate> modules;
    inline static thread_local std::unordered_map<std::uint64_t,std::unordered_map<std::uint64_t,std::unique_ptr<TlsBlock>>> blocks;
public:
    explicit TlsStorage(std::vector<TlsTemplate> values):modules(std::move(values)) {
        for(const auto& value:modules) if(value.initialized.size()>value.size || value.size>16*1024*1024 || !value.alignment || value.alignment>65536 || (value.alignment&(value.alignment-1))) throw std::runtime_error("TLS template size/alignment rejected");
    }
    std::size_t release_current_thread() const {
        const auto found=blocks.find(id);if(found==blocks.end())return 0;
        const auto count=found->second.size();blocks.erase(found);return count;
    }
    void* static_thread_pointer() const {
        if(modules.empty() || !modules[0].size) throw std::runtime_error("Missing primary static TLS image");
        return static_cast<unsigned char*>(address(1,0))+modules[0].size;
    }
    void* address(std::uint64_t module,std::uint64_t offset) const {
        if(!module || module>modules.size()) throw std::runtime_error("Unknown TLS module");
        const auto& value=modules[static_cast<std::size_t>(module-1)];
        if(offset>=value.size) throw std::runtime_error("TLS offset outside owned image");
        auto& block=blocks[id][module];if(!block) block=std::make_unique<TlsBlock>(value,module==1);
        return block->data+offset;
    }
};
inline thread_local const TlsStorage* current_tls_storage=nullptr;
// Guest "mov rax, fs:[0]" (static thread pointer) can be rewritten in place to
// "mov rax, gs:[0x1480+8*slot]", reading this Windows TLS slot of the TEB. Each guest
// context publishes its thread pointer here. Only slots 0..63 live inside the TEB.
inline const DWORD guest_tls_teb_slot=[]{const DWORD slot=TlsAlloc();return slot<64?slot:TLS_OUT_OF_INDEXES;}();
inline void publish_guest_tls(const TlsStorage* storage){
    if(guest_tls_teb_slot==TLS_OUT_OF_INDEXES)return;
    void* pointer=nullptr;if(storage)try{pointer=storage->static_thread_pointer();}catch(const std::exception&){pointer=nullptr;}
    TlsSetValue(guest_tls_teb_slot,pointer);
}
class TlsContextScope {
    const TlsStorage* previous;
public:
    explicit TlsContextScope(const TlsStorage& storage):previous(current_tls_storage) {current_tls_storage=&storage;publish_guest_tls(&storage);}
    ~TlsContextScope() {current_tls_storage=previous;publish_guest_tls(previous);}
    TlsContextScope(const TlsContextScope&)=delete;
    TlsContextScope& operator=(const TlsContextScope&)=delete;
};
// Construct before TlsContextScope so restoration precedes block release.
class TlsThreadLifetime {
    const TlsStorage& storage;
public:
    explicit TlsThreadLifetime(const TlsStorage& value):storage(value) {}
    ~TlsThreadLifetime() {storage.release_current_thread();}
    TlsThreadLifetime(const TlsThreadLifetime&)=delete;
    TlsThreadLifetime& operator=(const TlsThreadLifetime&)=delete;
};
}
