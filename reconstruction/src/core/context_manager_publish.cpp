#include "ctr/core/context_manager_publish.h"
namespace ctr {
void publish_context_manager(std::uint64_t& cell, std::uint64_t value) noexcept {
 cell = value;
}
}
