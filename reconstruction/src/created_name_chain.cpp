#include "ctr/created_name_chain.h"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ctr {
namespace {
template<class T>T read(std::span<const std::byte> data,std::size_t offset){
 if(offset>data.size()||sizeof(T)>data.size()-offset)throw std::invalid_argument("Name record extent");
 T v;std::memcpy(&v,data.data()+offset,sizeof(v));return v;
}
template<class T>void write(std::span<std::byte> data,std::size_t offset,T value){std::memcpy(data.data()+offset,&value,sizeof(value));}
}
CreatedNameResult intern_created_name(std::span<std::byte> object,
 const SystemPoolAllocation& table,std::span<std::byte> pool,
 SystemPoolBackend& backend,std::span<const PooledNameInsertion> records,
 std::string_view text,bool poison){
 if(object.size()!=64||table.payload.size()!=8192||pool.size()!=176||text.size()>128||text.find('\0')!=std::string_view::npos||records.size()>128)
  throw std::invalid_argument("Bounded name context required");
 if(object[40]!=std::byte{0}||read<std::uint64_t>(object,0)!=0x025c4510||read<std::uint64_t>(object,16)!=table.address||read<std::uint32_t>(object,24)!=1024||read<std::uint64_t>(object,56)!=reinterpret_cast<std::uintptr_t>(pool.data())||read<std::uint32_t>(object,12)!=records.size()||read<std::uint32_t>(object,28)<records.size())
  throw std::invalid_argument("Concrete unlocked name pool required");
 if(records.empty())return {insert_first_created_name(object,table,pool,backend,text,poison),true};
 std::uint32_t hash=0x811c9dc5;for(unsigned char c:text)hash=(hash^c)*0x1000193u;
 const std::size_t slot=(hash%1024)*8;const auto head=read<std::uint64_t>(table.payload,slot);auto cursor=head;
 for(std::size_t visited=0;cursor;++visited){
  if(visited>=records.size())throw std::invalid_argument("Cyclic name chain");
  const PooledNameInsertion* record=nullptr;
  for(const auto& candidate:records)if(candidate.record.address==cursor){if(record)throw std::invalid_argument("Duplicate name binding");record=&candidate;}
  if(!record)throw std::invalid_argument("Unknown name chain binding");
  const auto bytes=record->record.payload;
  if(bytes.size()<17||bytes.size()>145||record->string_address!=cursor+16||bytes.back()!=std::byte{0})throw std::invalid_argument("Name payload binding");
  const auto chars=std::string_view(reinterpret_cast<const char*>(bytes.data()+16),bytes.size()-17);
  if(chars.find('\0')!=std::string_view::npos)throw std::invalid_argument("Embedded name terminator");
  if(chars==text){
   auto refs=read<std::uint32_t>(bytes,8);
   if(!refs||refs==std::numeric_limits<std::uint32_t>::max())throw std::invalid_argument("Name reference lifetime missing");
   write<std::uint32_t>(bytes,8,refs+1);return {*record,false};
  }
  cursor=read<std::uint64_t>(bytes,0);
 }
 if(records.size()==128)throw std::invalid_argument("Bounded name count exceeded");
 const auto saved=pool[0xa0];pool[0xa0]=std::byte{0};SystemPoolAllocation record;
 try{record=dispatch_new_system_pool_allocation(pool,backend,17+text.size(),read<std::uint32_t>(pool,48),poison);}catch(...){pool[0xa0]=saved;throw;}
 pool[0xa0]=saved;write<std::uint64_t>(record.payload,0,head);write<std::uint32_t>(record.payload,8,1);
 if(!text.empty())std::memcpy(record.payload.data()+16,text.data(),text.size());record.payload[16+text.size()]=std::byte{0};
 write<std::uint64_t>(table.payload,slot,record.address);const auto count=static_cast<std::uint32_t>(records.size()+1);write<std::uint32_t>(object,12,count);
 if(count>read<std::uint32_t>(object,28))write<std::uint32_t>(object,28,count);
 return {{record,record.address+16},true};
}
}
