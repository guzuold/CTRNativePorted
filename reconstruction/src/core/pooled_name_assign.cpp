#include "ctr/core/pooled_name_assign.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void assign_pooled_name(std::span<std::byte> destination,
                       std::span<const std::byte> source_cell,
                       bool same_cell, std::span<std::byte> record,
                       std::uint64_t string_token) {
 if(destination.size()<12 || source_cell.size()<8)
  throw std::invalid_argument("Pooled-name cell extent missing");
 std::uint64_t old_name, name;
 std::memcpy(&old_name,destination.data(),8);
 std::memcpy(&name,same_cell?destination.data():source_cell.data(),8);
 if(!same_cell && old_name)
  throw std::runtime_error("missing: whole0154df00 release of old pooled name");
 if(name) {
  if(name!=string_token || record.size()<17)
   throw std::invalid_argument("Prepared pooled-name record binding missing");
  bool terminated=false;
  for(std::size_t i=16;i<record.size();++i)
   if(record[i]==std::byte{0}) {terminated=true;break;}
  if(!terminated) throw std::invalid_argument("Pooled-name terminator missing");
 }
 if(!same_cell) {
  if(name) {
   std::uint32_t refs;
   std::memcpy(&refs,record.data()+8,4);
   ++refs; // Original LOCK INC wraps; concurrency is outside this domain.
   std::memcpy(record.data()+8,&refs,4);
  }
  std::memcpy(destination.data(),&name,8);
 }
 std::uint32_t hash=0;
 if(name) {
  hash=0x811c9dc5u;
  for(std::size_t i=16;record[i]!=std::byte{0};++i) {
   auto ch=std::to_integer<std::uint32_t>(record[i]);
   if(ch>=65u && ch<91u) ch+=32u;
   hash=(hash^ch)*0x1000193u;
  }
 }
 std::memcpy(destination.data()+8,&hash,4);
}
}
