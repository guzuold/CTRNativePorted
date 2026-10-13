#include "ctr/game_heap_api_continuation.h"
#include <cstring>
#include <stdexcept>
namespace ctr {namespace {
std::uint64_t q(std::span<const std::byte> b,std::size_t n){std::uint64_t v;std::memcpy(&v,b.data()+n,8);return v;}
}
GameZeroHeapApiParameter::GameZeroHeapApiParameter(std::span<const std::byte> bytes){
 if(bytes.size()!=56)throw std::invalid_argument("Genuine parameter size38 required");
 std::memcpy(data_.data(),bytes.data(),56);
 if(q(data_,0)!=56||q(data_,8)!=1)throw std::invalid_argument("Original zero-API parameter version1 required");
 for(std::size_t n=0x10;n<56;n+=8)if(q(data_,n))throw std::invalid_argument("Nonzero heap-API callback path not reconstructed");
}
GameHeapApiContinuation prepare_game_heap_api_publication(const GameGlobalInstallation& installed,std::shared_ptr<const GameZeroHeapApiParameter> parameter,std::uint64_t module_base){
 if(!parameter||!installed.source||installed.parameter_publication!=0x022001b8||module_base>UINT64_MAX-0x125558)throw std::invalid_argument("Live genuine graph and module translation required");
 // Require the preceding proven boundary; no invented input callback registers.
 std::array<std::byte,192> slots{};std::array<std::byte,8> pub{};
 const auto expected=install_game_global_callbacks(installed.source,slots,pub,installed.boundary_registers[14],installed.boundary_registers[15]);
 if(expected.boundary_registers!=installed.boundary_registers||expected.malloc_identity!=installed.malloc_identity||expected.free_identity!=installed.free_identity)throw std::invalid_argument("Preceding original installation boundary required");
 GameHeapApiContinuation result{installed,parameter,installed.boundary_registers};auto& r=result.boundary_registers;
 r[0]=installed.parameter_publication; // RAX parameter loaded from original125550.
 r[2]=0; // RCX=parameter+18; all five callbacks genuinely zero.
 r[5]=module_base+0x1218d0; // RDI actual table argument at58ccf.
 r[13]=module_base+0x125550; // R15 publication cell, not heap parameter.
 return result;
}
}
