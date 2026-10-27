// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Bounded game logs. The game's stdout/stderr go through pipes; a reader thread per stream
// keeps the first `head` bytes and the last `tail` bytes in the log file, with a marker line
// for what was left out. Only the newest sessions are kept in runtime/user/logs.
// Before 0.6.0 the launcher handed the files themselves to the game: single logs reached
// several GB and a logs folder reached tens of GB.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace ctrport::bounded_log {
namespace fs = std::filesystem;
constexpr std::uint64_t head_bytes = 32ull << 20, tail_bytes = 16ull << 20;
constexpr std::size_t kept_sessions = 8;

class Capture {
public:
    // Creates the pipe; write() is the inheritable handle to give to the child process.
    explicit Capture(const fs::path& file) : path_(file) {
        SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
        if (!CreatePipe(&read_, &write_, &inherit, 1 << 16)) {
            read_ = write_ = nullptr;
            return;
        }
        SetHandleInformation(read_, HANDLE_FLAG_INHERIT, 0);
        out_ = CreateFileW(path_.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
        done_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }
    Capture(const Capture&) = delete;
    Capture& operator=(const Capture&) = delete;
    ~Capture() {
        close_writer();
        if (thread_.joinable()) {
            // Another process that inherited the pipe can keep it open after the game exits:
            // never block on it. The still-running reader keeps this object's handles.
            if (!wait(0)) {
                thread_.detach();
                return;
            }
            thread_.join();
        }
        for (HANDLE h : {read_, out_, done_})
            if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    bool valid() const { return read_ && write_ && out_ != INVALID_HANDLE_VALUE && done_; }
    HANDLE write() const { return write_; }
    // After CreateProcess: the launcher's copy of the write end must be closed, or the
    // reader never sees the end of the stream.
    void close_writer() {
        if (write_) {
            CloseHandle(write_);
            write_ = nullptr;
        }
    }
    void start() { thread_ = std::thread(run, read_, out_, done_); }
    // Waits until the file holds everything the game wrote (bounded wait).
    bool wait(DWORD milliseconds) const { return done_ && WaitForSingleObject(done_, milliseconds) == WAIT_OBJECT_0; }

private:
    // Works on handle copies only, so a detached reader never touches a destroyed object.
    static void run(HANDLE read_, HANDLE out_, HANDLE done_) {
        auto put = [&](const char* data, DWORD size) {
            DWORD written = 0;
            WriteFile(out_, data, size, &written, nullptr);
        };
        std::vector<char> buffer(1 << 16);
        std::uint64_t written = 0, dropped = 0, tail_size = 0;
        std::deque<std::string> tail;
        DWORD got = 0;
        while (ReadFile(read_, buffer.data(), static_cast<DWORD>(buffer.size()), &got, nullptr) && got) {
            const char* data = buffer.data();
            if (written < head_bytes) {
                const DWORD direct = static_cast<DWORD>(std::min<std::uint64_t>(got, head_bytes - written));
                put(data, direct);
                written += direct;
                data += direct;
                got -= direct;
                if (!got) continue;
            }
            tail.emplace_back(data, got);
            tail_size += got;
            while (tail_size - tail.front().size() >= tail_bytes) {
                tail_size -= tail.front().size();
                dropped += tail.front().size();
                tail.pop_front();
            }
        }
        if (dropped) {
            const std::string marker = "\n...[log limited: " + std::to_string(dropped >> 20) + " MB omitted]...\n";
            put(marker.data(), static_cast<DWORD>(marker.size()));
        }
        for (const auto& chunk : tail) put(chunk.data(), static_cast<DWORD>(chunk.size()));
        FlushFileBuffers(out_);
        SetEvent(done_);
    }
    fs::path path_;
    HANDLE read_ = nullptr, write_ = nullptr, out_ = INVALID_HANDLE_VALUE, done_ = nullptr;
    std::thread thread_;
};

// Deletes all but the newest kept_sessions game sessions (play_* logs) in `logs`.
inline void prune(const fs::path& logs) {
    std::error_code ec;
    std::vector<std::pair<std::wstring, std::vector<fs::path>>> sessions;
    for (const auto& entry : fs::directory_iterator(logs, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const auto name = entry.path().filename().wstring();
        if (name.rfind(L"play_", 0) != 0 || entry.path().extension() != L".log") continue;
        const auto stem = name.substr(0, name.find(L'.'));
        auto at = std::find_if(sessions.begin(), sessions.end(), [&](const auto& s) { return s.first == stem; });
        if (at == sessions.end()) {
            sessions.push_back({stem, {}});
            at = sessions.end() - 1;
        }
        at->second.push_back(entry.path());
    }
    // Session names are play_YYYYMMDD_HHMMSS: name order is time order.
    std::sort(sessions.begin(), sessions.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (std::size_t i = kept_sessions; i < sessions.size(); ++i)
        for (const auto& file : sessions[i].second) fs::remove(file, ec);
}
} // namespace ctrport::bounded_log
