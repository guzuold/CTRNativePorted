#include "ctr/data/igz_dependency_publish.h"
#include <atomic>
#include "ctr/core/dependency_list_resize.h"
#include <stdexcept>
namespace ctr::data {
static_assert(sizeof(IgzPublishedName)==16);
static void assign(IgzPublishedName& destination,const IgzPublishedName& source) {
    if(&destination!=&source) {
        if(destination.name)throw std::runtime_error("missing original oldname release0154df00");
        destination.name=0;
        if(source.name) {
            auto* counter=reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(source.name)-8);
            std::atomic_ref<std::uint32_t>(*counter).fetch_add(1,std::memory_order_seq_cst);
        }
        destination.name=source.name;
    }
    destination.hash=source.hash;
}
void igz_dependency_publish(IgzDependencyPublisher& p,std::uint64_t key,
                            const IgzPublishedName& first,const IgzPublishedName& second) {
    for(std::size_t i=0;i<p.keys.size();++i) {
        if(p.keys[i]!=key)continue;
        if(!p.second_initialized&&second.hash) {
            if(p.second_header.empty()) throw std::runtime_error("missing original second resize header0156b490");
            ctr::dependency_list_resize(p.second_header,std::as_writable_bytes(p.second),static_cast<std::int32_t>(p.keys.size()));
            p.second_initialized=1;
        }
        if(!p.first_initialized) {
            if(first.hash) {
                if(p.first_header.empty()) throw std::runtime_error("missing original first resize header0156b490");
                ctr::dependency_list_resize(p.first_header,std::as_writable_bytes(p.first),static_cast<std::int32_t>(p.keys.size()));
                p.first_initialized=1;
                if(i>=p.first.size())throw std::runtime_error("first array extent");
                assign(p.first[i],first);
            }
        } else {
            if(i>=p.first.size())throw std::runtime_error("first array extent");
            assign(p.first[i],first);
        }
        if(p.second_initialized) {
            if(i>=p.second.size())throw std::runtime_error("second array extent");
            assign(p.second[i],second);
        }
        return;
    }
}
}
