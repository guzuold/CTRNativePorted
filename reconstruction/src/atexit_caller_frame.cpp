#include "ctr/atexit_caller_frame.h"
#include "ctr/atexit_embedded_registration.h"
#include "ctr/typed_game_heap_callback.h"
#include <cstring>
#include <stdexcept>
namespace ctr {namespace {
template<class T>T get(std::span<const std::byte> bytes,std::size_t offset){T value{};std::memcpy(&value,bytes.data()+offset,sizeof(value));return value;}
template<class T>void put(std::span<std::byte> bytes,std::size_t offset,T value){std::memcpy(bytes.data()+offset,&value,sizeof(value));}
void need(bool ok){if(!ok)throw std::invalid_argument("Original NULL-DSO single-node caller-frame contract required");}
}
static FinalizeCallerFrame generate_call_impl(const NativeAtexitBss& registry,
 const FinalizeCallerFrame& entry,bool two_node,bool disabled_head=false,bool middle_node=false){
 const auto page=registry.page();const auto& e=entry.registers;
 need(e[14]==0x2000fff8&&e[5]==0&&get<std::uint64_t>(entry.stack,1016)==0x30000000);
 const std::size_t node=disabled_head?0xc70:two_node?0xca0:0xc70;const auto base=reinterpret_cast<std::uint64_t>(page.data());
 need(get<std::uint32_t>(page,0x1870)==0&&get<std::uint32_t>(page,0xc68)==(middle_node?3u:two_node?2u:1u));
 need(get<std::uint64_t>(page,0xc60)==base+(middle_node?0xcd0:two_node?0xca0:0xc70));
 need(get<std::uint64_t>(page,node)==(two_node&&!disabled_head?base+0xc70:0)&&get<std::uint64_t>(page,node+8)==(middle_node?base+0xcd0:disabled_head?base+0xca0:0));
 if(middle_node)need(get<std::uint32_t>(page,0xc80)==0&&get<std::uint32_t>(page,0xce0)==0&&get<std::uint64_t>(page,0xc70)==0&&get<std::uint64_t>(page,0xc78)==base+node&&get<std::uint64_t>(page,0xcd0)==base+node&&get<std::uint64_t>(page,0xcd8)==0);
 else if(disabled_head)need(get<std::uint32_t>(page,0xcb0)==0&&get<std::uint64_t>(page,0xca0)==base+node&&get<std::uint64_t>(page,0xca8)==0);
 else if(two_node)need(get<std::uint32_t>(page,0xc80)==0&&get<std::uint64_t>(page,0xc70)==0&&get<std::uint64_t>(page,0xc78)==base+node);
 need(get<std::uint32_t>(page,node+16)==2);
 need(get<std::uint64_t>(page,node+24)==original_game_heap_callback_id&&get<std::uint64_t>(page,node+32)==original_game_heap_argument_id&&get<std::uint64_t>(page,node+40)==original_game_dso_id);
 FinalizeCallerFrame out=entry;auto stack=std::span<std::byte>(out.stack);auto& r=out.registers;
 // ad9f0..ada14: preserve the six original caller-owned callee-saved words.
 put(stack,1008,e[15]);put(stack,1000,e[13]);put(stack,992,e[12]);
 put(stack,984,e[11]);put(stack,976,e[10]);put(stack,968,e[1]);
 put(stack,960,atexit_actual_cookie());put(stack,532,std::uint32_t{});
 // ada47..adad6, NULL filter and zero thread flag: first node kind2 call.
 r[0]=0x50127870;r[1]=NativeAtexitBss::logical_base+node;
 r[5]=original_game_heap_argument_id;r[10]=0;r[11]=original_game_heap_callback_id;
 r[12]=original_game_heap_argument_id;r[13]=2;
 r[14]=0x2000fe08;r[15]=0x2000fff0;put(stack,520,std::uint64_t{0x500adad9});
 // Last flags producer is CMP r15d,2. Preserve non-arithmetic bits; equal
 // yields ZF/PF set, CF/AF/SF/OF clear. No generic instruction emulation.
 out.flags=(entry.flags&~std::uint64_t{0x8d5})|0x44;
 return out;
}
FinalizeCallerFrame generate_single_finalize_call(const NativeAtexitBss& registry,const FinalizeCallerFrame& entry){return generate_call_impl(registry,entry,false);}
FinalizeCallerFrame generate_two_node_finalize_call(const NativeAtexitBss& registry,const FinalizeCallerFrame& entry){return generate_call_impl(registry,entry,true);}
FinalizeCallerFrame generate_disabled_head_finalize_call(const NativeAtexitBss& registry,const FinalizeCallerFrame& entry){return generate_call_impl(registry,entry,true,true);}
FinalizeCallerFrame generate_middle_node_finalize_call(const NativeAtexitBss& registry,const FinalizeCallerFrame& entry){return generate_call_impl(registry,entry,true,false,true);}
FinalizeCallerFrame complete_single_finalize_return(const FinalizeCallerFrame& entry,
 const FinalizeCallerFrame& generated,std::span<const std::byte> callback_output){
 constexpr std::size_t extent=3*16808+432;
 need(callback_output.size()==extent&&generated.registers[14]==0x2000fe08);
 const auto cookie=atexit_actual_cookie();need(get<std::uint64_t>(generated.stack,960)==cookie);
 const auto returned=callback_output.subspan(extent-432);
 need(get<std::uint64_t>(returned,0)==0&&get<std::uint64_t>(returned,424)==1);
 FinalizeCallerFrame out=generated;auto stack=std::span<std::byte>(out.stack);
 std::memcpy(stack.data()+272,returned.data()+40,256);
 std::memcpy(out.registers.data(),returned.data()+296,128);
 need(out.registers[14]==0x2000fe10&&out.registers[15]==0x2000fff0);
 // adbcf/2ebc0/2ebd0: actual helper services have already run in the
 // composed C++ finalizer. Model their observed logical caller save words.
 put(stack,520,std::uint64_t{0x500adbd4});put(stack,512,std::uint64_t{0x2000fff0});
 put(stack,504,std::uint64_t{0x5002ebc9});put(stack,496,std::uint64_t{0x2000fe00});
 put(stack,488,std::uint64_t{2});put(stack,480,std::uint64_t{});
 put(stack,472,std::uint64_t{original_game_heap_callback_id});put(stack,464,std::uint64_t{});
 put(stack,456,std::uint64_t{});put(stack,448,cookie);
 // The same helper call slot is overwritten by self, lock, then unlock;
 // the last original return word is retained after the three real calls.
 put(stack,8,std::uint64_t{0x5002ec19});
 need(get<std::uint64_t>(stack,448)==atexit_actual_cookie()&&get<std::uint64_t>(stack,960)==atexit_actual_cookie());
 auto& r=out.registers;r[0]=cookie;r[5]=0x50124268;
 // adbe4..adbf5 restores the caller's original six callee-saved words.
 r[1]=entry.registers[1];r[10]=entry.registers[10];r[11]=entry.registers[11];
 r[12]=entry.registers[12];r[13]=entry.registers[13];r[15]=entry.registers[15];r[14]=0x20010000;
 // Last flags producer ADD RSP,1b8: fixed fe10+1b8=ffc8 yields all
 // arithmetic flags clear. Preserve only original non-arithmetic flags.
 out.flags=entry.flags&~std::uint64_t{0x8d5};
 return out;
}
FinalizeCallerFrame complete_two_node_finalize_return(const FinalizeCallerFrame& entry,const FinalizeCallerFrame& generated,std::span<const std::byte> callback_output){
 auto out=complete_single_finalize_return(entry,generated,callback_output);
 // Final loop visit loads disabled kind0 into R15 before helper save words.
 put(std::span<std::byte>(out.stack),488,std::uint64_t{});return out;
}
FinalizeCallerFrame complete_disabled_head_finalize_return(const FinalizeCallerFrame& entry,const FinalizeCallerFrame& generated,std::span<const std::byte> callback_output){
 auto out=complete_single_finalize_return(entry,generated,callback_output);
 // adb1f/22 copies live.previous==0 through RCX to retained head.previous.
 out.registers[2]=0;return out;
}
FinalizeCallerFrame complete_middle_node_finalize_return(const FinalizeCallerFrame& entry,const FinalizeCallerFrame& generated,std::span<const std::byte> callback_output){
 auto out=complete_two_node_finalize_return(entry,generated,callback_output);
 // adb1f/22 passes the owned previous-node identity through logical RCX.
 out.registers[2]=NativeAtexitBss::logical_base+0xc70;return out;
}
}
