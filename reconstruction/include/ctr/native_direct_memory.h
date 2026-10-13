#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Declared Windows backing adapter: one real reservation, CPU RW map only.
class NativeDirectMemory {
 public:
  explicit NativeDirectMemory(std::uint64_t budget=0x200000);
  ~NativeDirectMemory();
  NativeDirectMemory(const NativeDirectMemory&)=delete;
  NativeDirectMemory& operator=(const NativeDirectMemory&)=delete;
  std::uint64_t size() const {return budget_;}
  int allocate(std::int64_t start,std::int64_t end,std::uint64_t length,
      std::uint64_t alignment,int type,std::int64_t* physical);
  int map(void** address,std::uint64_t length,int protection,int flags,
      std::int64_t physical,std::uint64_t alignment);
  int release();
  std::span<std::byte> bytes() const;
 private:
  std::uint64_t budget_{},length_{};
  void* reservation_{};
  bool mapped_{};
};
}
