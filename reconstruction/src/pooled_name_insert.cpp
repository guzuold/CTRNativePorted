#include "ctr/pooled_name_insert.h"
#include <cstring>
#include <stdexcept>
namespace ctr {namespace {
template<class T>T read(std::span<std::byte>s,std::size_t at){T v;std::memcpy(&v,s.data()+at,sizeof(v));return v;}
template<class T>void write(std::span<std::byte>s,std::size_t at,T v){std::memcpy(s.data()+at,&v,sizeof(v));}
}
PooledNameInsertion insert_fresh_pooled_name(std::span<std::byte> strings,
 std::span<std::byte> buckets,std::uint64_t buckets_address,std::span<std::byte> pool,
 SystemPoolBackend& backend,std::string_view name,bool poison){
 if(strings.size()!=64||buckets.size()!=128||pool.size()!=176||strings[0x28]!=std::byte{0}||name.size()>128||name.find('\0')!=std::string_view::npos)
  throw std::invalid_argument("Fresh nonthreaded bounded string pool required");
 if(read<std::uint64_t>(strings,0x10)!=buckets_address||read<std::uint32_t>(strings,0x18)!=16||read<std::uint32_t>(strings,0xc)!=0||read<std::uint32_t>(strings,0x1c)<1||read<std::uint64_t>(strings,0x38)!=reinterpret_cast<std::uintptr_t>(pool.data()))
  throw std::invalid_argument("Fresh table/pool identity differs");
 for(std::size_t i=0;i<128;i+=8)if(read<std::uint64_t>(buckets,i))throw std::invalid_argument("Existing entries missing");
 std::uint32_t hash=0x811c9dc5;
 for(unsigned char c:name)hash=(hash^c)*0x1000193u;
 const auto slot=static_cast<std::size_t>(hash%16)*8;
 auto saved=pool[0xa0];pool[0xa0]=std::byte{0};
 const auto alignment=read<std::uint32_t>(pool,0x30);
 auto allocation=dispatch_new_system_pool_allocation(pool,backend,16+name.size()+1,alignment,poison);
 pool[0xa0]=saved;
 write(allocation.payload,0,std::uint64_t{0});write(allocation.payload,8,std::uint32_t{1});
 if(!name.empty())std::memcpy(allocation.payload.data()+16,name.data(),name.size());
 allocation.payload[16+name.size()]=std::byte{0};
 write(buckets,slot,allocation.address);write(strings,0xc,std::uint32_t{1});
 return {allocation,allocation.address+16};
}
}
