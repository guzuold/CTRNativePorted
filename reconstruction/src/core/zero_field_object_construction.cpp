#include "ctr/core/zero_field_object_construction.h"
#include "ctr/factory_header.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T> T load(std::span<const std::byte> bytes, std::size_t offset) {
    T result; std::memcpy(&result, bytes.data()+offset, sizeof(result)); return result;
}
}
void construct_zero_field_object(std::span<std::byte> object,
    std::span<std::byte> metadata, std::span<const std::byte> descriptor,
    std::span<const std::byte> base_metadata, std::span<const std::byte> pool) {
    if(object.size()!=96 || metadata.size()!=152 || descriptor.size()!=176 ||
       base_metadata.size()!=152 || (!pool.empty() && pool.size()!=176))
        throw std::invalid_argument("Explicit object/metadata/descriptor/pool bindings required");
    if(load<std::int32_t>(metadata,24)!=load<std::int32_t>(base_metadata,24))
        throw std::invalid_argument("Field construction/reset/default services missing");
    const auto offset=load<std::int32_t>(descriptor,168);
    const auto dispatch=load<std::uint64_t>(metadata,56);
    if(offset!=0 || (dispatch!=0x025ba3c0 && dispatch!=0x025ba2e8))
        throw std::invalid_argument("Object lifecycle hooks or alternate layout missing");
    std::optional<std::uint32_t> index;
    if(!pool.empty()) index=load<std::uint32_t>(pool,28);
    initialize_factory_header(object,offset,dispatch,index);
    // +20 014d76f0, +50 014d7790, +40 014d7730 are actual RET bodies.
    // Equal counts skip all four field walks, independently of descriptor flags.
    auto count=load<std::uint32_t>(metadata,48); ++count;
    std::memcpy(metadata.data()+48,&count,sizeof(count)); // single-thread fixture
}
}
