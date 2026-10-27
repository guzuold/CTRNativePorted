// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "crypto.hpp"
#include <filesystem>
#include <fstream>
#include <windows.h>

namespace ctr_online {
inline std::filesystem::path backup_io_path(const std::filesystem::path& path) {
    auto absolute = std::filesystem::absolute(path).lexically_normal().make_preferred().wstring();
    if (absolute.rfind(L"\\\\?\\", 0) == 0) return absolute;
    if (absolute.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + absolute.substr(2);
    return L"\\\\?\\" + absolute;
}
// Immutable, content-addressed snapshots. Publication is atomic; incomplete writes
// never appear as completed snapshots. Existing copies are never overwritten.
inline std::filesystem::path backup_blob(const std::filesystem::path& directory, const std::string& content,
                                         const std::string& identity = "") {
    const auto normalized = backup_io_path(directory);
    std::filesystem::create_directories(normalized);
    const auto digest = hex(sha256(bytes_of(identity.empty() ? content : identity)));
    const auto target = normalized / (digest + ".json");
    if (std::filesystem::exists(target)) return target;
    const auto temporary = normalized / (digest + "." + hex(random_bytes(8)) + ".tmp");
    try {
        std::ofstream out(backup_io_path(temporary), std::ios::binary | std::ios::trunc);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.close();
        if (!out) throw std::runtime_error("Local backup write failed");
        if (!MoveFileExW(backup_io_path(temporary).c_str(), backup_io_path(target).c_str(), MOVEFILE_WRITE_THROUGH)) {
            const auto error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS)
                throw std::runtime_error("Local backup publication failed");
            std::filesystem::remove(temporary);
        }
    } catch (...) {
        std::error_code error;
        std::filesystem::remove(temporary, error);
        throw;
    }
    return target;
}

inline std::filesystem::path backup_savedata(const std::filesystem::path& root, const std::string& phase) {
    const auto source = backup_io_path(root / "runtime/user/savedata");
    if (!std::filesystem::exists(source)) return {};
    const auto destination =
        backup_io_path(root / "runtime/user/savedata_backups" /
                       (phase + "_" + std::to_string(GetTickCount64()) + "_" + hex(random_bytes(8))));
    const auto temporary = std::filesystem::path(destination.wstring() + L".partial");
    std::filesystem::create_directories(temporary.parent_path());
    std::filesystem::copy(source, temporary, std::filesystem::copy_options::recursive);
    std::filesystem::rename(temporary, destination);
    return destination;
}
} // namespace ctr_online
