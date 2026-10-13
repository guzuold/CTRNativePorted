#include "ctr/descriptor_counts_prefix.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void update_bootstrap_descriptor_counts(std::span<std::byte> descriptor,
 const SystemPoolAllocation& registry_array,std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison,std::uint64_t& lazy_cell,
 SystemPoolAllocation& lazy_array){
 if(descriptor.size()!=176||registry_array.payload.size()!=24)
  throw std::invalid_argument("Descriptor176/registry array24 required");
 std::uint64_t registry_token;std::memcpy(&registry_token,descriptor.data()+0x20,8);
 if(registry_token!=registry_array.address)throw std::invalid_argument("Registry array token differs");
 if(lazy_cell){
  if(lazy_array.address!=lazy_cell||lazy_array.payload.size()!=24)
   throw std::invalid_argument("Retained lazy array24 required");
 }else{
  if(lazy_array.address||!lazy_array.payload.empty())throw std::invalid_argument("Cold lazy cell required");
  // Actual01538800 chooses prepared nonthreaded System Pool1, allocates24,
  // then015395d0/0109e120 reserve1024 pointer slots (8192 bytes).
  lazy_array=create_bootstrap_secondary_array(pool,backend,poison);
  lazy_cell=lazy_array.address;
 }
 std::memcpy(descriptor.data()+0xc,registry_array.payload.data(),4);
 std::memcpy(descriptor.data()+0x10,lazy_array.payload.data(),4);
 // Final storage only: original writes+0c before getter allocation.
 // Unknown kernel call01adbf70 remains excluded, not successfully stubbed.
}
}
