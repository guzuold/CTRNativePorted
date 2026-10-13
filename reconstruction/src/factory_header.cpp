#include "ctr/factory_header.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
std::size_t checked_offset(std::size_t size, std::int32_t offset) {
    if (offset < 0 || size < 8 || static_cast<std::size_t>(offset)>size-8)
        throw std::invalid_argument("Dispatch metadata outside supplied storage");
    return static_cast<std::size_t>(offset);
}
}
std::uint64_t capture_model_dispatch(std::span<const std::byte> model,
    std::int32_t dispatch_offset) {
    const auto offset=checked_offset(model.size(),dispatch_offset);
    std::uint64_t result;
    std::memcpy(&result,model.data()+offset,sizeof(result));
    return result;
}
void initialize_factory_header(std::span<std::byte> storage,
    std::int32_t dispatch_offset, std::uint64_t dispatch,
    std::optional<std::uint32_t> pool_index) {
    const auto offset=checked_offset(storage.size(),dispatch_offset);
    if (storage.size()<12) throw std::invalid_argument("Object counter outside supplied storage");
    std::memcpy(storage.data()+offset,&dispatch,sizeof(dispatch));
    // Preserve write ordering even for synthetic overlapping metadata offsets.
    const std::uint32_t ownership=pool_index ? ((*pool_index<<22)|1u) : 1u;
    std::memcpy(storage.data()+8,&ownership,sizeof(ownership));
}
}
