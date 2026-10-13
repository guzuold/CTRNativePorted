#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
namespace ctr {
// Selected original file sections only; guest callbacks remain identities.
class NativeProcParam final {
public:
 static std::shared_ptr<const NativeProcParam> load(const std::filesystem::path& self,const std::filesystem::path& flat);
 ~NativeProcParam();
 NativeProcParam(const NativeProcParam&)=delete;
 NativeProcParam& operator=(const NativeProcParam&)=delete;
 std::uint64_t query_logical_root() const noexcept {return 0x02600000;}
 std::span<const std::byte> section(std::uint64_t logical) const;
 std::span<const std::byte> page(std::uint64_t logical) const;
 std::uint64_t unresolved_symbol_cell() const noexcept {return 0x02200048;}
private:
 NativeProcParam()=default;
 void* root_{};void* libc_{};
};
}
