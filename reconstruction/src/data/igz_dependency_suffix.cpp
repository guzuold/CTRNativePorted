#include "ctr/data/igz_dependency_suffix.h"
#include <cstring>
namespace ctr::data {
bool igz_dependency_suffix(const char* text, const char* suffix) {
    const auto text_size = std::strlen(text);
    const auto suffix_size = std::strlen(suffix);
    if (text_size < suffix_size) return false;
    text += text_size - suffix_size;
    for (;;) {
        const auto a = static_cast<unsigned char>(*text++);
        const auto b = static_cast<unsigned char>(*suffix++);
        const auto folded_a = a >= 'A' && a <= 'Z' ? a + 32 : a;
        const auto folded_b = b >= 'A' && b <= 'Z' ? b + 32 : b;
        if (folded_a != folded_b) return false;
        if (!a) return true;
    }
}
}
