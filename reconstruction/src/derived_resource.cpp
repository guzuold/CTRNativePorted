#include "ctr/derived_resource.h"
#include "ctr/system_pool_large.h"
#include "ctr/bitmap_resource.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T> T get(std::span<std::byte> s,std::size_t off){T v;std::memcpy(&v,s.data()+off,sizeof(v));return v;}
template<class T> void put(std::span<std::byte> s,std::size_t off,T v){std::memcpy(s.data()+off,&v,sizeof(v));}
void validate_storage(std::span<std::byte> s,SystemPoolBackend& b){
 const auto base=reinterpret_cast<std::uintptr_t>(b.memory.bytes().data()),address=reinterpret_cast<std::uintptr_t>(s.data());
 if(s.size()!=192||address<base||address-base>b.memory.bytes().size()||s.size()>b.memory.bytes().size()-(address-base)||get<std::uint64_t>(s,0)!=0x025baa70)
  throw std::invalid_argument("Actual constructed derived192 storage required");
}
}
std::uint32_t initialize_derived_resource_backing(std::span<std::byte> s,
 std::span<std::byte> parent,SystemPoolBackend& b,std::uint64_t capacity,std::uint32_t alignment,bool poison){
 validate_storage(s,b);
 if(capacity!=8192||(alignment&&(alignment<8||alignment>256||(alignment&(alignment-1))))||s[0x18]!=std::byte{0}||get<std::uint64_t>(s,0x20))
  throw std::invalid_argument("Separate initial8192 derived backing contract required");
 const auto normalized=std::max(alignment,8u);put(s,0x30,normalized);
 const auto block=dispatch_system_pool_resource_backing(parent,b,capacity,normalized,poison);
 put(s,0x20,block.address);put(s,0x28,capacity);s[0x49]=std::byte{1};put(s,0x90,static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(parent.data())));put(s,0x98,capacity);return 0;
}
std::uint32_t activate_derived_bitmap_resource(std::span<std::byte> s,SystemPoolBackend& b,const SystemPoolBootstrapContext& c){
 validate_storage(s,b);if(s[0x18]!=std::byte{0})return 1;
 if(get<std::uint64_t>(s,0x10)||c.descriptor_initialized||c.threaded_manager||c.tls_key!=-1||get<std::uint32_t>(s,0x1c)!=1)
  throw std::invalid_argument("Derived resource early registry/owner/type contract missing");
 const auto pointer=get<std::uint64_t>(s,0x20),capacity=get<std::uint64_t>(s,0x28),base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(b.memory.bytes().data()));
 if(capacity!=8192||pointer<base||pointer-base>b.memory.bytes().size()||capacity>b.memory.bytes().size()-(pointer-base))
  throw std::invalid_argument("Actual8192 parent backing required");
 // Original014e1670 preflight: provider+d0 returns8, stats cleared,
 // absent owner/flag+39 sets+38, TLS-1 clears+40.
 put(s,0x30,std::max(get<std::uint32_t>(s,0x30),8u));std::memset(s.data()+0x50,0,0x30);
 if(s[0x39]!=std::byte{0})s[0x38]=std::byte{1};put(s,0x40,std::uint64_t{0});
 const auto header=create_bitmap_resource(b.memory.bytes().subspan(static_cast<std::size_t>(pointer-base),8192));put(s,0xa8,header);
 const auto used=get<std::uint64_t>(s,0x70)+0x1998;put(s,0x70,used);put(s,0x78,std::max(used,get<std::uint64_t>(s,0x78)));s[0x18]=std::byte{1};return 0;
}
std::uint32_t publish_derived_bitmap_resource(std::span<std::byte> s,SystemPoolBackend& b,const SystemPoolBootstrapContext& c){
 validate_storage(s,b);put(s,0x1c,std::uint32_t{1});return activate_derived_bitmap_resource(s,b,c);
}
}
