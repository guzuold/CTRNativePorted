#pragma once
#include "ctr/bitmap_align16_peak4_complete_free.h"
#include "ctr/bitmap_align16_peak4_gap_free.h"
#include <array>
#include <optional>
namespace ctr {
// Borrows actual object and SAMEcreator backend; caller retains both through all steps.
class BitmapAlign16SlackCompleteFree {
public:
 BitmapAlign16SlackCompleteFree(std::span<std::byte>,SystemPoolBackend&);
 BitmapAlign16SlackCompleteFree(const BitmapAlign16SlackCompleteFree&)=delete;
 BitmapAlign16SlackCompleteFree& operator=(const BitmapAlign16SlackCompleteFree&)=delete;
 std::uint32_t free(std::uint64_t);
private:
 std::span<std::byte> object_; SystemPoolBackend& backend_;
 std::array<std::uint64_t,4> identities_{};
 std::optional<BitmapAlign16Peak4CompleteFree> remaining_;
 unsigned step_{};
};
}
