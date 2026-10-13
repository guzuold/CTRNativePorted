#include "ctr/core/handle_initializer.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void initialize_handle(std::span<std::byte> destination,
 std::span<const std::byte> source, std::span<std::byte> old_record,
 std::uint64_t old_record_token) {
 if(destination.size()<8)throw std::invalid_argument("Handle destination missing");
 if(!source.empty()) {
  if(source.size()<12)throw std::invalid_argument("Handle source identifier missing");
  std::uint32_t identifier;std::memcpy(&identifier,source.data()+8,4);
  if(identifier&0x7ffffu)
   throw std::runtime_error("missing: original handle manager00332a20/014ecc10 and resolution014e7230/014f8e60");
 }
 std::uint64_t old;std::memcpy(&old,destination.data(),8);
 if(old) {
  if(old!=old_record_token || old_record.size()<4)
   throw std::invalid_argument("Old handle record binding missing");
  std::uint32_t before;std::memcpy(&before,old_record.data(),4);
  const std::uint32_t after=before-1u;
  std::memcpy(old_record.data(),&after,4); // Single-thread domain, original LOCK XADD.
  const auto terminal=(before&0x90000000u)?3u:2u;
  if((before&0xffffu)==terminal)
   throw std::runtime_error("missing: whole014da170 terminal handle release");
 }
 const std::uint64_t zero=0;std::memcpy(destination.data(),&zero,8);
}
}
