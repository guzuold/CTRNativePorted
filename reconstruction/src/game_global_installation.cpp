#include "ctr/game_global_installation.h"
#include <cstring>
#include <stdexcept>
namespace ctr {namespace {
std::uint64_t q(std::span<const std::byte> b,std::size_t n){std::uint64_t v;std::memcpy(&v,b.data()+n,8);return v;}
void put(std::span<std::byte> b,std::size_t n,std::uint64_t v){std::memcpy(b.data()+n,&v,8);}
bool overlap(std::span<const std::byte> a,std::span<const std::byte> b){const auto x=reinterpret_cast<std::uintptr_t>(a.data()),y=reinterpret_cast<std::uintptr_t>(b.data());return x<y+b.size()&&y<x+a.size();}
}
GameParameterGraph::GameParameterGraph(std::span<const std::byte> bytes){
 if(bytes.size()!=data_.size())throw std::invalid_argument("Original graph extent456 required");
 std::memcpy(data_.data(),bytes.data(),bytes.size());
 // Resolve the declared original logical graph instead of dereferencing guest IDs.
 const auto p=this->bytes().first(88),l=this->bytes().subspan(88,144),d=this->bytes().subspan(232,112),h=this->bytes().subspan(344,112);
 if(q(p,0)!=88||q(p,0x38)!=0x02200000||q(l,0)!=144||q(l,8)!=0x10000000cULL||q(l,0x30)!=0x022000d8||q(l,0x38)!=0x02200148||q(l,0x60)!=0x022001b8||q(d,0)!=112||q(d,8)!=1||q(h,0)!=112||q(h,8)!=2)throw std::invalid_argument("Present original version12/mode1 graph required");
 for(std::size_t j=0x10;j<112;j+=8)if(!q(d,j))throw std::invalid_argument("Original nonzero callback identities required");
}
GameGlobalInstallation install_game_global_callbacks(std::shared_ptr<const GameParameterGraph> graph,std::span<std::byte> slots,std::span<std::byte> publication,std::uint64_t rsp,std::uint64_t rbp){
 if(!graph||slots.size()!=192||publication.size()!=8||rsp>UINT64_MAX-0x30||rbp!=rsp+0x30||overlap(slots,publication)||overlap(slots,graph->bytes())||overlap(publication,graph->bytes()))throw std::invalid_argument("Bounded frame/independent original slot extents required");
 const auto l=graph->bytes().subspan(88,144),d=graph->bytes().subspan(232,112);
 GameGlobalInstallation result{graph,{}};auto& r=result.boundary_registers;
 r={q(d,0x68),1,q(d,0x18),q(d,0x10),q(d,0x20),q(d,0x28),q(d,0x30),q(d,0x38),q(d,0x40),q(d,0x50),q(d,0x58),q(d,0x60),q(l,0x38),q(d,0x48),rsp,rbp};
 // Original58b3f/58b4a publishes libc+60, distinct from heapparam inR14.
 result.parameter_publication=q(l,0x60);put(publication,0,result.parameter_publication);
 const std::array<std::size_t,15> destinations{0x70,0x78,0x10,0x18,0x20,0x28,0x30,0x38,0x40,0x48,0xa0,0x50,0x58,0x60,0x68};
 const std::array<std::size_t,15> sources{0x20,0x28,0x10,0x18,0x20,0x28,0x30,0x38,0x40,0x50,0x50,0x48,0x58,0x60,0x68};
 for(std::size_t j=0;j<destinations.size();++j)put(slots,destinations[j],q(d,sources[j]));
 result.malloc_identity=q(d,0x20);result.free_identity=q(d,0x28);return result;
}
}
