#pragma once
#include "ctr/native_atexit_bss.h"
#include <array>
namespace ctr {
struct AtexitCallResult {std::array<std::uint64_t,16> registers;};
// Binary callback/arg/DSO identities are records only: no cast or invocation.
class AtexitEmbeddedRegistration final {
 std::shared_ptr<NativeAtexitBss> owner_;
public:
 explicit AtexitEmbeddedRegistration(std::shared_ptr<NativeAtexitBss>);
 std::span<std::byte> page() const;
 AtexitCallResult invoke(const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack128);
};
std::uint64_t atexit_actual_cookie();
std::uint64_t atexit_actual_cookie_cell();
}
