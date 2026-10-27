// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#include <fstream>
#include <vector>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <map>
#include <memory>
#include "retail_pins.hpp"
namespace ctrport {
inline std::uint64_t little(const std::vector<unsigned char>& data, std::size_t offset, std::size_t width) {
    if (offset > data.size() || width > data.size() - offset) throw std::runtime_error("ELF truncated");
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < width; ++i) value |= std::uint64_t(data[offset + i]) << (i * 8);
    return value;
}
// A loaded image owns its reservation until its last loader operation finishes.
struct ImageRelease {
    void operator()(unsigned char* address) const noexcept {
        if (address) VirtualFree(address, 0, MEM_RELEASE);
    }
};
struct OriginalSymbol {
    std::uint64_t size = 0;
    std::string nid, library, module;
    std::uint64_t type = 0, section = 0, address = 0, slot = 0, addend = 0, relocation = 0;
};
struct ImageMetadata {
    std::string sha256;
    bool has_tls = false;
    std::uint64_t tls_size = 0, tls_alignment = 1;
    std::vector<unsigned char> tls_initialized;
    std::vector<std::uint64_t> tls_module_slots;
    std::vector<OriginalSymbol> exports, imports;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> spans, executable_spans;
};
using MappedImage = std::unique_ptr<unsigned char, ImageRelease>;
inline MappedImage load_retail_image(const char* filename, bool inspect_symbols = false,
                                     ImageMetadata* metadata = nullptr) {
    std::ifstream input(filename, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("flat ELF missing");
    const auto size = input.tellg();
    if (size <= 0 || size > 128 * 1024 * 1024) throw std::runtime_error("flat ELF size rejected");
    std::vector<unsigned char> data(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(data.data()), size)) throw std::runtime_error("flat ELF read failed");
    unsigned char digest[32];
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("SHA256 provider missing");
    const auto status = BCryptHash(algorithm, nullptr, 0, data.data(), static_cast<ULONG>(data.size()), digest, 32);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) throw std::runtime_error("SHA256 failed");
    std::ostringstream hash;
    for (auto byte : digest) hash << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
    const bool pinned = pinned_retail_image(hash.str());
    const bool original_self = pinned && data.size() >= 4 && little(data, 0, 4) == 0x1d3d154f;
    if (!pinned && hash.str() != "3e0158933a6bf690cf386f5ab846b0ef7b3d30ff8fc75c420d0dc41736fc4b9a")
        throw std::runtime_error("flat ELF retail 1.00 hash mismatch");
    if (original_self) {
        // Plain SELF translation follows the project's read-only oracle images.py.
        const auto descriptors = little(data, 24, 2), elf = 32 + descriptors * 32;
        const auto phoff = little(data, static_cast<std::size_t>(elf + 32), 8);
        const auto stride = little(data, static_cast<std::size_t>(elf + 54), 2),
                   count = little(data, static_cast<std::size_t>(elf + 56), 2);
        if (stride != 56 || count > 128 || elf > data.size() || phoff > data.size() - elf ||
            count * stride > data.size() - elf - phoff)
            throw std::runtime_error("SELF program table rejected");
        std::uint64_t logical_size = phoff + count * stride;
        for (std::uint64_t i = 0; i < count; ++i) {
            const auto p = static_cast<std::size_t>(elf + phoff + i * stride);
            const auto offset = little(data, p + 8, 8), bytes = little(data, p + 32, 8);
            if (offset > 128 * 1024 * 1024 || bytes > 128 * 1024 * 1024 - offset)
                throw std::runtime_error("SELF logical extent rejected");
            logical_size = (std::max)(logical_size, offset + bytes);
        }
        std::vector<unsigned char> logical(static_cast<std::size_t>(logical_size));
        std::memcpy(logical.data(), data.data() + elf, static_cast<std::size_t>(phoff + count * stride));
        for (std::uint64_t i = 0; i < descriptors; ++i) {
            const auto d = static_cast<std::size_t>(32 + i * 32), flags = little(data, d, 8);
            if (!(flags & 0x800)) continue;
            if (flags & 10) throw std::runtime_error("compressed/encrypted SELF unsupported");
            const auto index = (flags >> 20) & 4095;
            if (index >= count) throw std::runtime_error("SELF segment index rejected");
            const auto p = static_cast<std::size_t>(elf + phoff + index * stride);
            const auto target = little(data, p + 8, 8), bytes = little(data, p + 32, 8);
            const auto source = little(data, d + 8, 8), available = little(data, d + 16, 8);
            if (bytes > available || source > data.size() || bytes > data.size() - source)
                throw std::runtime_error("SELF plaintext segment truncated");
            std::memcpy(logical.data() + target, data.data() + source, static_cast<std::size_t>(bytes));
        }
        data.swap(logical);
    }
    if (data.size() < 64 || std::memcmp(data.data(),
                                        "\x7f"
                                        "ELF\x02\x01",
                                        6))
        throw std::runtime_error("ELF64 required");
    const auto table = little(data, 32, 8), count = little(data, 56, 2), stride = little(data, 54, 2);
    if (stride != 56 || count > 128 || table > data.size() || count * stride > data.size() - table)
        throw std::runtime_error("ELF program table rejected");
    struct Span {
        std::uint64_t address, memory, file, offset;
    };
    std::uint64_t dynamic_offset = 0, dynamic_size = 0, link_offset = 0, link_size = 0;
    std::vector<Span> spans;
    std::uint64_t extent = 0;
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto p = static_cast<std::size_t>(table + i * stride);
        const auto type = little(data, p, 4);
        if (type == 2) {
            dynamic_offset = little(data, p + 8, 8);
            dynamic_size = little(data, p + 32, 8);
        }
        if (type == 0x61000000) {
            link_offset = little(data, p + 8, 8);
            link_size = little(data, p + 32, 8);
        }
        if (type != 1 && type != 0x61000010) continue;
        Span s{little(data, p + 16, 8), little(data, p + 40, 8), little(data, p + 32, 8), little(data, p + 8, 8)};
        if (!s.memory || s.file > s.memory || s.offset > data.size() || s.file > data.size() - s.offset ||
            s.address > 128 * 1024 * 1024 || s.memory > 128 * 1024 * 1024 - s.address)
            throw std::runtime_error("ELF load span rejected");
        for (const auto& old : spans)
            if (s.address < old.address + old.memory && s.address + s.memory > old.address)
                throw std::runtime_error("ELF load spans overlap");
        extent = (std::max)(extent, s.address + s.memory);
        spans.push_back(s);
    }
    if (spans.empty()) throw std::runtime_error("ELF has no load segments");
    if (!dynamic_size || !link_size)
        throw std::runtime_error(
            "flat ELF lacks SCE dynamic/link metadata; original SELF metadata required before relocation");
    std::map<std::uint64_t, std::uint64_t> tags;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> entries;
    if (dynamic_offset > data.size() || dynamic_size > data.size() - dynamic_offset || dynamic_size % 16 ||
        link_offset > data.size() || link_size > data.size() - link_offset)
        throw std::runtime_error("ELF dynamic extent rejected");
    for (std::uint64_t at = dynamic_offset; at < dynamic_offset + dynamic_size; at += 16) {
        const auto tag = little(data, static_cast<std::size_t>(at), 8);
        const auto value = little(data, static_cast<std::size_t>(at + 8), 8);
        tags[tag] = value;
        entries.emplace_back(tag, value);
    }
    const auto link_value = [&](std::uint64_t at, std::size_t width) {
        if (at > link_size || width > link_size - at) throw std::runtime_error("ELF relocation metadata truncated");
        return little(data, static_cast<std::size_t>(link_offset + at), width);
    };
    const auto symbol_name = [&](std::uint64_t offset) {
        const auto start = tags.at(0x61000035) + offset;
        std::string result;
        for (auto cursor = start; cursor < link_size; ++cursor) {
            const auto byte = link_value(cursor, 1);
            if (!byte) return result;
            if (byte < 32 || byte > 126) throw std::runtime_error("SCE symbol string rejected");
            result.push_back(static_cast<char>(byte));
        }
        throw std::runtime_error("SCE symbol string unterminated");
    };
    const auto encoded = [](std::uint64_t value) {
        const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";
        std::string result;
        do {
            result.insert(result.begin(), alphabet[value & 63]);
            value >>= 6;
        } while (value);
        return result;
    };
    std::map<std::string, std::string> libraries, modules;
    for (const auto& entry : entries) {
        if (entry.first == 0x61000013 || entry.first == 0x61000015)
            libraries[encoded(entry.second >> 48)] = symbol_name(entry.second & 0xffffffff);
        if (entry.first == 0x6100000d || entry.first == 0x6100000f)
            modules[encoded(entry.second >> 48)] = symbol_name(entry.second & 0xffffffff);
    }
    if (inspect_symbols) {
        const auto bytes = tags.at(0x6100003f), begin = tags.at(0x61000039);
        if (bytes % 24 || begin > link_size || bytes > link_size - begin)
            throw std::runtime_error("SCE symbol table rejected");
        std::cout << "{\"image_sha256\":" << std::quoted(hash.str()) << ",\"symbols\":[";
        for (std::uint64_t offset = 0; offset < bytes; offset += 24) {
            const auto at = begin + offset;
            const auto name = symbol_name(link_value(at, 4));
            const auto first = name.find('#'), second = first == std::string::npos ? first : name.find('#', first + 1);
            const auto library =
                first == std::string::npos ? std::string() : name.substr(first + 1, second - first - 1);
            const auto module = second == std::string::npos ? std::string() : name.substr(second + 1);
            if (offset) std::cout << ',';
            std::cout << "{\"symbol\":" << std::quoted(name) << ",\"nid\":" << std::quoted(name.substr(0, first))
                      << ",\"library\":"
                      << std::quoted(libraries.count(library) ? libraries.at(library) : std::string())
                      << ",\"module\":" << std::quoted(modules.count(module) ? modules.at(module) : std::string())
                      << ",\"type\":" << (link_value(at + 4, 1) & 15) << ",\"section\":" << link_value(at + 6, 2)
                      << ",\"address\":" << link_value(at + 8, 8) << '}';
        }
        std::cout << "]}\n";
    }
    const auto read_symbol = [&](std::uint64_t index) {
        if (index >= tags.at(0x6100003f) / 24) throw std::runtime_error("SCE symbol index rejected");
        const auto at = tags.at(0x61000039) + index * 24;
        const auto name = symbol_name(link_value(at, 4));
        const auto first = name.find('#'), second = first == std::string::npos ? first : name.find('#', first + 1);
        const auto library = first == std::string::npos ? std::string() : name.substr(first + 1, second - first - 1);
        const auto module = second == std::string::npos ? std::string() : name.substr(second + 1);
        OriginalSymbol symbol;
        symbol.nid = name.substr(0, first);
        symbol.library = libraries.count(library) ? libraries.at(library) : std::string();
        symbol.module = modules.count(module) ? modules.at(module) : std::string();
        symbol.type = link_value(at + 4, 1) & 15;
        symbol.section = link_value(at + 6, 2);
        symbol.address = link_value(at + 8, 8);
        symbol.size = link_value(at + 16, 8);
        return symbol;
    };
    if (metadata) {
        *metadata = ImageMetadata{};
        metadata->sha256 = hash.str();
        for (std::uint64_t i = 0; i < count; ++i) {
            const auto at = static_cast<std::size_t>(table + i * stride);
            if (little(data, at, 4) != 7) continue;
            if (metadata->has_tls) throw std::runtime_error("Multiple TLS segments rejected");
            const auto offset = little(data, at + 8, 8), files = little(data, at + 32, 8),
                       memory = little(data, at + 40, 8), alignment = little(data, at + 48, 8);
            if (files > memory || memory > 16 * 1024 * 1024 || !alignment || alignment > 65536 ||
                (alignment & (alignment - 1)) || offset > data.size() || files > data.size() - offset)
                throw std::runtime_error("TLS template extent rejected");
            metadata->has_tls = true;
            metadata->tls_size = memory;
            metadata->tls_alignment = alignment;
            metadata->tls_initialized.assign(data.begin() + static_cast<std::size_t>(offset),
                                             data.begin() + static_cast<std::size_t>(offset + files));
        }

        for (const auto& span : spans) metadata->spans.emplace_back(span.address, span.address + span.memory);
        for (std::uint64_t i = 0; i < count; ++i) {
            const auto at = static_cast<std::size_t>(table + i * stride);
            if (little(data, at, 4) == 1 && (little(data, at + 4, 4) & 1)) {
                const auto address = little(data, at + 16, 8);
                metadata->executable_spans.emplace_back(address, address + little(data, at + 40, 8));
            }
        }
        for (std::uint64_t i = 0; i < tags.at(0x6100003f) / 24; ++i) {
            const auto symbol = read_symbol(i);
            if (symbol.section && symbol.address && (symbol.type == 2 || symbol.type == 1))
                metadata->exports.push_back(symbol);
        }
    }
    std::vector<std::pair<std::uint64_t, std::uint64_t>> relocations;
    std::uint64_t missing_imports = 0, unresolved_tls = 0;
    for (auto pair : {std::pair<std::uint64_t, std::uint64_t>{0x61000029, 0x6100002d}, {0x6100002f, 0x61000031}}) {
        if (!tags.count(pair.first)) continue;
        const auto begin = tags.at(pair.first), bytes = tags.at(pair.second);
        if (bytes % 24 || begin > link_size || bytes > link_size - begin)
            throw std::runtime_error("ELF relocation table rejected");
        for (std::uint64_t at = begin; at < begin + bytes; at += 24) {
            const auto slot = link_value(at, 8), info = link_value(at + 8, 8), addend = link_value(at + 16, 8);
            const auto kind = info & 0xffffffff;
            if (metadata && (kind == 6 || kind == 7 || (kind == 1 && !read_symbol(info >> 32).section))) {
                auto symbol = read_symbol(info >> 32);
                symbol.slot = slot;
                symbol.addend = addend;
                symbol.relocation = kind;
                metadata->imports.push_back(symbol);
            }
            if (kind == 8)
                relocations.emplace_back(slot, addend);
            else if (kind == 1) {
                const auto sym = tags.at(0x61000039) + (info >> 32) * 24;
                if (link_value(sym + 6, 2))
                    relocations.emplace_back(slot, link_value(sym + 8, 8) + addend);
                else
                    ++missing_imports;
            } else if (kind == 6 || kind == 7)
                ++missing_imports;
            else if (kind == 16) {
                ++unresolved_tls;
                if (metadata) metadata->tls_module_slots.push_back(slot);
            } else if (kind)
                throw std::runtime_error("ELF relocation type unsupported");
        }
    }
    const auto backed = [&](std::uint64_t at, std::uint64_t width) {
        for (const auto& span : spans)
            if (at >= span.address && at <= span.address + span.memory && width <= span.address + span.memory - at)
                return true;
        return false;
    };
    for (const auto& relocation : relocations)
        // An address relocation may name the one-past-end sentinel; this does
        // not permit any dereference or execution at that address.
        if (!backed(relocation.first, 8) || !backed(relocation.second, 0))
            throw std::runtime_error("ELF relocation outside mapped spans");
    auto* mapped =
        static_cast<unsigned char*>(VirtualAlloc(nullptr, static_cast<SIZE_T>(extent), MEM_RESERVE, PAGE_NOACCESS));
    if (!mapped) throw std::runtime_error("ELF reserve failed");
    MappedImage owner(mapped);
    bool okay = true;
    for (const auto& s : spans) {
        if (!VirtualAlloc(mapped + s.address, static_cast<SIZE_T>(s.memory), MEM_COMMIT, PAGE_READWRITE)) {
            okay = false;
            break;
        }
        std::memcpy(mapped + s.address, data.data() + s.offset, static_cast<std::size_t>(s.file));
        if (std::memcmp(mapped + s.address, data.data() + s.offset, static_cast<std::size_t>(s.file))) {
            okay = false;
            break;
        }
    }
    if (okay)
        for (const auto& relocation : relocations) {
            const auto target = reinterpret_cast<std::uint64_t>(mapped) + relocation.second;
            std::memcpy(mapped + relocation.first, &target, 8);
            std::uint64_t observed = 0;
            std::memcpy(&observed, mapped + relocation.first, 8);
            if (observed != target) {
                okay = false;
                break;
            }
        }
    if (!okay) throw std::runtime_error("ELF map comparison failed");
    std::cout << "{\"flat_mapping_verified\":true,\"segments\":" << spans.size()
              << ",\"internal_relocations_applied\":" << relocations.size()
              << ",\"missing_imports\":" << missing_imports << ",\"unresolved_tls\":" << unresolved_tls
              << ",\"executable_permissions\":false,\"main_reached\":false,\"runtime_replacements\":0}\n";
    return owner;
}
inline std::size_t apply_tls_module_id(MappedImage& image, const ImageMetadata& metadata, std::uint64_t id) {
    if (!image || !id || !metadata.has_tls) throw std::runtime_error("TLS module identity missing");
    for (auto slot : metadata.tls_module_slots) {
        bool valid = false;
        for (const auto& span : metadata.spans)
            if (slot >= span.first && slot <= span.second && 8 <= span.second - slot) valid = true;
        if (!valid) throw std::runtime_error("TLS relocation outside owned span");
    }
    for (auto slot : metadata.tls_module_slots) {
        std::memcpy(image.get() + slot, &id, 8);
        std::uint64_t observed = 0;
        std::memcpy(&observed, image.get() + slot, 8);
        if (observed != id) throw std::runtime_error("TLS module relocation readback failed");
    }
    return metadata.tls_module_slots.size();
}
}
