#include "ctr/mspace_destroy.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::array<std::uint64_t,16> destroy_borrowed_mspace(SystemPoolBackend& b,
 const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack){
 auto backing=b.memory.bytes();
 const auto base=reinterpret_cast<std::uint64_t>(backing.data());
 const auto header=b.heap.header_token;
 if(backing.size()!=0x4000||header<base||header-base>backing.size()||
    backing.size()-(header-base)<0x520||stack.size()!=128||entry[14]!=0x2000fff8||entry[5]!=header)
  throw std::invalid_argument("Missing borrowed heap/frame extent");
 const auto start=reinterpret_cast<std::uintptr_t>(stack.data());
 if(start<base+backing.size()&&base<start+stack.size())throw std::invalid_argument("Aliased destroy frame");
 auto* h=backing.data()+header-base;
 std::uint32_t flags,index;std::uint64_t cell;
 std::memcpy(&flags,h+0x370,4);std::memcpy(&index,h+0x3f4,4);std::memcpy(&cell,h+0x378,8);
 if(flags!=7||index||!b.heap.mutex||
    cell!=reinterpret_cast<std::uint64_t>(&b.heap.mutex->prefix)||b.heap.mutex->prefix.owner.load())
  throw std::invalid_argument("Unsupported ownership, registry or locked destroy path");
 // Release the creator's unique owner once. The same native provider destroys it.
 auto* actual=b.heap.mutex.release();
 const auto status=mutex_destroy(&actual);
 if(status){b.heap.mutex.reset(actual);throw std::runtime_error("Actual mutex destruction failed");}
 if(actual)throw std::runtime_error("Mutex destroy did not clear actual handle cell");
 const std::uint64_t zero=0;std::memcpy(h+0x378,&zero,8);
 auto put=[&](std::size_t offset,std::uint64_t value){std::memcpy(stack.data()+offset,&value,8);};
 put(112,entry[15]);put(104,0x50062b19);put(96,0x2000fff0);
 put(88,entry[13]);put(80,entry[12]);put(72,entry[11]);put(64,entry[10]);
 put(56,entry[1]);put(48,entry[0]);put(40,0x50060b2a);
 auto result=entry;result[0]=0;result[5]=header+0x378;result[14]=0x20010000;
 return result;
}
}
