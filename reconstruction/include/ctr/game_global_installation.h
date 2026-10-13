#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
namespace ctr {
// Owned immutable copies of original relocated metadata. Logical callback IDs
// remain binary identities; they are never cast to host function pointers.
class GameParameterGraph final {
public:
 explicit GameParameterGraph(std::span<const std::byte> relocated_bytes);
 std::span<const std::byte> bytes() const noexcept {return data_;}
private:
 std::array<std::byte,456> data_{};
};
struct GameGlobalInstallation {
 std::shared_ptr<const GameParameterGraph> source;
 // RAX,RBX,RCX,RDX,RSI,RDI,R8..R15,RSP,RBP at bounded58c65.
 std::array<std::uint64_t,16> boundary_registers{};
 std::uint64_t malloc_identity{},free_identity{},parameter_publication{};
};
// Present/version12/mode1 original graph, prepared post-ProcParam frame.
// slots is exactly libc121860..121920; publication is the125550 qword.
GameGlobalInstallation install_game_global_callbacks(
 std::shared_ptr<const GameParameterGraph> graph,std::span<std::byte> slots,
 std::span<std::byte> publication,std::uint64_t rsp,std::uint64_t rbp);
}
