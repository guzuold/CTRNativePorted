#pragma once
#include "ctr/bitmap_split_three_free.h"
namespace ctr {
// Borrows the actual derived object and SAMEcreator backend; caller retains both.
class BitmapAlign16Peak4CompleteFree {
public:
 BitmapAlign16Peak4CompleteFree(std::span<std::byte>,SystemPoolBackend&);
 BitmapAlign16Peak4CompleteFree(const BitmapAlign16Peak4CompleteFree&)=delete;
 BitmapAlign16Peak4CompleteFree& operator=(const BitmapAlign16Peak4CompleteFree&)=delete;
 std::uint32_t free(std::uint64_t);
private:
 void validate() const;
 std::span<std::byte> object_;SystemPoolBackend& backend_;
 std::uint64_t resource_{},first_pointer_{},second_pointer_{},original_size_{};
 unsigned step_{};
};
}
