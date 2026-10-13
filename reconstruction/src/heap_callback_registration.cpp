#include "ctr/heap_callback_registration.h"
#include <cstring>
#include <stdexcept>
namespace ctr {namespace {
std::uint64_t q(std::span<const std::byte> b,std::size_t n){std::uint64_t v{};std::memcpy(&v,b.data()+n,8);return v;}
bool overlap(std::span<const std::byte> a,std::span<const std::byte> b){auto x=reinterpret_cast<std::uintptr_t>(a.data()),y=reinterpret_cast<std::uintptr_t>(b.data());return x<y+b.size()&&y<x+a.size();}
}
HeapCallbackRegistration register_game_heap_callbacks(std::shared_ptr<const NativeProcParam> parameters,std::span<std::byte> slots,const std::array<std::uint64_t,16>& entry){
 if(!parameters||slots.size()!=96||entry[5]!=0x02200148||entry[14]>UINT64_MAX-8)throw std::invalid_argument("Genuine heapparam/logical frame/destination96 required");
 if(overlap(slots,parameters->page(0x02600000))||overlap(slots,parameters->page(0x02200000)))throw std::invalid_argument("Readonly source cannot alias destination");
 const auto heap=parameters->section(0x02200000).subspan(0x148,112);
 if(q(heap,0)!=112||q(heap,8)!=2)throw std::invalid_argument("Genuine size70/version2 heapparam required");
 std::array<std::uint64_t,8> callbacks{};
 for(std::size_t j=0;j<8;++j){callbacks[j]=q(heap,0x10+j*8);if(!callbacks[j])throw std::invalid_argument("Positive original callback registration required");}
 for(std::size_t n=0x50;n<0x70;n+=8)if(q(heap,n))throw std::invalid_argument("Only genuine zero optional callbacks covered");
 HeapCallbackRegistration result{std::move(parameters),entry};auto& r=result.return_registers;
 r[0]=0;r[2]=112;r[3]=callbacks[1];r[4]=callbacks[0];for(std::size_t j=0;j<4;++j)r[6+j]=callbacks[2+j];r[14]+=8;
 // Required eight stores a975e..a978f. Optional zero fields preserve existing
 // 121cc0..121cd8 exactly, including genuine original callback identities.
 for(std::size_t j=0;j<8;++j)std::memcpy(slots.data()+j*8,&callbacks[j],8);
 return result;
}
}
