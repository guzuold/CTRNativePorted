#include "ctr/derived_pool_init.h"
#include "ctr/pool_manager.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace ctr {
void construct_allocated_derived_pool(std::span<std::byte> pool,SystemPoolBackend& b,
 std::uint64_t& publication){
 const auto bytes=b.memory.bytes();const auto address=reinterpret_cast<std::uint64_t>(pool.data()),base=reinterpret_cast<std::uint64_t>(bytes.data());
 if(pool.size()!=192 || address<base || address-base>bytes.size() || pool.size()>bytes.size()-(address-base))throw std::invalid_argument("Actual192-byte derived storage required");
 const std::uint32_t references=1;const std::uint64_t zero=0,table=0x025baa70;
 std::memcpy(pool.data()+8,&references,4);std::memcpy(pool.data()+0x10,&zero,8);std::memcpy(pool.data()+0x88,&zero,8);
 initialize_unowned_pool_storage(pool);std::memcpy(pool.data(),&table,8);std::memcpy(pool.data()+0xa8,&zero,8);publication=address;
}
std::uint32_t initialize_derived_pool_backing(std::span<std::byte> pool,
 std::span<std::byte> parent,SystemPoolBackend& b,std::uint64_t capacity,std::uint32_t alignment,bool poison){
 if(pool.size()!=192 || !capacity || capacity>512 || (alignment && (alignment<8 || alignment>256 || (alignment&(alignment-1)))))throw std::invalid_argument("Initial bounded derived backing contract required");
 std::uint64_t table,backing;std::memcpy(&table,pool.data(),8);std::memcpy(&backing,pool.data()+0x20,8);
 if(table!=0x025baa70 || pool[0x18]!=std::byte{0} || backing)throw std::invalid_argument("Constructed derived type with absent backing required");
 const auto normalized=std::max(alignment,8u);std::memcpy(pool.data()+0x30,&normalized,4);
 // Concrete+e8 is original014ec5f0(true); +d0 original014d7dd0 returns8.
 const auto block=dispatch_null_reallocation_system_pool(parent,b,capacity,normalized,poison);
 const auto parent_address=reinterpret_cast<std::uint64_t>(parent.data());
 std::memcpy(pool.data()+0x20,&block.address,8);std::memcpy(pool.data()+0x28,&capacity,8);pool[0x49]=std::byte{1};
 std::memcpy(pool.data()+0x90,&parent_address,8);std::memcpy(pool.data()+0x98,&capacity,8);return 0;
}
}

