#include "ctr/inline_field_default.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace ctr {
void initialize_fresh_scalar_field_default(std::span<std::byte> field,
 std::span<const std::byte> manager,std::span<const std::byte> pool,
 std::span<const std::byte> value,bool has_value){
 if(field.size()!=64||manager.size()!=112||pool.size()!=176||manager[108]!=std::byte{0})throw std::invalid_argument("Prepared nonthreaded default pool required");
 std::uint64_t flags,old,selected,table;std::uint32_t refs;
 std::memcpy(&flags,field.data()+48,8);std::memcpy(&old,field.data()+56,8);std::memcpy(&selected,manager.data()+24,8);std::memcpy(&refs,field.data()+8,4);std::memcpy(&table,field.data(),8);
 if(flags||old||(refs>>22)||selected!=reinterpret_cast<std::uintptr_t>(pool.data()))throw std::invalid_argument("Fresh default and actual slot0 pool binding required");
 if(table!=0x025bb470&&table!=0x025c3090)throw std::invalid_argument("Uncovered fresh scalar type");
 const auto width=table==0x025bb470?4u:2u;
 if(has_value&&value.size()!=width)throw std::invalid_argument("Fresh default value extent");
 // Original014e38c0 selects actual pool, but sizes<=4 use inline storage.
 // Pool index/alignment token is discarded by014e3aef, not an allocation.
 flags=std::uint64_t(1)<<56;std::memcpy(field.data()+48,&flags,8);
 set_inline_scalar_field_default(field,value,has_value);
}
void set_inline_scalar_field_default(std::span<std::byte> field,std::span<const std::byte> value,bool has_value){
 if(field.size()!=64)throw std::invalid_argument("Prepared scalar field64 required");
 std::uint64_t table,flags,old;std::memcpy(&table,field.data(),8);std::memcpy(&flags,field.data()+48,8);std::memcpy(&old,field.data()+56,8);
 std::uint16_t width;
 if(table==0x025bb470)width=4;else if(table==0x025c3090)width=2;else throw std::invalid_argument("Uncovered scalar default type");
 constexpr std::uint64_t inline_bit=std::uint64_t(1)<<56;
 if(!(flags&inline_bit))throw std::invalid_argument("Default backing allocator missing");
 if(has_value&&value.size()!=width)throw std::invalid_argument("Default value extent");
 // Original014e38c0 calls the real +110 size getter before scalar copying.
 std::memcpy(field.data()+18,&width,2);
 if(!old){flags=inline_bit;std::memcpy(field.data()+48,&flags,8);}
 if(has_value)std::memcpy(field.data()+56,value.data(),width);
}
}
