// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>
namespace ctrport {
inline std::filesystem::path app0_read_path(const std::filesystem::path& base,
                                           const std::filesystem::path& patch,
                                           const std::string& relative, bool regular_only=true) {
    namespace fs = std::filesystem;
    if (relative.empty() || relative.find(':') != std::string::npos ||
        relative.find('\\') != std::string::npos)
        throw std::runtime_error("invalid app0 relative path");
    fs::path requested(relative);
    if (requested.is_absolute() || requested.has_root_path())
        throw std::runtime_error("absolute app0 path rejected");
    for (const auto& part : requested)
        if (part == ".." || part == ".")
            throw std::runtime_error("app0 traversal rejected");
    const auto resolve = [&](const fs::path& root) -> fs::path {
        if (root.empty()) return {};
        const auto canonical_root = fs::canonical(root);
        const auto candidate = canonical_root / requested;
        if (!fs::exists(candidate)) return {};
        const auto actual = fs::canonical(candidate);
        const auto within = actual.lexically_relative(canonical_root);
        if (within.empty() || within.is_absolute())
            throw std::runtime_error("app0 target outside root");
        for (const auto& part : within)
            if (part == "..") throw std::runtime_error("app0 link escapes root");
        if (regular_only && !fs::is_regular_file(actual))
            throw std::runtime_error("app0 target is not a file");
        return actual;
    };
    if (!patch.empty()) {
        const auto candidate = resolve(patch);
        if (!candidate.empty()) return candidate;
    }
    const auto candidate = resolve(base);
    if (candidate.empty()) throw std::runtime_error("app0 file missing");
    return candidate;
}
}
