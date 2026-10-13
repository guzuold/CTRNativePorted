#include "ctr/atexit_embedded_registration.h"
#include <cstring>
#include <stdexcept>
extern "C" std::uintptr_t __security_cookie;
namespace ctr {namespace {
template<class T>T get(std::span<std::byte> b,std::size_t off){T v{};std::memcpy(&v,b.data()+off,sizeof(v));return v;}
template<class T>void put(std::span<std::byte> b,std::size_t off,T v){std::memcpy(b.data()+off,&v,sizeof(v));}
void need(bool ok){if(!ok)throw std::invalid_argument("Nonthreaded contiguous owned atexit records required");}
}
AtexitEmbeddedRegistration::AtexitEmbeddedRegistration(std::shared_ptr<NativeAtexitBss> p):owner_(std::move(p)){need(static_cast<bool>(owner_));}
std::span<std::byte> AtexitEmbeddedRegistration::page()const{return owner_->page();}
std::uint64_t atexit_actual_cookie(){return __security_cookie;}
std::uint64_t atexit_actual_cookie_cell(){return reinterpret_cast<std::uint64_t>(&__security_cookie);}
AtexitCallResult AtexitEmbeddedRegistration::invoke(const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack){
 auto b=page();const auto base=reinterpret_cast<std::uint64_t>(b.data()),s=reinterpret_cast<std::uint64_t>(stack.data());need(stack.size()==128&&entry[14]==0x2000fff8&&entry[5]!=0&&(s>=base+8192||(s<=base&&128<=base-s)));need(get<std::uint32_t>(b,0x1870)==0);const auto count=get<std::uint32_t>(b,0xc68);need(count<64);need(get<std::uint64_t>(b,0xc60)==(count?base+0xc70+(count-1)*48:0));
 for(std::uint32_t i=0;i<64;++i){const auto off=0xc70+static_cast<std::size_t>(i)*48;
  if(i<count){need(get<std::uint64_t>(b,off)==(i?base+off-48:0));need(get<std::uint64_t>(b,off+8)==(i+1<count?base+off+48:0));need(get<std::uint32_t>(b,off+16)==2&&get<std::uint64_t>(b,off+24)!=0);}
  else for(std::size_t j=0;j<48;++j)need(b[off+j]==std::byte{});
 }
 for(std::size_t i=NativeAtexitBss::source_extent;i<b.size();++i)need(b[i]==std::byte{0xa5});
 const auto cookie=atexit_actual_cookie();
 // Explicit logical caller frame. Actual physical register/call ABI is not claimed.
 put(stack,112,entry[15]);put(stack,104,entry[1]);put(stack,96,cookie);put(stack,64,std::uint32_t{2});put(stack,72,entry[5]);put(stack,80,entry[4]);put(stack,88,entry[3]);put(stack,56,std::uint64_t{0x500ad9d3});put(stack,48,std::uint64_t{0x2000fff0});put(stack,40,entry[13]);put(stack,32,entry[12]);put(stack,24,entry[10]);put(stack,16,atexit_actual_cookie_cell());
 const auto off=0xc70+static_cast<std::size_t>(count)*48;const auto previous=get<std::uint64_t>(b,0xc60);put(b,off,previous);put(b,off+8,std::uint64_t{});if(previous)put(b,0xc70+static_cast<std::size_t>(count-1)*48+8,base+off);put(b,0xc60,base+off);put(b,0xc68,count+1);std::memcpy(b.data()+off+16,stack.data()+64,32);
 if(get<std::uint64_t>(stack,96)!=atexit_actual_cookie())throw std::runtime_error("Actual native atexit cookie changed");
 AtexitCallResult result{entry};result.registers[0]=0;result.registers[2]=cookie;result.registers[5]=0x2000ffc0;result.registers[14]=0x20010000;return result;
}
}
