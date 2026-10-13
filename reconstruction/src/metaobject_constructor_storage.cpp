#include "ctr/metaobject_constructor_storage.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void initialize_metaobject_constructor_storage(std::span<std::byte> storage,
 std::optional<std::int32_t> selected_pool_index){
 if(storage.size()!=152)throw std::invalid_argument("Original152-byte metadata storage required");
 auto put=[&]<class T>(std::size_t offset,T value){std::memcpy(storage.data()+offset,&value,sizeof(value));};
 put(8,std::uint32_t{1});put(0x10,std::uint64_t{0});
 put(0,std::uint64_t{0x025ba2e8});
 put(0x18,std::uint32_t{0});
 put(0x20,std::uint64_t{0x0010000000000000});put(0x28,std::uint64_t{0});
 put(0x68,std::uint32_t{0});
 put(0x70,std::uint64_t{0x0010000000000000});put(0x78,std::uint64_t{0});
 put(0x90,std::uint32_t{0});
 std::memset(storage.data()+0x4a,0,16);std::memset(storage.data()+0x40,0,16);
 // Original XCHG is an initialization store on exclusively owned fresh storage.
 put(0x30,std::uint32_t{0});put(0x5c,std::uint16_t{0});put(0x80,std::uint64_t{0});
 put(0x5a,std::uint16_t{0xffff});put(0x5e,std::uint16_t{4});put(0x38,std::uint64_t{0});
 const auto token=selected_pool_index
  ? static_cast<std::uint64_t>(static_cast<std::int64_t>(*selected_pool_index)+1)&0x000fffffffffffffull : 0;
 // Both original0153d4c0 calls see empty containers; no free/reallocation branch.
 put(0x20,std::uint64_t{0x0010000000000000}|token);
 put(0x70,std::uint64_t{0x0010000000000000}|token);
}
}
