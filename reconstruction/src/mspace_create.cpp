#include "ctr/mspace_create.h"
#include "ctr/mspace_chunks.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ctr {
namespace {
std::uint64_t create_tail(std::span<std::byte> backing,
    const MspaceLayoutParameters& p,std::string_view name,const MspaceCreateContext& c,std::unique_ptr<NativeMutex>* mutex) {
    if ((p.debug_flags && !mutex) || p.owns_backing)
        throw std::invalid_argument("Unsupported mspace mutex/debug or backing ownership");
    if (p.capacity<0x5a1 || ((p.capacity|p.base_token)&7)) return 0;
    if (p.capacity>backing.size() || p.base_token>std::numeric_limits<std::uint64_t>::max()-p.capacity ||
        (c.bitmap && c.table.size()<65)) throw std::invalid_argument("Missing mspace backing or registry");
    const auto header=initialize_mspace_layout(backing,p);
    auto count=std::min<std::size_t>(name.size(),31);
    const auto nul=name.find('\0');if (nul!=std::string_view::npos) count=std::min(count,nul);
    std::memset(backing.data()+header+0x3b0,0,32);
    std::memcpy(backing.data()+header+0x3b0,name.data(),count);
    if (c.parent_tracking) backing[header+0x3f0]|=std::byte{1};
    initialize_mspace_chunks(backing,{p.base_token,p.capacity,header,c.lock_policy,mutex!=nullptr});
    if (p.debug_flags) {
        *mutex=initialize_mspace_mutex(backing,header,name);
        if (!*mutex) return 0;
    }
    register_mspace(backing,p.base_token,header,header-16,c.bitmap,c.table);
    const auto signature=(p.base_token+header)^(p.poison?0x58585858ull:0ull);
    std::memcpy(backing.data()+header-16+0x520,&signature,8);
    return p.base_token+header;
}
}
std::uint64_t create_named_mspace(std::span<std::byte> backing,
    const MspaceLayoutParameters& p,std::string_view name,const MspaceCreateContext& c) {
    return create_tail(backing,p,name,c,nullptr);
}
MspaceCreation create_named_mspace_with_mutex(std::span<std::byte> backing,
    const MspaceLayoutParameters& p,std::string_view name,const MspaceCreateContext& c) {
    MspaceCreation result;result.header_token=create_tail(backing,p,name,c,&result.mutex);return result;
}
}
