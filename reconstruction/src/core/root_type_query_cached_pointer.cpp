// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/root_type_query_cached_pointer.h"
namespace ctr {
// Retail3786c0 reads image publication cell26ee130 and returns its value.
// The adapter supplies that cell value explicitly; no pointed object is read.
std::uintptr_t root_type_query_cached_pointer(std::uintptr_t publication_value) noexcept {return publication_value;}
}
