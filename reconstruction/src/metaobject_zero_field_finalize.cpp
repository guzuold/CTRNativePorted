#include "ctr/metaobject_zero_field_finalize.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace ctr {
void finalize_zero_field_metaobject_storage(std::span<std::byte> metadata){
 if(metadata.size()!=152)throw std::invalid_argument("Metadata152 required");
 std::int32_t fields,secondary;std::memcpy(&fields,metadata.data()+0x18,4);std::memcpy(&secondary,metadata.data()+0x68,4);
 if(fields>0||secondary>0)throw std::invalid_argument("Field callbacks/index services missing");
 std::uint16_t first,second;std::memcpy(&first,metadata.data()+0x5e,2);std::memcpy(&second,metadata.data()+0x62,2);
 first=std::max<std::uint16_t>(first,4);second=std::max<std::uint16_t>(second,4);
 std::memcpy(metadata.data()+0x5e,&first,2);std::memcpy(metadata.data()+0x62,&second,2);
 metadata[0x5c]|=std::byte{4};
}
}
