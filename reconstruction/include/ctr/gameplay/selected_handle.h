#pragma once
#include "ctr/object_access.h"
#include <array>
namespace ctr {
// 00ee19b0. Adapted borrowed views; output overwrite does not release prior handle.
void select_retained_handle(HandleRecord*& output, unsigned char selector,
 const std::array<HandleRecord*,2>* record);
}
