#pragma once
namespace ctr::data {
// Original 01569f60..01569fb7, declared bool result is AL only.
// Non-null inputs must point to valid terminated strings. Original strlen result
// is narrowed to signed int32; supported fixtures exclude lengths >= INT32_MAX.
bool igz_dependency_prefix(const char* text, const char* prefix);
}
