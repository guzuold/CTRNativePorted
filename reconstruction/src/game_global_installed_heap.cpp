#include "ctr/game_global_installed_heap.h"
#include <cstring>
#include <stdexcept>
namespace ctr {namespace {
void validate(const InstalledGameGlobalHeap& b){
 if(!b.installation.source||!b.backend)throw std::invalid_argument("Live installed graph/backend required");
 std::array<std::byte,192> expected=b.slots;std::array<std::byte,8> publication{};
 const auto reference=install_game_global_callbacks(b.installation.source,expected,publication,0x2000ffc0,0x2000fff0);
 if(expected!=b.slots||reference.malloc_identity!=0x01437990||reference.free_identity!=0x014379b0||b.installation.malloc_identity!=reference.malloc_identity||b.installation.free_identity!=reference.free_identity||b.installation.parameter_publication!=reference.parameter_publication)throw std::invalid_argument("Original installed game callback graph required");
}
}
MspaceAllocation allocate_installed_game_global_top(const InstalledGameGlobalHeap& b,std::uint64_t request){validate(b);return allocate_game_global_top({b.backend,b.installation.malloc_identity,b.poison},request);}
std::uint32_t free_installed_game_global_top(const InstalledGameGlobalHeap& b,std::uint64_t pointer){validate(b);return free_game_global_top({b.backend,b.installation.free_identity,b.poison},pointer);}
}
