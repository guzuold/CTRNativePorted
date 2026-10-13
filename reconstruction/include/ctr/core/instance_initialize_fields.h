// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
namespace ctr {
struct InstanceInitializationDefaults {
 const std::byte* metadata{};
 std::int32_t base_field_count{};
 std::uintptr_t type_query_target{},scalar_copy_target{},post_target{};
};
using InstanceTypeOriginal=std::function<std::uintptr_t(std::uintptr_t)>;
using InstanceFieldOriginal=std::function<void(std::uintptr_t,std::size_t,std::uintptr_t,std::uintptr_t)>;
using InstancePostOriginal=std::function<void(std::uintptr_t,std::size_t,std::uintptr_t)>;
void instance_initialize_fields_alternate(std::span<std::byte> destination,std::span<const std::byte> source,
 const InstanceInitializationDefaults& defaults,const InstanceTypeOriginal& type_query,
 const InstanceFieldOriginal& field_dispatch,const InstancePostOriginal& post_dispatch);
void instance_initialize_fields(std::span<std::byte> destination,std::span<const std::byte> source,
 const InstanceInitializationDefaults& defaults,const InstanceTypeOriginal& type_query,
 const InstanceFieldOriginal& field_dispatch,const InstancePostOriginal& post_dispatch);
}
