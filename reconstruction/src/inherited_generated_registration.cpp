#include "ctr/inherited_generated_registration.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void register_inherited_generated_fields(ColdFieldTable& fields,std::span<std::byte> metadata,
 std::span<std::byte> names,const SystemPoolAllocation& buckets,std::span<std::byte> pool,
 SystemPoolBackend& backend,std::span<const std::byte> cached_types,
 std::span<const std::uint16_t,16> offsets,std::span<std::byte> publications,std::span<const InheritedFieldBinding> inherited,bool poison){
 if(metadata.size()!=152||cached_types.size()!=160||publications.size()!=128||inherited.empty()||inherited.size()>16||fields.table.payload.size()!=(16+inherited.size())*8)throw std::invalid_argument("Actual16field table and explicit caches required");
 std::uint32_t count;std::uint64_t table;std::memcpy(&count,metadata.data()+24,4);std::memcpy(&table,metadata.data()+40,8);
 if(count!=16+inherited.size()||table!=fields.table.address)throw std::invalid_argument("Inherited0/concrete field bindings required");
 constexpr std::array<std::uint64_t,5> identities{0x025bb470,0x025c3090,0x025bba10,0x025c2918,0x025c4220};
 constexpr std::array<std::uint16_t,5> widths{4,2,8,8,24};
 constexpr std::array<std::size_t,5> extents{64,64,80,72,112};
 constexpr std::array<std::size_t,16> kinds{0,4,0,3,2,2,2,1,1,1,1,1,1,4,2,2};
 for(std::size_t i=0;i<16;++i){auto bytes=fields.fields[i].payload;std::uint64_t identity,old,entry;std::memcpy(&entry,fields.table.payload.data()+8*(i+inherited.size()),8);
  if(bytes.size()!=extents[kinds[i]])throw std::invalid_argument("Unknown field extent");
  std::memcpy(&identity,bytes.data(),8);std::memcpy(&old,bytes.data()+40,8);
  if(identity!=identities[kinds[i]]||old||entry!=fields.fields[i].address||(std::to_integer<unsigned>(bytes[35])&1))throw std::invalid_argument("Known fresh field with registration enabled required");
 }
 for(std::size_t i=0;i<inherited.size();++i){std::uint64_t entry,identity;std::memcpy(&entry,fields.table.payload.data()+8*i,8);
  if(!inherited[i].token||entry!=inherited[i].token||inherited[i].storage.size()<64)throw std::invalid_argument("Explicit inherited object binding required");
  std::memcpy(&identity,inherited[i].storage.data(),8);bool found=false;
  for(std::size_t k=0;k<5;++k)if(identity==identities[k]&&inherited[i].storage.size()>=extents[k])found=true;
  if(!found)throw std::invalid_argument("Unknown inherited size getter missing");
 }
 std::vector<PooledNameInsertion> known;
 for(std::size_t i=0;i<16;++i){auto name=intern_created_name(names,buckets,pool,backend,known,generated_metadata_names[i],poison);
  if(!name.created)throw std::invalid_argument("Empty generated name context required");known.push_back(name.name);
  auto retained=intern_created_name(names,buckets,pool,backend,known,generated_metadata_names[i],poison);
  if(retained.created||retained.name.string_address!=name.name.string_address)throw std::runtime_error("Retained name binding differs");
  auto bytes=fields.fields[i].payload;std::memcpy(bytes.data()+40,&name.name.string_address,8);std::memcpy(bytes.data()+20,&offsets[i],2);std::memcpy(bytes.data()+12,metadata.data()+88,2);
  std::uint32_t refs;std::memcpy(&refs,name.name.record.payload.data()+8,4);--refs;std::memcpy(name.name.record.payload.data()+8,&refs,4);
  std::memcpy(bytes.data()+14,cached_types.data()+32*kinds[i]+16,2);std::memcpy(bytes.data()+18,&widths[kinds[i]],2);std::memcpy(publications.data()+8*i,&fields.fields[i].address,8);
 }
 const auto selected=select_inherited_highest_offset(inherited,static_cast<std::int32_t>(inherited.size()));std::uint16_t chosen_offset=0,chosen_width=0;
 for(const auto& field:inherited)if(field.token==selected){std::uint64_t identity;std::memcpy(&identity,field.storage.data(),8);std::memcpy(&chosen_offset,field.storage.data()+20,2);for(std::size_t k=0;k<5;++k)if(identity==identities[k])chosen_width=widths[k];break;}
 for(std::size_t i=0;i<16;++i)if(offsets[i]>chosen_offset){chosen_offset=offsets[i];chosen_width=widths[kinds[i]];}
 std::uint16_t previous;std::memcpy(&previous,metadata.data()+90,2);const auto end=static_cast<std::uint32_t>(chosen_offset)+chosen_width;
 const auto size=static_cast<std::uint16_t>(end<previous?previous:end);std::memcpy(metadata.data()+90,&size,2);
}
}
