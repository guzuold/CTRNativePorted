#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
namespace ctr::assets {
// Data tooling derived from pinned NST Maker helper behavior and CTR payloads.
// These are not recovered original engine functions or a complete IGZ loader.
struct Vertex { std::array<float,3> position,normal; std::array<float,2> uv; std::array<float,3> color; };
struct Mesh { std::vector<Vertex> vertices; std::vector<std::uint32_t> indices; };
Mesh decode_ctr_mesh(std::span<const std::byte> vertex_data,std::span<const std::byte> index_data,
    std::uint32_t vertex_count,std::uint32_t index_count,std::uint32_t stride,
    std::uint32_t index_width,std::array<float,3> translation);
// Serialized element data tooling; not original engine execution.
Mesh decode_ctr_described_mesh(std::span<const std::byte> vertex_data,std::span<const std::byte> index_data,
    std::span<const std::byte> elements,std::uint32_t vertex_count,std::uint32_t index_count,
    std::uint32_t stride,std::array<float,3> translation);
// Mip0 BC1/BC3/BC5: full aligned tiles or square 4/8 images with original 32x32 padding.
std::vector<std::byte> unswizzle_ps4_blocks(std::span<const std::byte> data,
    std::uint32_t width,std::uint32_t height,std::uint32_t bytes_per_block);
}
