#include "ctr/fresh_vector_default.h"
#include "ctr/vector_descriptor_default.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation set_fresh_vector_descriptor_default(std::span<std::byte> field,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& backend,
 bool poison,std::uint64_t new_token,std::span<std::byte> new_object){
 if(field.size()!=112)throw std::invalid_argument("Fresh Vector descriptor112 required");
 std::uint64_t table,header,pointer;
 std::memcpy(&table,field.data(),8);std::memcpy(&header,field.data()+48,8);std::memcpy(&pointer,field.data()+56,8);
 if(table!=0x025c4220||header||pointer)throw std::invalid_argument("Known fresh Vector required; reuse missing");
 //014f6918..6924 bypass helper/allocation/width/object acquisition for fresh null.
 if(!new_token)return {};
 if(new_object.size()<12)throw std::invalid_argument("Explicit nonnull object header binding required");
 if(field[67]!=std::byte{0}){
  std::uint32_t flags;std::memcpy(&flags,new_object.data()+8,4);const auto refs=flags&0x7ffffu;
  if(!refs||refs==0x7ffffu)throw std::invalid_argument("Object lifetime/overflow unresolved");
 }
 auto backing=initialize_fresh_field_backing(field,manager,pool,backend,poison);
 // Fresh zero backing cannot release an old reference. Existing verified acquisition.
 set_prepared_vector_descriptor_default(field,backing.address,backing.payload,0,{},new_token,new_object);
 return backing;
}
}
