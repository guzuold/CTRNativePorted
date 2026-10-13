#include "ctr/core/context_manager_reset.h"
#include <stdexcept>
namespace ctr {
void reset_context_manager(std::uint64_t& cell,std::uint8_t lifecycle_flag) {
 if(!lifecycle_flag)throw std::runtime_error("Missing original lifecycle014ed4b0 before manager reset");
 cell=0;
}
}
