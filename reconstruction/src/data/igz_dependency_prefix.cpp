#include "ctr/data/igz_dependency_prefix.h"
#include <cstring>
namespace ctr::data {
bool igz_dependency_prefix(const char* text, const char* prefix) {
    if (!text || !prefix) return text == prefix;
    return std::strncmp(text, prefix, std::strlen(prefix)) == 0;
}
}
