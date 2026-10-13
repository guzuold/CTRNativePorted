#include "ctr/populated_generated_field_table.h"
#include "ctr/populated_field_reserve.h"
#include "ctr/cold_scalar_field64.h"
#include "ctr/reserved_field_append.h"
#include "ctr/object_nonterminal_release.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
ColdFieldTable append_cold_generated_fields(std::span<std::byte> metadata,
 std::span<const std::byte> descriptor,std::span<const std::byte> manager,
 std::span<std::byte> pool,SystemPoolBackend& b,const SystemPoolAllocation& previous,bool poison){
 if(metadata.size()!=152 || descriptor.size()!=176 || descriptor[0]!=std::byte{0} || manager.size()!=112 || manager[0x6c]!=std::byte{0})
  throw std::invalid_argument("Prepared empty metadata/cold descriptor/nonthreaded manager required");
 // Pool selector1/nonthreaded route0153f243; no alternate owner query.
 ColdFieldTable result;std::uint32_t inherited;std::memcpy(&inherited,metadata.data()+24,4);
 if(!inherited||inherited>32)throw std::invalid_argument("Bounded populated prefix required");
 result.table=reserve_populated_field_array(metadata.subspan(24,24),manager,pool,b,previous,inherited+16,poison);
 // Original025cbf80 contains16 getters in this order. -1 selects igIntMetaField.
 constexpr std::array<int,16> kinds{-1,3,-1,2,1,1,1,0,0,0,0,0,0,3,1,1};
 for(std::size_t i=0;i<kinds.size();++i){
  auto field=kinds[i]<0?create_cold_scalar_field64(descriptor,manager,pool,b,poison):create_cold_typed_field(descriptor,manager,pool,b,static_cast<ColdFieldKind>(kinds[i]),poison);
  append_reserved_metadata_field(metadata,result.table,field.payload.first(32),field.address);
  release_nonterminal_object_storage(field.payload); //0153f1d8..1db owns one table ref.
  result.fields[i]=field;
 }
 return result;
}
}
