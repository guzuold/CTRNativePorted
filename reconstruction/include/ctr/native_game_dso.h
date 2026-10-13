#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
namespace ctr {
class NativeGameDso final {
 void* page_{};
 NativeGameDso()=default;
public:
 ~NativeGameDso();
 NativeGameDso(const NativeGameDso&)=delete;
 NativeGameDso& operator=(const NativeGameDso&)=delete;
 static std::shared_ptr<NativeGameDso> load(const std::filesystem::path& original);
 std::span<std::byte> page() const;
 std::span<std::byte> translate(std::uint64_t logical,std::size_t extent) const;
 static constexpr std::size_t source_extent=8,owner_extent=4096;
 static constexpr std::uint64_t logical_base=0x02600090;
};
}
