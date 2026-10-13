#include "ctr/core/root_metadata_property.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace ctr {
void apply_root_metadata_property(std::span<std::byte> metadata){
 if(metadata.size()!=152)throw std::invalid_argument("Bound root metadata152 required");
 std::uint16_t flags;std::memcpy(&flags,metadata.data()+92,2);
 if(flags&256)return;
 std::int32_t count;std::memcpy(&count,metadata.data()+24,4);
 if(count>0)throw std::invalid_argument("Field type hierarchy and virtual property callback missing");
 flags|=256;std::memcpy(metadata.data()+92,&flags,2);
 // Whole015187b0 returns at01518827 for signed nonpositive field count.
}
}
