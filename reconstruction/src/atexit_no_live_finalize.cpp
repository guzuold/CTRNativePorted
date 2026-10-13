#include "ctr/atexit_no_live_finalize.h"
#include "ctr/native_finalize_thread_services.h"
#include <cstring>
#include <stdexcept>
namespace ctr {namespace {
constexpr std::uint32_t none=64;
template<class T>T get(std::span<const std::byte>b,std::size_t off){T v{};std::memcpy(&v,b.data()+off,sizeof(v));return v;}
template<class T>void put(std::span<std::byte>b,std::size_t off,T v){std::memcpy(b.data()+off,&v,sizeof(v));}
void need(bool ok){if(!ok)throw std::invalid_argument("Verified no-live retained embedded chain required");}
std::size_t off(std::uint32_t i){return 0xc70+48*static_cast<std::size_t>(i);}
struct Services{NativeFinalizeThreadServices* owner{create_finalize_thread_services(0x50124268)};~Services(){if(owner)(void)release_finalize_thread_services(owner);}};
}
NoLiveFinalizeResult finalize_no_live(const std::shared_ptr<NativeAtexitBss>& owner,BoundedWalkLayout l,const FinalizeCallerFrame& entry,std::uint64_t dso_filter,std::uint64_t thread_exit_list){
 need(owner&&dso_filter==0&&thread_exit_list==0&&l.count>=1&&l.count<=64&&l.live<l.count);const auto retained=owner;auto b=owner->page();const auto base=reinterpret_cast<std::uint64_t>(b.data());const auto& e=entry.registers;
 need(e[14]==0x2000fff8&&e[5]==0&&get<std::uint64_t>(entry.stack,1016)==0x30000000&&get<std::uint32_t>(b,0xc68)==l.count-1&&get<std::uint32_t>(b,0x1870)==0);
 // All actual owned links are validated before following a pointer or creating
 // services. Disabled callback identities remain numeric and are never invoked.
 const auto serialized=serialize_reused_head_removed(*owner,l);
 for(std::uint32_t i=0;i<l.count;++i){const auto start=off(i);need(get<std::uint32_t>(b,start+16)==0);
  if(i==l.live){for(std::size_t j=16;j<48;++j)need(b[start+j]==std::byte{});}
  else need(get<std::uint64_t>(b,start+24)==original_game_heap_callback_id&&get<std::uint64_t>(b,start+32)==original_game_heap_argument_id&&get<std::uint64_t>(b,start+40)==original_game_dso_id);
 }
 for(std::size_t i=off(l.count);i<off(64);++i)need(b[i]==std::byte{});
 for(std::size_t i=NativeAtexitBss::source_extent;i<b.size();++i)need(b[i]==std::byte{0xa5});
 std::array<std::byte,8192> initial{};std::memcpy(initial.data(),b.data(),8192);NoLiveFinalizeResult result{entry,entry,serialized,{},0};result.order.reserve(l.count-1);
 for(auto pointer=get<std::uint64_t>(b,0xc60);pointer;){need(pointer>=base+off(0)&&pointer<base+off(l.count)&&(pointer-base-off(0))%48==0);const auto i=static_cast<std::uint32_t>((pointer-base-off(0))/48);need(i!=l.live&&result.order.size()<l.count-1&&get<std::uint32_t>(b,off(i)+16)==0);result.order.push_back(i);pointer=get<std::uint64_t>(b,off(i));}
 need(result.order.size()==l.count-1);Services services;need(services.owner!=nullptr);const auto cookie=atexit_actual_cookie();
 if(finalize_thread_self(services.owner,&result.actual_thread)!=0||!result.actual_thread)throw std::runtime_error("Actual no-live self failed");
 if(finalize_mutex_lock(services.owner,0x50124268)!=0||finalize_mutex_unlock(services.owner,0x50124268)!=0)throw std::runtime_error("Actual no-live cleanup mutex failed");
 if(cookie!=atexit_actual_cookie()||release_finalize_thread_services(services.owner)!=0)throw std::runtime_error("Actual no-live cookie/release failed");services.owner=nullptr;
 if(std::memcmp(initial.data(),b.data(),8192))throw std::runtime_error("No-live registry changed");
 auto& out=result.frame;auto s=std::span<std::byte>(out.stack);
 put(s,1008,e[15]);put(s,1000,e[13]);put(s,992,e[12]);put(s,984,e[11]);put(s,976,e[10]);put(s,968,e[1]);put(s,960,cookie);put(s,532,std::uint32_t{});
 put(s,520,std::uint64_t{0x500adbd4});result.helper_entry=out;auto& helper=result.helper_entry;
 helper.registers[0]=0x50127870;helper.registers[1]=0;helper.registers[5]=get<std::uint32_t>(entry.stack,800);helper.registers[10]=0;helper.registers[12]=result.order.empty()?e[12]:0;helper.registers[13]=result.order.empty()?e[13]:0;helper.registers[14]=0x2000fe08;helper.registers[15]=0x2000fff0;helper.flags=(entry.flags&~std::uint64_t{0x8d5})|0x44;
 put(s,520,std::uint64_t{0x500adbd4});put(s,512,std::uint64_t{0x2000fff0});put(s,504,std::uint64_t{0x5002ebc9});put(s,496,std::uint64_t{0x2000fe00});
 put(s,488,result.order.empty()?e[13]:std::uint64_t{});put(s,480,result.order.empty()?e[12]:std::uint64_t{});put(s,472,e[11]);put(s,464,std::uint64_t{});put(s,456,std::uint64_t{});put(s,448,cookie);put(s,8,std::uint64_t{0x5002ec19});
 out.registers[0]=cookie;out.registers[5]=0x50124268;out.registers[14]=0x20010000;out.flags=entry.flags&~std::uint64_t{0x8d5};return result;
}
}
