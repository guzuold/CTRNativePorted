#include "ctr/generated_metadata_names.h"
namespace ctr {
std::array<PooledNameInsertion,16> intern_generated_metadata_names(std::span<std::byte> object,
 const SystemPoolAllocation& table,std::span<std::byte> pool,SystemPoolBackend& backend,
 std::vector<PooledNameInsertion>& known,bool poison){
 std::array<PooledNameInsertion,16> result;
 for(std::size_t i=0;i<result.size();++i){auto name=intern_created_name(object,table,pool,backend,known,generated_metadata_names[i],poison);result[i]=name.name;if(name.created)known.push_back(name.name);}
 return result;
}
}
