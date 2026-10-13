#pragma once
#include <cstddef>
#include <span>
namespace ctr {
// Full014d8710 callback-free return, resolved single-thread storage only.
void release_nonterminal_object_storage(std::span<std::byte> object);
}
