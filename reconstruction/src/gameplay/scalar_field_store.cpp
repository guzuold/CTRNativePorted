// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/scalar_field_store.h"
#include <cstring>
namespace ctr { void scalar_field_store(std::span<std::byte,324> object,float value){std::memcpy(object.data()+320,&value,4);} }
