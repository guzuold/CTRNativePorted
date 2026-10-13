#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
namespace ctr {
struct NativeMutexAttributes {std::uint32_t type{1},protocol{},ceiling{};};
struct NativeMutexPrefix {
    std::atomic<std::uint64_t> owner{};
    std::int32_t count{},spinloops{},yieldloops{};
    std::uint32_t protocol{};
    std::uint64_t reserved{};
    std::uint32_t flags{1};
};
static_assert(offsetof(NativeMutexPrefix,flags)==0x20);
struct NativeMutexBackend;
class NativeMutex {
public:
    NativeMutexPrefix prefix;
    explicit NativeMutex(std::string_view name);
    ~NativeMutex();
    NativeMutex(const NativeMutex&)=delete;
    NativeMutex& operator=(const NativeMutex&)=delete;
    int lock() noexcept;
    int try_lock() noexcept;
    int unlock() noexcept;
private:
    std::unique_ptr<NativeMutexBackend> backend_;
};
int mutexattr_init(NativeMutexAttributes** attributes) noexcept;
int mutexattr_destroy(NativeMutexAttributes** attributes) noexcept;
int mutex_init(NativeMutex** mutex,NativeMutexAttributes* const* attributes,std::string_view name) noexcept;
int mutex_destroy(NativeMutex** mutex) noexcept;
bool native_address_sanitizer_enabled() noexcept;
std::uint64_t native_stack_cookie() noexcept;
// Original libc +60a73..+609bb using real native imports; lifetime returned explicitly.
std::unique_ptr<NativeMutex> initialize_mspace_mutex(std::span<std::byte> backing,
    std::uint32_t header_offset,std::string_view name);
}
