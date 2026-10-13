#include "ctr/guard_wait.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
GuardCallResult invoke_pending_guard_wait(NativeGuardSync& sync,std::span<std::byte> guard,const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack){
 const auto base=reinterpret_cast<std::uintptr_t>(sync.page().data()),p=reinterpret_cast<std::uintptr_t>(guard.data()),s=reinterpret_cast<std::uintptr_t>(stack.data());
 if(guard.size()!=8||p<base+0x200||p>base+0x278||(p-base)%8||stack.size()!=128||entry[14]!=0x2000fff8||entry[5]!=p||(s<base+4096&&s+128>base))throw std::invalid_argument("Owned pending guard/caller frame required");
 if(sync.lock(sync.mutex_cell()))throw std::runtime_error("Actual guard lock failed");
 auto read=[&](){std::uint64_t v{};std::memcpy(&v,guard.data(),8);return v;};
 if(read()!=0x100){sync.unlock(sync.mutex_cell());throw std::invalid_argument("Only actual pending100 route covered");}
 do {if(sync.wait(sync.condition_cell(),sync.mutex_cell(),0xffffffff)){sync.unlock(sync.mutex_cell());throw std::runtime_error("Actual condition wait failed");}}while((read()&0xff00)!=0);
 if(read()!=1){sync.unlock(sync.mutex_cell());throw std::invalid_argument("Only release-produced ready1 covered");}
 if(sync.unlock(sync.mutex_cell()))throw std::runtime_error("Actual guard unlock failed");
 auto save=[&](std::size_t o,std::uint64_t v){std::memcpy(stack.data()+o,&v,8);};save(112,entry[15]);save(104,entry[13]);save(96,entry[12]);save(88,entry[10]);save(80,entry[1]);save(72,0x5002ba24);
 GuardCallResult out{entry};out.registers[0]=0;out.registers[4]=reinterpret_cast<std::uint64_t>(sync.mutex_cell());out.registers[5]=reinterpret_cast<std::uint64_t>(sync.mutex_cell());out.registers[14]=0x20010000;return out;
}
}
