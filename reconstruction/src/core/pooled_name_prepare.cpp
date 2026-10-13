#include "ctr/core/pooled_name_prepare.h"
#include "ctr/core/pooled_name_assign.h"
#include <array>
#include <cstring>
#include <stdexcept>
namespace ctr {
void prepare_pooled_name(std::span<std::byte> destination, std::uint64_t text_token) {
 if(destination.size()<12) throw std::invalid_argument("Pooled-name destination extent missing");
 const std::uint64_t zero=0;
 std::memcpy(destination.data(),&zero,8); // Original clears old pointer without release.
 if(text_token)
  throw std::runtime_error("missing: whole015a74c0 pooled-name lookup/create; original contention reaches libkernel1jfXLRVzisc");
 std::array<std::byte,8> temporary{};
 assign_pooled_name(destination,temporary,false,{},0);
 // Original releases temporary only when nonnull; the null path owns nothing.
 // Nonnull completion additionally requires whole0154df00 and real pool lifetime.
}
}
