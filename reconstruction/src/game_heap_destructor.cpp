#include "ctr/game_heap_destructor.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
int release_direct_memory_region(NativeDirectMemory& owner,std::int64_t physical,std::uint64_t length){
 if(physical!=0||length!=0x4000)return 22;
 try{if(owner.bytes().size()!=length)return 22;}catch(const std::invalid_argument&){return 22;}
 return owner.release();
}
static GameHeapDestructorResult destroy_registered_game_heap_impl(SystemPoolBackend& b,
 std::span<std::byte> manager,const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack,
 std::uint64_t expected_entry_rsp,bool finalizer_frame){
 const auto backing=b.memory.bytes();const auto base=reinterpret_cast<std::uint64_t>(backing.data());
 const auto header=b.heap.header_token;
 if(backing.size()!=0x4000||b.capacity!=0x4000||b.physical||b.original_manager_identity!=0x026ee0b8||manager.size()!=32||stack.size()!=256||
    entry[5]!=0x026ee0b8||entry[14]!=expected_entry_rsp||header!=base+0x20)
  throw std::invalid_argument("Unsupported exclusive destructor owner/frame");
 auto overlap=[](std::span<std::byte> a,std::span<std::byte> other){const auto x=reinterpret_cast<std::uintptr_t>(a.data()),y=reinterpret_cast<std::uintptr_t>(other.data());return x<y+other.size()&&y<x+a.size();};
 if(overlap(manager,stack)||overlap(manager,backing)||overlap(stack,backing))throw std::invalid_argument("Aliased destructor storage");
 auto get=[&](std::size_t offset){std::uint64_t value;std::memcpy(&value,manager.data()+offset,8);return value;};
 auto* h=backing.data()+0x20;std::uint32_t flags,index;std::uint64_t cell;
 std::memcpy(&flags,h+0x370,4);std::memcpy(&index,h+0x3f4,4);std::memcpy(&cell,h+0x378,8);
 if(get(0)!=0x025b9c90||get(0x10)!=0||get(0x18)!=header||flags!=7||index||!b.heap.mutex||
    b.heap.mutex->prefix.owner.load()||cell!=reinterpret_cast<std::uint64_t>(&b.heap.mutex->prefix))
  throw std::invalid_argument("Unsupported manager/heap/mutex lifetime");
 GameHeapDestructorResult result{};auto regs=entry;
 auto snapshot=[&](std::size_t phase){auto& s=result.before_release[phase];s.published_manager=b.original_manager_identity;std::memcpy(s.backing.data(),backing.data(),backing.size());std::memcpy(s.manager.data(),manager.data(),32);std::memcpy(s.stack.data(),stack.data(),256);s.registers=regs;};
 auto put=[&](std::size_t offset,std::uint64_t value){std::memcpy(stack.data()+offset,&value,8);};
 snapshot(0);put(240,entry[15]);put(232,entry[1]);put(224,entry[0]);put(216,0x0143792c);
 const std::uint64_t table=0x025b9c90;std::memcpy(manager.data(),&table,8);
 put(208,expected_entry_rsp-8);put(200,0x50062b19);put(192,expected_entry_rsp-0x28);
 put(184,entry[13]);put(176,entry[12]);put(168,entry[11]);put(160,entry[10]);
 put(152,0x026ee0b8);put(144,table);put(136,0x50060b2a);
 regs[0]=table;regs[1]=0x026ee0b8;regs[5]=header+0x378;regs[10]=0;regs[12]=header;regs[14]=expected_entry_rsp-0x70;regs[15]=expected_entry_rsp-0x38;
 snapshot(1);
 // Same creator/destructor ownership; transient actual handle cell adapter.
 auto* actual=b.heap.mutex.release();const auto status=mutex_destroy(&actual);
 if(status){b.heap.mutex.reset(actual);throw std::runtime_error("Actual destructor mutex service failed");}
 if(actual)throw std::runtime_error("Actual mutex handle cell not cleared");
 const std::uint64_t zero=0;std::memcpy(h+0x378,&zero,8);
 regs=entry;regs[0]=0;regs[1]=0x026ee0b8;regs[4]=0x4000;regs[5]=0;regs[14]=expected_entry_rsp-0x20;regs[15]=expected_entry_rsp-8;
 put(216,0x0143793c);snapshot(2);
 if(release_direct_memory_region(b.memory,b.physical,b.capacity))throw std::runtime_error("Actual direct-memory release failed");
 // No backing or mutex access after release. Only separate caller/module state remains.
 b.original_manager_identity=0;result.published_manager=0;
 regs=entry;regs[0]=0;regs[4]=0x4000;regs[5]=0;regs[14]=finalizer_frame?expected_entry_rsp+8:0x20010000;
 std::memcpy(result.manager.data(),manager.data(),32);std::memcpy(result.stack.data(),stack.data(),256);result.registers=regs;
 return result;
}
GameHeapDestructorResult destroy_registered_game_heap(SystemPoolBackend& b,
 std::span<std::byte> manager,const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack){
 return destroy_registered_game_heap_impl(b,manager,entry,stack,0x2000fff8,false);
}
GameHeapDestructorResult destroy_registered_game_heap_from_finalizer(SystemPoolBackend& b,
 std::span<std::byte> manager,const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack){
 std::uint64_t return_word=0;
 if(stack.size()==256)std::memcpy(&return_word,stack.data()+248,sizeof(return_word));
 if(stack.size()!=256||entry[14]!=0x2000fe08||entry[15]!=0x2000fff0||return_word!=0x500adad9)
  throw std::invalid_argument("Original finalizer CALL r13 entry frame required");
 return destroy_registered_game_heap_impl(b,manager,entry,stack,entry[14],true);
}
}
