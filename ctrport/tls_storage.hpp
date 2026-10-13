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
inline int static_tls_control() {
    TlsStorage storage({{{},24,8}});
    auto* base=static_cast<unsigned char*>(storage.address(1,0));
    auto* tcb=static_cast<unsigned char*>(storage.static_thread_pointer());
    void* self=nullptr;std::memcpy(&self,tcb,8);
    if(tcb!=base+24 || self!=tcb) return 1;
    std::uint64_t value=123;std::memcpy(tcb-16,&value,8);
    std::uint64_t read=0;std::memcpy(&read,storage.address(1,8),8);if(read!=123)return 1;
    auto worker=std::async(std::launch::async,[&]{auto* own=static_cast<unsigned char*>(storage.static_thread_pointer());void* own_self=nullptr;std::memcpy(&own_self,own,8);std::uint64_t initial=1;std::memcpy(&initial,own-16,8);return own!=tcb && own_self==own && initial==0;});
    if(!worker.get())return 1;
    std::cout<<"{\"scope\":\"synthetic_static_tls\",\"self_pointer\":true,\"dynamic_static_alias\":true,\"thread_isolation\":true}\n";return 0;
}
inline int tls_context_scope_control() {
    TlsStorage outer({{{11},8,8}}),inner({{{22},8,8}});
    if(current_tls_storage) return 1;
    {
        TlsContextScope scope(outer);
        if(current_tls_storage!=&outer) return 1;
        try {TlsContextScope nested(inner);if(*static_cast<unsigned char*>(current_tls_storage->address(1,0))!=22)return 1;throw std::runtime_error("control unwind");} catch(const std::runtime_error&) {}
        if(current_tls_storage!=&outer) return 1;
        auto worker=std::async(std::launch::async,[&]{if(current_tls_storage)return false;{TlsContextScope own(inner);if(current_tls_storage!=&inner)return false;}return current_tls_storage==nullptr;});
        if(!worker.get() || current_tls_storage!=&outer) return 1;
    }
    if(current_tls_storage) return 1;
    std::cout<<"{\"scope\":\"synthetic_tls_context_lifetime\",\"nested_exception_restore\":true,\"worker_restore\":true,\"outer_restore\":true}\n";return 0;
}
inline int tls_release_control() {
    TlsStorage storage({{{9},16,8},{{},32,8}});
    *static_cast<unsigned char*>(storage.address(1,0))=88;
    storage.address(2,0);
    auto worker=std::async(std::launch::async,[&]{auto* own=static_cast<unsigned char*>(storage.address(1,0));if(*own!=9)return false;*own=77;return storage.release_current_thread()==1 && storage.release_current_thread()==0 && *static_cast<unsigned char*>(storage.address(1,0))==9;});
    if(!worker.get() || *static_cast<unsigned char*>(storage.address(1,0))!=88)return 1;
    if(storage.release_current_thread()!=2 || storage.release_current_thread()!=0)return 1;
    if(*static_cast<unsigned char*>(storage.address(1,0))!=9 || *static_cast<unsigned char*>(storage.address(2,31))!=0)return 1;
    if(storage.release_current_thread()!=2)return 1;
    std::cout<<"{\"scope\":\"synthetic_tls_release\",\"current_thread_only\":true,\"repeat_release_zero\":true,\"template_reinitialized\":true}\n";return 0;
}
inline int tls_storage_control() {
    TlsStorage storage({{{1,2,3,4},32,16},{{},0,1},{{},64,8}});
    auto* main=static_cast<unsigned char*>(storage.address(1,0));
    if(reinterpret_cast<std::uintptr_t>(main)%16 || main[0]!=1 || main[3]!=4) return 1;
    for(unsigned i=4;i<32;++i) if(main[i]) return 1;
    main[0]=99;
    auto worker=std::async(std::launch::async,[&]{auto* own=static_cast<unsigned char*>(storage.address(1,0));if(own==main || own[0]!=1 || own[4]) return false;own[0]=77;return storage.address(1,0)==own && *static_cast<unsigned char*>(storage.address(3,63))==0;});
    if(!worker.get() || main[0]!=99 || storage.address(1,0)!=main) return 1;
    unsigned rejected=0;
    for(auto test:std::vector<std::pair<unsigned,unsigned>>{{0,0},{4,0},{1,32},{2,0}}) try{storage.address(test.first,test.second);}catch(const std::runtime_error&){++rejected;}
    if(rejected!=4) return 1;
    std::cout<<"{\"scope\":\"tls_storage_host\",\"template_and_zero_fill\":true,\"two_thread_isolation\":true,\"alignment\":true,\"negative_cases\":4,\"guest_tls_bound\":false}\n";return 0;
}
}
