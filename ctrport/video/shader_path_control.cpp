// SPDX-License-Identifier: GPL-2.0-only
#include "shader_path.h"
#include <stdexcept>
int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) return 1;
    namespace fs = std::filesystem;
    const fs::path root = fs::absolute(argv[1]);
    if (argc == 3) {
        try {
            (void)CtrVideo::ResolveShaderOutput(root, fs::path(argv[2]));
            return 5;
        } catch (const std::invalid_argument&) {
            return 0;
        }
    }
    const auto result = CtrVideo::ResolveShaderOutput(root, L"dumps/test.bin");
    if (result != root / L"runtime/user/shader/dumps/test.bin") return 2;
    const fs::path invalid[] = {
        L"../escape",  L"/absolute", L"C:stream", L"dumps/../escape", L"dumps/file:ads", L"dumps/end.",
        L"dumps/end ", L"dumps/*",   L"NUL",      L"con.txt",         L"dumps/COM1.bin", L"LPT9",
        L"AUX.ext",    L"PRN",       L"CONIN$",   L"CONOUT$",         L"COM\u00b9",      L"LPT\u00b2"};
    for (const auto& path : invalid) {
        try {
            (void)CtrVideo::ResolveShaderOutput(root, path);
            return 3;
        } catch (const std::invalid_argument&) {}
    }
    try {
        (void)CtrVideo::ResolveShaderOutput(L"relative", L"file");
        return 4;
    } catch (const std::invalid_argument&) {}
    return 0;
}
