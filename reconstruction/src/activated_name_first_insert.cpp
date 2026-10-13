#include "ctr/activated_name_first_insert.h"
#include <atomic>
#include <cstring>
#include <stdexcept>
namespace ctr {
PooledNameInsertion insert_first_activated_name(std::span<std::byte> object,
 const SystemPoolAllocation& table,std::span<std::byte> pool,
 SystemPoolBackend& backend,std::string_view name,bool poison,std::int32_t spin_limit){
 if(object.size()!=64||table.payload.size()!=8192||pool.size()!=176||object[0x28]==std::byte{0}||spin_limit<=0||name.size()>128||name.find('\0')!=std::string_view::npos)
  throw std::invalid_argument("Active initial1024-bucket context/positive spin limit required");
 auto read=[&]<class T>(std::size_t at){T v;std::memcpy(&v,object.data()+at,sizeof(v));return v;};
 if(read.operator()<std::uint64_t>(0)!=0x025c4510||read.operator()<std::uint64_t>(0x10)!=table.address||read.operator()<std::uint32_t>(0x18)!=1024||read.operator()<std::uint32_t>(0xc)||read.operator()<std::uint32_t>(0x1c)||read.operator()<std::uint64_t>(0x38)!=reinterpret_cast<std::uintptr_t>(pool.data()))
  throw std::invalid_argument("Actual initialized empty context/table/pool required");
 for(std::byte b:table.payload)if(b!=std::byte{0})throw std::invalid_argument("Existing name entries missing");
 auto* lock_word=reinterpret_cast<std::uint32_t*>(object.data()+0x24);
 if(reinterpret_cast<std::uintptr_t>(lock_word)%std::atomic_ref<std::uint32_t>::required_alignment)
  throw std::invalid_argument("Aligned lock word required");
 std::atomic_ref<std::uint32_t> lock(*lock_word);std::uint32_t expected=0;
 if(!lock.compare_exchange_strong(expected,1))throw std::invalid_argument("Contended wait services missing");
 // This API covers a single caller with enough storage for the first entry.
 // Original successful return decrements the lock after count/threshold stores.
 try{
  std::uint32_t hash=0x811c9dc5;for(unsigned char c:name)hash=(hash^c)*0x1000193u;
  const auto slot=static_cast<std::size_t>(hash%1024)*8;
  const auto saved=pool[0xa0];pool[0xa0]=std::byte{0};std::uint32_t alignment;std::memcpy(&alignment,pool.data()+0x30,4);
  SystemPoolAllocation allocation;
  try{allocation=dispatch_new_system_pool_allocation(pool,backend,16+name.size()+1,alignment,poison);}
  catch(...){pool[0xa0]=saved;throw;}
  pool[0xa0]=saved;
  const std::uint64_t zero=0;const std::uint32_t one=1;
  std::memcpy(allocation.payload.data(),&zero,8);std::memcpy(allocation.payload.data()+8,&one,4);
  if(!name.empty())std::memcpy(allocation.payload.data()+16,name.data(),name.size());allocation.payload[16+name.size()]=std::byte{0};
  std::memcpy(table.payload.data()+slot,&allocation.address,8);std::memcpy(object.data()+0xc,&one,4);std::memcpy(object.data()+0x1c,&one,4);
  lock.fetch_sub(1);
  return {allocation,allocation.address+16};
 }catch(...){lock.fetch_sub(1);throw;}
}
}
