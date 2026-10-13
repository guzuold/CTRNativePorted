// SPDX-License-Identifier: GPL-2.0-only
// Windows host description only; never fabricates a guest pthread identity.
#include <windows.h>
#include <string>
#include <stdexcept>
namespace Common {
std::string GetCurrentThreadName() {
    PWSTR description=nullptr;
    if (FAILED(GetThreadDescription(GetCurrentThread(), &description)))
        throw std::runtime_error("CTR graphics: GetThreadDescription failed");
    struct Release { PWSTR value; ~Release(){ LocalFree(value); } } release{description};
    if (!description || !*description) return {};
    const int size=WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, description, -1, nullptr, 0, nullptr, nullptr);
    if (!size) throw std::runtime_error("CTR graphics: invalid thread description");
    std::string result(static_cast<size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, description, -1, result.data(), size, nullptr, nullptr)!=size)
        throw std::runtime_error("CTR graphics: thread description conversion failed");
    result.pop_back();
    return result;
}
}
