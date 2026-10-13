#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
namespace ctr {
class NativeEmbeddedLibcPage final {
 void* page_{};
 NativeEmbeddedLibcPage()=default;
public:
 ~NativeEmbeddedLibcPage();
 NativeEmbeddedLibcPage(const NativeEmbeddedLibcPage&)=delete;
 NativeEmbeddedLibcPage& operator=(const NativeEmbeddedLibcPage&)=delete;
 static std::shared_ptr<NativeEmbeddedLibcPage> load(const std::filesystem::path& original);
 std::span<std::byte> page() const;
 std::span<std::byte> translate(std::uint64_t logical,std::size_t extent) const;
 static constexpr std::uint64_t logical_base=0x50125000;
};
}
