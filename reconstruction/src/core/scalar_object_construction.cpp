#include "ctr/core/scalar_object_construction.h"
#include "ctr/factory_header.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T> T load(std::span<const std::byte> b,std::size_t at) {
 T v; std::memcpy(&v,b.data()+at,sizeof(v)); return v;
}
}
void construct_scalar_field_object(std::span<std::byte> object,
 std::span<std::byte> metadata,std::span<const std::byte> descriptor,
 std::span<const std::byte> base_metadata,std::span<const std::byte> pool,
 std::span<const std::byte> table,std::uint64_t table_token,
 std::span<const ConstructionFieldBinding> fields) {
 if(object.size()!=128 || metadata.size()!=152 || descriptor.size()!=176 ||
    base_metadata.size()!=152 || (!pool.empty() && pool.size()!=176) || table.size()!=128)
  throw std::invalid_argument("Bound construction layout required");
 const auto count=load<std::int32_t>(metadata,24),base=load<std::int32_t>(base_metadata,24);
 if(base<0 || count<base || count>16 || fields.size()!=16 ||
    load<std::uint64_t>(metadata,40)!=table_token || !table_token)
  throw std::invalid_argument("Bound field table/count domain required");
 const auto dispatch=load<std::uint64_t>(metadata,56);
 if(load<std::int32_t>(descriptor,168)!=0 || (dispatch!=0x025ba3c0 && dispatch!=0x025ba2e8))
  throw std::invalid_argument("Object lifecycle/layout services missing");
 for(auto j=base;j<count;++j) {
  const auto& field=fields[static_cast<std::size_t>(j)];
  if(field.field.size()!=72 || field.backing.size()!=16 || !field.token ||
     load<std::uint64_t>(table,static_cast<std::size_t>(j)*8)!=field.token)
   throw std::invalid_argument("Field token/storage binding missing");
  const auto vt=load<std::uint64_t>(field.field,0);
  if(vt!=0x025bb470 && vt!=0x025c3090 && vt!=0x025c2918)
   throw std::invalid_argument("Object/vector/custom field services missing");
 }
 std::optional<std::uint32_t> index;
 if(!pool.empty()) index=load<std::uint32_t>(pool,28);
 initialize_factory_header(object,0,dispatch,index);
 // The conditional first walk has only actual014dc530 RET providers here.
 // The second walk has only actual014dd4b0 RET providers here.
 // Object +20 is actual014d76f0 RET. No unknown query is replaced by success.
 for(auto j=base;j<count;++j) {
  const auto& field=fields[static_cast<std::size_t>(j)];
  const auto vt=load<std::uint64_t>(field.field,0);
  // Scalar +268014ddb00 returns false; Raw +2680154bb60 loads field+64.
  if(vt==0x025c2918 && field.field[64]!=std::byte{0}) continue;
  reset_prepared_scalar_field(field.field,object,field.backing,field.backing_token);
 }
 // Object +50/+40 are actual014d7790/014d7730 RET providers.
 auto live=load<std::uint32_t>(metadata,48); ++live;
 std::memcpy(metadata.data()+48,&live,sizeof(live)); // single-thread fixture
}
}
