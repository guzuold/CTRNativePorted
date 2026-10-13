#include "ctr/metaobject_parent_zero_fields.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T>T read(std::span<std::byte>s,std::size_t at){T v;std::memcpy(&v,s.data()+at,sizeof(v));return v;}
}
void link_metaobject_parent_zero_fields(std::span<std::byte> child,
 std::span<std::byte> parent,std::span<std::byte> head,
 std::uint64_t child_address,std::uint64_t parent_address,std::uint64_t head_address){
 if(parent_address){
  if(child.size()!=152||parent.size()!=152)throw std::invalid_argument("Metadata152 required");
  if(read<std::uint32_t>(child,0x18)||read<std::uint32_t>(parent,0x18)||read<std::uint64_t>(child,0x28))throw std::invalid_argument("Zero field counts and null child buffer required");
 }
 // These preconditions force actual0109e120 size0==capacity0, then RET;
 // parent count0 reaches01518941. No dependency callback is fabricated.
 (void)link_metaobject_parent_prefix(child,parent,head,child_address,parent_address,head_address);
}
}
