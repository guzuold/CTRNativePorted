#pragma once
#include "ctr/heap_callback_continuation.h"
namespace ctr {
class CustomHeapPreparation final {
public:
 explicit CustomHeapPreparation(std::span<const std::byte> page4096);
 ~CustomHeapPreparation();
 CustomHeapPreparation(const CustomHeapPreparation&)=delete;
 std::span<const std::byte> page() const;
 HeapCallbackRegistration prepare(const HeapCallbackRegistration&,std::span<std::byte> stack256,std::uint64_t cookie,const std::filesystem::path& original_libc);
private:
 void* page_{};
};
}
