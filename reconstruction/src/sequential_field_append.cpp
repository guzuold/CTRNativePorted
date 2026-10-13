#include "ctr/sequential_field_append.h"
#include "ctr/system_pool_grow_reallocation.h"
#include "ctr/system_pool_reuse_reallocation.h"
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation append_sequential_metadata_field(std::span<std::byte> metadata,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& b,
 const SystemPoolAllocation& table,std::span<std::byte> field,std::uint64_t token,bool poison){
 if(metadata.size()!=152 || manager.size()!=112 || pool.size()!=176 || manager[0x6c]!=std::byte{0} ||
    (field.size()!=0 && field.size()!=32) || (token==0)!=field.empty())throw std::invalid_argument("Resolved single-thread metadata/manager/pool/field required");
 if(token){std::uint32_t refs;std::memcpy(&refs,field.data()+8,4);if(!refs||refs>0x7fffc)throw std::invalid_argument("Weak/lifetime services missing");}
 for(std::size_t at=0x18;at<0x30;at+=8){std::uint64_t p;std::memcpy(&p,manager.data()+at,8);if(p)throw std::invalid_argument("Alternate pool queries missing");}
 std::uint32_t count;std::uint64_t tag,address,selected;
 std::memcpy(&count,metadata.data()+0x18,4);std::memcpy(&tag,metadata.data()+0x20,8);std::memcpy(&address,metadata.data()+0x28,8);std::memcpy(&selected,manager.data()+0x10,8);
 if(count>16 || selected!=reinterpret_cast<std::uintptr_t>(pool.data()))throw std::invalid_argument("Observed sequence ends at17 fields and actual pool1 required");
 auto result=table;
 if(table.payload.empty()){
  if(count || address || tag!=0x0010000000000002ull)throw std::invalid_argument("Empty tagged pool1 array required");
  result=reserve_metadata_field_array(metadata,manager,pool,b,1,poison);
 }else{
  if(table.payload.size()%8 || tag!=(0x0010000000000000ull|table.payload.size()) || address!=table.address || count>table.payload.size()/8)
   throw std::invalid_argument("Actual tagged table storage required");
  if(count==table.payload.size()/8){
   const auto capacity=count+1<8?count+1:std::bit_ceil(count+1);
   const auto request=static_cast<std::uint64_t>(capacity)*8;
   const auto old_size=(table.payload.size()+63)&~31ull,new_size=(request+63)&~31ull;
   result=new_size==old_size?dispatch_reuse_reallocation_system_pool(pool,b,table,request,8,poison):dispatch_grow_reallocation_system_pool(pool,b,table,request,8,poison);
   const auto new_tag=0x0010000000000000ull|request;
   std::memcpy(metadata.data()+0x20,&new_tag,8);std::memcpy(metadata.data()+0x28,&result.address,8);
  }
 }
 append_reserved_metadata_field(metadata,result,field,token);
 return result;
}
}
