#pragma once
#include "ctr/bitmap_split_three_free.h"
namespace ctr {
// Borrows the actual derived object and SAMEcreator backend; caller retains both.
class BitmapAlign16GapCompleteFree {
public:
 BitmapAlign16GapCompleteFree(std::span<std::byte>,SystemPoolBackend&);
 BitmapAlign16GapCompleteFree(const BitmapAlign16GapCompleteFree&)=delete;
 BitmapAlign16GapCompleteFree& operator=(const BitmapAlign16GapCompleteFree&)=delete;
 std::uint32_t free(std::uint64_t);
private:
 void validate() const;
 std::span<std::byte> object_;SystemPoolBackend& backend_;
 std::uint64_t resource_{},first_pointer_{},second_pointer_{},original_size_{};
 unsigned step_{};
};
}
