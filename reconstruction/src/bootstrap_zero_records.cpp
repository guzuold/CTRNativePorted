#include "ctr/bootstrap_zero_records.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation create_bootstrap_zero_record(std::span<std::byte> descriptor,
 std::span<std::byte> pool,SystemPoolBackend& backend,bool poison,unsigned index){
 if(descriptor.size()!=176 || index>2)
  throw std::invalid_argument("Complete descriptor and observed record index0..2 required");
 auto record=dispatch_null_reallocation_system_pool(pool,backend,16,8,poison);
 std::memset(record.payload.data(),0,16);
 std::memcpy(descriptor.data()+0x48+index*8,&record.address,8);
 return record;
}
}
