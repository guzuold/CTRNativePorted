// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/allocation_context_default.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uintptr_t allocation_context_default(std::span<const std::byte> m,std::size_t manager,std::int32_t tls_key){
 if(tls_key!=-1)throw std::runtime_error("Missing original TLS context lookup");
 if(manager>m.size()||m.size()-manager<0x38)throw std::runtime_error("Missing manager context extent");
 std::uintptr_t result;std::memcpy(&result,m.data()+manager+0x30,8);return result;
}
}
