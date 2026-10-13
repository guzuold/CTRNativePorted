#include "ctr/data/igz_read_completion.h"
namespace ctr::data {
std::uint32_t igz_read_completion(std::optional<std::uint32_t> primary,
    std::span<const std::optional<std::uint32_t>> children, bool& ready) {
    constexpr std::uint32_t mask = 0x7c000000, completed = 0x08000000;
    std::uint32_t error = primary ? ((*primary & mask) > completed) : 1;
    ready = primary && ((*primary & mask) == completed);
    for (const auto child : children) {
        if (error || !ready) break;
        if (!child) ready = true;
        else {
            const auto status = *child & mask;
            ready = status == completed;
            if (status > completed) error = 1;
        }
    }
    return error;
}
}
