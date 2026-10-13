#pragma once
#include "ctr/bitmap_align16_peak5_complete_free.h"
#include "ctr/bitmap_align16_peak5_prefix_free.h"
#include "ctr/bitmap_align16_peak5_gap_free.h"
#include <array>
#include <optional>
namespace ctr {
// Borrows actual object and SAMEcreator backend; caller retains both through all steps.
class BitmapAlign16SuffixSlackCompleteFree {
public:
 BitmapAlign16SuffixSlackCompleteFree(std::span<std::byte>,SystemPoolBackend&);
 BitmapAlign16SuffixSlackCompleteFree(const BitmapAlign16SuffixSlackCompleteFree&)=delete;
 BitmapAlign16SuffixSlackCompleteFree& operator=(const BitmapAlign16SuffixSlackCompleteFree&)=delete;
 std::uint32_t free(std::uint64_t);
private:
 std::span<std::byte> object_;SystemPoolBackend& backend_;
 std::array<std::uint64_t,5> identities_{};
 std::optional<BitmapAlign16Peak5CompleteFree> remaining_;
 unsigned step_{};
};
}
