// SPDX-License-Identifier: GPL-2.0-only
// Launcher UI integration lives in launcher.cpp. Windows PowerShell 5.1 performs
// TLS downloads, ZIP validation and durable installation in a separate process.
#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <string>
#include "online_server/json.hpp"
// build_launcher.ps1 passes the version unquoted (CTR_LAUNCHER_VERSION_TOKEN=0.4.4):
// Windows PowerShell 5.1 drops quotes inside native arguments, so the string is made here.
#define CTR_LAUNCHER_STRING2(x) #x
#define CTR_LAUNCHER_STRING(x) CTR_LAUNCHER_STRING2(x)
#if !defined(CTR_LAUNCHER_VERSION) && defined(CTR_LAUNCHER_VERSION_TOKEN)
#define CTR_LAUNCHER_VERSION CTR_LAUNCHER_STRING(CTR_LAUNCHER_VERSION_TOKEN)
#endif
#ifndef CTR_LAUNCHER_VERSION
#define CTR_LAUNCHER_VERSION "0.4.0"
#endif

namespace ctr_update {
namespace fs = std::filesystem;
using ctr_online::Json;
inline constexpr char version[] = CTR_LAUNCHER_VERSION;
inline std::wstring wide(const std::string& value) {
    if (value.empty()) return {};
    const int size =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (!size) throw std::runtime_error("Invalid UTF-8 in update response.");
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), out.data(), size);
    return out;
}
inline std::wstring quote(const std::wstring& value) {
    std::wstring out = L"\"";
    size_t slashes = 0;
    for (const auto c : value) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        out.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    return out + L'"';
}
inline fs::path system_powershell() {
    wchar_t system[MAX_PATH]{};
    if (!GetSystemDirectoryW(system, MAX_PATH)) throw std::runtime_error("Cannot locate Windows PowerShell.");
    return fs::path(system) / L"WindowsPowerShell/v1.0/powershell.exe";
}
inline HANDLE start(const fs::path& script, const std::wstring& args) {
    const auto exe = system_powershell();
    std::wstring command = quote(exe.wstring()) +
                           L" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File " +
                           quote(script.wstring()) + args;
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                        &startup, &process))
        throw std::runtime_error("Cannot start the Windows update helper.");
    CloseHandle(process.hThread);
    return process.hProcess;
}
inline Json read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Update helper did not produce a result.");
    std::string data((std::istreambuf_iterator<char>(input)), {});
    if (data.size() > 4 * 1024 * 1024) throw std::runtime_error("Update response is too large.");
    return ctr_online::parse_json(data);
}
inline Json run(const fs::path& root, const fs::path& script, const fs::path& result, bool prepare) {
    try {
        const std::wstring args = std::wstring(L" -Mode ") + (prepare ? L"Prepare" : L"Check") + L" -Root " +
                                  quote(root.wstring()) + L" -CurrentVersion " + quote(wide(version)) + L" -Result " +
                                  quote(result.wstring());
        HANDLE process = start(script, args);
        // Network operations have their own timeout. Only this worker waits.
        const DWORD waited = WaitForSingleObject(process, prepare ? 600000 : 60000);
        DWORD code = 1;
        if (waited != WAIT_OBJECT_0) {
            TerminateProcess(process, 1);
            WaitForSingleObject(process, 5000);
            CloseHandle(process);
            throw std::runtime_error("Update request timed out.");
        }
        GetExitCodeProcess(process, &code);
        CloseHandle(process);
        auto response = read(result);
        if (code && response.at("ok").boolean) throw std::runtime_error("Update helper failed.");
        return response;
    } catch (const std::exception& e) {
        auto response = Json::object();
        response.set("ok", Json::of(false));
        response.set("error", Json::of(e.what()));
        return response;
    }
}
inline void launch_apply(const fs::path& root, const fs::path& script, const fs::path& txn, bool recover = false) {
    const auto args = std::wstring(L" -Mode ") + (recover ? L"Recover" : L"Apply") + L" -Root " +
                      quote(root.wstring()) + L" -Transaction " + quote(txn.wstring()) + L" -ParentId " +
                      std::to_wstring(GetCurrentProcessId());
    HANDLE process = start(script, args);
    CloseHandle(process);
}
inline bool portable(const fs::path& root, const fs::path& exe_dir) {
    std::error_code ec;
    return !root.empty() && fs::equivalent(root, exe_dir, ec) && !ec && !fs::exists(root / ".git") &&
           fs::exists(root / "launcher.exe") && fs::exists(root / "launcher_update.ps1");
}
inline void assert_plain_path(fs::path path) {
    for (; !path.empty(); path = path.parent_path()) {
        const auto flags = GetFileAttributesW(path.c_str());
        if (flags != INVALID_FILE_ATTRIBUTES && (flags & FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::runtime_error("Updates cannot use junctions or symbolic links.");
        if (path == path.parent_path()) break;
    }
}
// An installation is held by exactly one launcher or installer. The helper opens
// the same file exclusively after the old launcher has exited.
inline HANDLE acquire_install_lock(const fs::path& root) {
    const auto dir = root / "runtime/user/updates";
    assert_plain_path(dir);
    fs::create_directories(dir);
    const auto file = dir / "install.lock";
    assert_plain_path(file);
    return CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                       nullptr);
}
}
