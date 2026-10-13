#include "ctr/metaobject_parent_link.h"
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T>T read(std::span<std::byte> s,std::size_t at){T v;std::memcpy(&v,s.data()+at,sizeof(v));return v;}
template<class T>void write(std::span<std::byte> s,std::size_t at,T v){std::memcpy(s.data()+at,&v,sizeof(v));}
bool overlap(std::span<std::byte>a,std::span<std::byte>b){
 const auto x=reinterpret_cast<std::uintptr_t>(a.data());
 const auto y=reinterpret_cast<std::uintptr_t>(b.data());
 return x<=y ? y-x<a.size() : x-y<b.size();
}
}
MetaobjectParentLinkResult link_metaobject_parent_prefix(std::span<std::byte> child,
 std::span<std::byte> parent,std::span<std::byte> head,std::uint64_t child_address,
 std::uint64_t parent_address,std::uint64_t head_address){
 if(child.size()!=152||!child_address)throw std::invalid_argument("Resolved metadata child required");
 if(!parent_address){
  if(!parent.empty()||!head.empty()||head_address)throw std::invalid_argument("Null parent domain");
  return {0x01518941,0};
 }
 if(parent.size()!=152||child_address==parent_address||overlap(child,parent))throw std::invalid_argument("Separate parent required");
 if(read<std::uint64_t>(parent,0x48)!=head_address)throw std::invalid_argument("Parent head identity differs");
 if(head_address){
  if(head.size()!=152||head_address==child_address||head_address==parent_address||overlap(head,child)||overlap(head,parent))throw std::invalid_argument("Separate resolved previous head required");
 }else if(!head.empty())throw std::invalid_argument("Empty head domain");
 // Original empty-list path starts a circular sibling link at the child itself.
 if(head_address){write(child,0x50,read<std::uint64_t>(head,0x50));write(head,0x50,child_address);}
 else write(child,0x50,child_address);
 write(parent,0x48,child_address);write(child,0x40,parent_address);
 write(child,0x38,read<std::uint64_t>(parent,0x38));
 write(child,0x5e,read<std::uint16_t>(parent,0x5e));
 write(child,0x5a,read<std::uint16_t>(parent,0x5a));
 const auto capacity=read<std::uint64_t>(child,0x28) ? static_cast<std::uint32_t>(read<std::uint64_t>(child,0x20)>>3) : 0u;
 const auto sum=read<std::uint32_t>(child,0x18)+read<std::uint32_t>(parent,0x18);
 const auto request=std::bit_cast<std::int32_t>(capacity)>=std::bit_cast<std::int32_t>(sum) ? capacity : sum;
 return {0x015188c1,request}; // BEFORE original field-array reservation call.
}
}
