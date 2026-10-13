#pragma once
#include "ctr/created_name_chain.h"
#include <array>
#include <vector>
namespace ctr {
// Actual original01bb6559/6567 argument arrays, recovered with ELF relocations.
inline constexpr std::array<std::string_view,16> generated_metadata_names={"_id","_metaFields","_instanceCount","_vTablePointer","_parent","_lastChild","_nextSibling","_index","_sizeofSize","_properties","_requiredAlignment","_dynamicFieldSize","_requiredDynamicFieldAlignment","_metaFunctions","_attributes","_library"};
inline constexpr std::array<std::uint16_t,16> generated_metadata_offsets={144,24,48,56,64,72,80,88,90,92,94,96,98,104,128,136};
std::array<PooledNameInsertion,16> intern_generated_metadata_names(std::span<std::byte> object,
 const SystemPoolAllocation& table,std::span<std::byte> pool,SystemPoolBackend& backend,
 std::vector<PooledNameInsertion>& known,bool poison);
}
