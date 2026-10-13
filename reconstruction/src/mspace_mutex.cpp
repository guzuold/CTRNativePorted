#include "ctr/native_mutex.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::unique_ptr<NativeMutex> initialize_mspace_mutex(std::span<std::byte> backing,
    std::uint32_t header_offset,std::string_view name) {
    const std::size_t h=header_offset;
    if (h>backing.size() || backing.size()-h<0x380) throw std::invalid_argument("Missing mspace mutex slot");
    NativeMutexAttributes* attr=nullptr;NativeMutex* mutex=nullptr;
    if (mutexattr_init(&attr)) return {};
    const auto result=mutex_init(&mutex,&attr,name);
    if (result) {mutexattr_destroy(&attr);return {};}
    std::unique_ptr<NativeMutex> owned(mutex);
    const auto pointer=reinterpret_cast<std::uint64_t>(&mutex->prefix);
    std::memcpy(backing.data()+h+0x378,&pointer,8);
    mutex->prefix.flags|=0x100;
    if (mutexattr_destroy(&attr)) return {};
    return owned;
}
}
