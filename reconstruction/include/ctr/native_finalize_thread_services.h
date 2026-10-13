#pragma once
#include <cstdint>
namespace ctr {
struct NativeFinalizeThreadServices;
NativeFinalizeThreadServices* create_finalize_thread_services(std::uint64_t logical_mutex);
int release_finalize_thread_services(NativeFinalizeThreadServices* owner) noexcept;
int finalize_thread_self(NativeFinalizeThreadServices* owner,std::uint64_t* thread_token) noexcept;
int finalize_mutex_lock(NativeFinalizeThreadServices* owner,std::uint64_t logical_mutex) noexcept;
int finalize_mutex_unlock(NativeFinalizeThreadServices* owner,std::uint64_t logical_mutex) noexcept;
}
