// SPDX-License-Identifier: GPL-2.0-only
// Checks ctrport/xbox_prepare.hpp against the user's retail archives.
//   extract <pak> <entry>            SHA256 of a decompressed entry (compare with ctr_nf_iga.py)
//   prepare <pak> <user-dir> <assets> build the overlay archive under <user-dir>
//   compare <font-a> <font-b>         count differing bytes between two patched fonts
#include "../../ctrport/xbox_prepare.hpp"
#include <cstdio>
#include <iostream>
using namespace ctrport::xbox_prepare;

static Bytes read_all(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(f), {});
}

int wmain(int argc, wchar_t** argv) {
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartup(&token, &input, nullptr);
    try {
        const std::wstring mode = argc > 1 ? argv[1] : L"";
        if (mode == L"extract" && argc == 4) {
            Archive archive(argv[2]);
            const std::string wanted = lower(fs::path(argv[3]).string());
            for (const auto& e : archive.files)
                if (lower(e.path) == wanted) {
                    const Bytes data = archive.extract(e);
                    std::printf("%s %s %zu\n", sha256(data).c_str(), e.path.c_str(), data.size());
                    return 0;
                }
            std::printf("entry not found\n");
            return 2;
        }
        if (mode == L"extract-to" && argc == 5) {
            Archive archive(argv[2]);
            const std::string wanted = lower(fs::path(argv[3]).string());
            for (const auto& e : archive.files)
                if (lower(e.path) == wanted) {
                    const Bytes data = archive.extract(e);
                    std::ofstream(argv[4], std::ios::binary)
                        .write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
                    std::printf("%s %s %zu\n", sha256(data).c_str(), e.path.c_str(), data.size());
                    return 0;
                }
            return 2;
        }
        if (mode == L"patch" && argc == 5) {
            const Bytes out = patch_font(read_all(argv[2]), argv[3]);
            std::ofstream(argv[4], std::ios::binary)
                .write(reinterpret_cast<const char*>(out.data()), std::streamsize(out.size()));
            std::printf("%s\n", sha256(out).c_str());
            return 0;
        }
        if (mode == L"prepare" && argc == 5) {
            const fs::path pak = prepare(argv[2], argv[3], argv[4]);
            std::wcout << pak.wstring() << L"\n";
            return 0;
        }
        if (mode == L"compare" && argc == 4) {
            const Bytes a = read_all(argv[2]), b = read_all(argv[3]);
            if (a.size() != b.size()) {
                std::printf("size differs %zu %zu\n", a.size(), b.size());
                return 1;
            }
            size_t diff = 0, first = SIZE_MAX, last = 0;
            for (size_t i = 0; i < a.size(); ++i)
                if (a[i] != b[i]) {
                    ++diff;
                    first = std::min(first, i);
                    last = i;
                }
            std::printf("differing bytes %zu (first %zu, last %zu) of %zu\n", diff, first, last, a.size());
            return 0;
        }
        std::printf("usage: extract|extract-to|patch|prepare|compare ...\n");
        return 2;
    } catch (const std::exception& e) {
        std::printf("ERROR: %s\n", e.what());
        return 1;
    }
}
