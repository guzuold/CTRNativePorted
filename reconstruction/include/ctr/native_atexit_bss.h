#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
namespace ctr {
class NativeAtexitBss final {
 void* page_{};
 NativeAtexitBss()=default;
public:
 ~NativeAtexitBss();
 NativeAtexitBss(const NativeAtexitBss&)=delete;
 NativeAtexitBss& operator=(const NativeAtexitBss&)=delete;
 static std::shared_ptr<NativeAtexitBss> load(const std::filesystem::path& original);
 std::span<std::byte> page() const;
 std::span<std::byte> translate(std::uint64_t logical,std::size_t extent) const;
 static constexpr std::size_t source_extent=0x1bc0,owner_extent=8192;
 static constexpr std::uint64_t logical_base=0x50126000;
};
}
