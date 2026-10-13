#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
namespace ctr {
class NativeGuardSync final {
 void* page_{};
public:
 NativeGuardSync();
 ~NativeGuardSync();
 NativeGuardSync(const NativeGuardSync&)=delete;
 std::span<std::byte> page() const;
 void* mutex_cell() const;
 void* condition_cell() const;
 int lock(void* cell) noexcept;
 int unlock(void* cell) noexcept;
 int wait(void* condition,void* mutex,std::uint32_t timeout_ms) noexcept;
 int broadcast(void* condition) noexcept;
 std::uint32_t waiters() const noexcept;
};
// Fresh acquire/release/ready routes only; pending-wait path is excluded here.
int acquire_guard_without_wait(NativeGuardSync&,std::span<std::byte> guard8);
void release_pending_guard(NativeGuardSync&,std::span<std::byte> guard8);
// Explicit logical caller stack/register model, not Windows or PS4 machine-register ABI.
struct GuardCallResult {std::array<std::uint64_t,16> registers;};
GuardCallResult invoke_guard_acquire_no_wait(NativeGuardSync&,std::span<std::byte> guard8,const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack128);
GuardCallResult invoke_guard_release_pending(NativeGuardSync&,std::span<std::byte> guard8,const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack128);
}
