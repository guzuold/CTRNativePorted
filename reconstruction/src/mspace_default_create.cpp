#include "ctr/mspace_create.h"
#include <limits>
#include <stdexcept>
namespace ctr {
MspaceCreation create_default_named_mspace(std::span<std::byte> backing,std::uint64_t base,
    std::uint64_t capacity,std::string_view name,MspaceGlobalConfiguration& globals,const MspaceCreateContext& context) {
    if (!globals.page_scale) globals={1,1,1,1,1};
    // Original sanitizer query is a declared native build-configuration adapter.
    if ((base>>40) && native_address_sanitizer_enabled()) return {};
    if (base<0x400000 || capacity>0xfbffc00000ull) return {};
    if (base>std::numeric_limits<std::uint64_t>::max()-capacity)
        throw std::invalid_argument("Unsupported overflowing mspace address range");
    if (base+capacity>0xfbffffffffull) return {};
    const MspaceLayoutParameters p{base,capacity,false,globals.poison,globals.debug_flags,globals.page_scale,globals.granularity_scale};
    auto c=context;c.lock_policy=globals.lock_policy;
    return create_named_mspace_with_mutex(backing,p,name,c);
}
}
