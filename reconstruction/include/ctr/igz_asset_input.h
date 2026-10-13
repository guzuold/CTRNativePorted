#pragma once
#include "ctr/asset_support.h"
#include <map>
#include <string>
namespace ctr::assets {
struct IgzChunk {std::uint32_t identifier,offset,size,alignment;};
struct IgzFixup {std::uint32_t count,offset,size,header;};
enum class SerializedReferenceKind : std::uint32_t { named_handle, hashed_handle, named_object, hashed_object };
struct SerializedReference {
 std::string tag;
 SerializedReferenceKind kind;
 std::uint64_t file_offset;
 std::uint32_t raw,index,name_hash{},namespace_hash{};
 std::string name,name_space;
 // Describes bytes only. Runtime handle/object resolution is still missing.
};
struct SerializedGraphicsEntry {std::uint64_t object_offset;std::string type;};
struct SerializedShaderBinding {
 std::uint64_t list_offset,value_offset,constant_offset;
 std::string type,name;
 std::vector<std::byte> value_bytes;
};
struct SerializedMaterialGraph {
 std::uint64_t material_offset,graphics_set_offset;
 std::vector<SerializedGraphicsEntry> ordered_objects;
 std::vector<SerializedReference> ordered_texture_references;
};
// Read-only serialized views. No engine factory, relocation services or callable vtables.
class IgzAssetInput {
public:
 explicit IgzAssetInput(std::span<const std::byte> file);
 std::size_t global_offset(std::uint32_t encoded) const;
 std::span<const std::byte> memory(std::size_t object,std::size_t field) const;
 std::vector<std::byte> image_mip0() const;
 std::vector<Mesh> model_draw_calls() const;
 std::vector<SerializedReference> external_references() const;
 std::vector<SerializedReference> graphics_texture_references() const;
 std::vector<SerializedMaterialGraph> material_graphs() const;
 std::vector<SerializedShaderBinding> shader_bindings() const;
 std::size_t object_count()const{return objects_.size();}
 const std::vector<IgzChunk>&chunks()const{return chunks_;}
private:
 std::span<const std::byte> file_;
 std::vector<IgzChunk> chunks_;
 std::map<std::string,IgzFixup> fixups_;
 std::vector<std::string> types_;
 std::vector<std::pair<std::size_t,std::uint32_t>> objects_;
};
}
