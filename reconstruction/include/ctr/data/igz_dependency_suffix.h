#pragma once
namespace ctr::data {
// Original 01569fc0..0156a035; AL result. Both strings must be non-null,
// terminated; supported lengths below INT32_MAX. Original libc static ASCII
// folding table maps A..Z to a..z; bytes >=128 are compared unchanged.
bool igz_dependency_suffix(const char* text, const char* suffix);
}
