// SPDX-License-Identifier: GPL-2.0-only
// Decodes every function of a flat eboot image with x86_length.hpp and writes one record per
// instruction, for tools/region_map.py --check-decoder to compare with capstone.
//   x86_length_check <image.bin> <functions.bin> <out.bin>
// functions.bin: pairs of little-endian u64 (start, size). out.bin: per instruction six bytes
// (length, disp offset, disp size, imm offset, imm size, flags: 1 rip, 2 relative); a function
// whose decoding stops early ends with a zero length; every function ends with six 0xFF bytes.
#include "x86_length.hpp"
#include <cstdio>
#include <vector>

static std::vector<unsigned char> read(const char* path) {
    std::vector<unsigned char> data;
    if (FILE* f = std::fopen(path, "rb")) {
        unsigned char buffer[1 << 16];
        for (std::size_t n; (n = std::fread(buffer, 1, sizeof(buffer), f)) > 0;)
            data.insert(data.end(), buffer, buffer + n);
        std::fclose(f);
    }
    return data;
}

int main(int argc, char** argv) {
    if (argc != 4) return 2;
    const auto image = read(argv[1]), functions = read(argv[2]);
    FILE* out = std::fopen(argv[3], "wb");
    if (image.empty() || functions.empty() || !out) return 1;
    for (std::size_t f = 0; f + 16 <= functions.size(); f += 16) {
        unsigned long long start = 0, size = 0;
        for (int b = 7; b >= 0; --b) {
            start = start << 8 | functions[f + b];
            size = size << 8 | functions[f + 8 + b];
        }
        for (unsigned long long at = start; at < start + size && at < image.size();) {
            const auto insn = ctrport::x86::decode(image.data() + at, static_cast<std::size_t>(start + size - at));
            const unsigned char record[6] = {static_cast<unsigned char>(insn.length),
                                             static_cast<unsigned char>(insn.disp_offset),
                                             static_cast<unsigned char>(insn.disp_size),
                                             static_cast<unsigned char>(insn.imm_offset),
                                             static_cast<unsigned char>(insn.imm_size),
                                             static_cast<unsigned char>((insn.rip ? 1 : 0) | (insn.relative ? 2 : 0))};
            std::fwrite(record, 1, 6, out);
            if (!insn.length) break;
            at += insn.length;
        }
        const unsigned char end[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        std::fwrite(end, 1, 6, out);
    }
    std::fclose(out);
    return 0;
}
