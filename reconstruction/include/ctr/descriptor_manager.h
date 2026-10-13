#pragma once
#include "ctr/descriptor_storage.h"
namespace ctr {
void mark_dispatch_descriptor_bootstrap_started(std::span<std::byte> descriptor);
void construct_allocated_descriptor_manager(std::span<std::byte> descriptor,std::span<std::byte> manager,std::uint64_t& publication);
}
