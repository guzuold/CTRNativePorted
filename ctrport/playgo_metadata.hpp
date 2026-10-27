// SPDX-License-Identifier: GPL-2.0-only
// Own bounded PlayGo metadata and handle lifecycle. Public layout/behavior:
// shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a playgo_chunk.h/cpp,
// playgo.cpp Initialize/Open/GetChunkId/Terminate; GPL-2.0-or-later.
// Does not declare chunks installed, fabricate progress, ETA or a language mask.
#pragma once
#include "flat_loader.hpp"
#include "kernel_file.hpp"
namespace ctrport {
inline unsigned parse_playgo_metadata(const std::vector<unsigned char>& bytes) {
    if (bytes.size() < 256 || little(bytes, 0, 4) != 0x6f676c70) throw std::runtime_error("PlayGo header invalid");
    const auto chunks = static_cast<unsigned>(little(bytes, 10, 2));
    const auto micros = little(bytes, 12, 2), scenarios = little(bytes, 14, 2);
    if (little(bytes, 8, 2) > 1 || chunks > 1000 || micros > 8000 || scenarios > 32 ||
        little(bytes, 16, 4) != bytes.size())
        throw std::runtime_error("PlayGo header bounds invalid");
    std::array<std::pair<std::size_t, std::size_t>, 8> table{};
    for (unsigned i = 0; i < 8; ++i) {
        auto offset = static_cast<std::size_t>(little(bytes, 192 + i * 8, 4)),
             length = static_cast<std::size_t>(little(bytes, 196 + i * 8, 4));
        if (offset > bytes.size() || length > bytes.size() - offset || (length && offset < 256))
            throw std::runtime_error("PlayGo table extent invalid");
        table[i] = {offset, length};
    }
    if (table[0].second < chunks * 32ULL || table[3].second < micros * 16ULL || table[4].second < scenarios * 32ULL)
        throw std::runtime_error("PlayGo attribute extent invalid");
    for (unsigned i = 0; i < chunks; ++i) {
        const auto at = table[0].first + i * 32;
        const auto count = little(bytes, at + 14, 2), offset = little(bytes, at + 24, 4),
                   label = little(bytes, at + 28, 4);
        if (offset > table[1].second || count * 2 > table[1].second - offset || label >= table[2].second)
            throw std::runtime_error("PlayGo chunk references invalid");
        auto end = table[2].first + static_cast<std::size_t>(label);
        while (end < table[2].first + table[2].second && bytes[end]) ++end;
        if (end == table[2].first + table[2].second) throw std::runtime_error("PlayGo chunk label unterminated");
        for (std::size_t n = 0; n < count; ++n)
            if (little(bytes, table[1].first + static_cast<std::size_t>(offset) + n * 2, 2) >= micros)
                throw std::runtime_error("PlayGo microchunk index invalid");
    }
    return chunks;
}
inline int playgo_error(unsigned low) {
    return static_cast<int>(0x80b20000U | low);
}
struct PlaygoInit {
    const void* buffer;
    std::uint32_t size, reserved;
};
static_assert(sizeof(PlaygoInit) == 16);
class PlaygoSpeedPolicy {
    int value = 1;
    std::int64_t suspended_at = 0;

public:
    int set(int speed, std::int64_t now) {
        if (speed < 0 || speed > 2) return playgo_error(4);
        value = speed;
        suspended_at = now;
        return 0;
    }
    int get(std::int64_t now) {
        if (value == 0 && now - suspended_at > 30000) value = 1;
        return value;
    }
};
inline std::int64_t playgo_now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
class PlaygoMetadata {
    std::mutex mutex;
    bool loaded = false, initialized = false, opened = false;
    unsigned chunk_count = 0;
    PlaygoSpeedPolicy speed;

public:
    int load(const std::filesystem::path& path) {
        std::lock_guard<std::mutex> lock(mutex);
        if (loaded) return 0;
        const auto size = std::filesystem::file_size(path);
        if (size > 16 * 1024 * 1024) throw std::runtime_error("PlayGo metadata too large");
        std::ifstream in(path, std::ios::binary);
        std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
        if (!in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)))
            throw std::runtime_error("PlayGo metadata read failed");
        chunk_count = parse_playgo_metadata(bytes);
        loaded = true;
        std::cout << "{\"scope\":\"playgo_metadata_loaded\",\"bytes\":" << size << ",\"chunks\":" << chunk_count
                  << ",\"installation_status_available\":false}\n"
                  << std::flush;
        return 0;
    }
    int initialize(const PlaygoInit* param) {
        if (!param) return playgo_error(10);
        PlaygoInit local{};
        SIZE_T n = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), param, &local, sizeof(local), &n) || n != sizeof(local))
            throw std::runtime_error("PlayGo init params unreadable");
        if (!local.buffer) return playgo_error(10);
        if (local.size < 0x200000) return playgo_error(11);
        // The API lends a work buffer until termination. Validate the whole writable range.
        require_time_output(local.buffer, local.size);
        std::lock_guard<std::mutex> lock(mutex);
        if (initialized) return playgo_error(6);
        if (!loaded) throw std::runtime_error("PlayGo module not loaded");
        speed = PlaygoSpeedPolicy{};
        initialized = true;
        return 0;
    }
    int open(std::uint32_t* output, const void* param) {
        if (!output) return playgo_error(10);
        if (param) return playgo_error(4);
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) return playgo_error(5);
        require_time_output(output, 4);
        opened = true;
        *output = 1;
        return 0;
    }
    int chunk_ids(std::uint32_t handle, std::uint16_t* output, std::uint32_t count, std::uint32_t* entries) {
        if (handle != 1) return playgo_error(9);
        if (!entries) return playgo_error(10);
        if (output && !count) return playgo_error(11);
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) return playgo_error(5);
        if (!opened) return playgo_error(9);
        const auto written = output ? (std::min)(count, chunk_count) : chunk_count;
        require_time_output(entries, 4);
        if (output) require_time_output(output, written * 2ULL);
        if (output)
            for (unsigned i = 0; i < written; ++i) output[i] = static_cast<std::uint16_t>(i);
        *entries = written;
        return 0;
    }
    int get_todo(std::uint32_t handle, void* entries, std::uint32_t count, std::uint32_t* written) {
        if (handle != 1) return playgo_error(9);
        if (!entries || !written) return playgo_error(10);
        if (!count) return playgo_error(11);
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) return playgo_error(5);
        if (!opened) return playgo_error(9);
        require_time_output(written, 4);
        *written = 0; // offline / complete installation: nothing left to install
        return 0;
    }
    int set_todo(std::uint32_t handle, const void* entries, std::uint32_t count) {
        if (handle != 1) return playgo_error(9);
        if (!entries) return playgo_error(10);
        if (!count) return playgo_error(11);
        if (count > 1000) throw std::runtime_error("PlayGo todo count outside metadata domain");
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) return playgo_error(5);
        if (!opened) return playgo_error(9);
        return 0;
    }
    int get_locus(std::uint32_t handle, const std::uint16_t* ids, std::uint32_t count, std::int8_t* output) {
        if (handle != 1) return playgo_error(9);
        if (!ids || !output) return playgo_error(10);
        if (!count) return playgo_error(11);
        if (count > 1000) throw std::runtime_error("PlayGo locus count outside metadata domain");
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) return playgo_error(5);
        if (!opened) return playgo_error(9);
        require_time_output(output, count);
        std::vector<std::uint16_t> copy(count);
        SIZE_T n = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), ids, copy.data(), count * 2ULL, &n) || n != count * 2ULL)
            throw std::runtime_error("PlayGo locus IDs unreadable");
        for (std::uint32_t i = 0; i < count; ++i) {
            if (copy[i] < chunk_count) {
                output[i] = 3; // LocalFast: retail dump is fully local
            } else {
                output[i] = 0; // NotDownloaded
                return playgo_error(12);
            }
        }
        return 0;
    }
    int get_speed(std::uint32_t handle, std::int32_t* output) {
        if (handle != 1) return playgo_error(9);
        if (!output) return playgo_error(10);
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) return playgo_error(5);
        require_time_output(output, 4);
        *output = speed.get(playgo_now());
        return 0;
    }
    int set_speed(std::uint32_t handle, int value) {
        if (handle != 1) return playgo_error(9);
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) return playgo_error(5);
        return speed.set(value, playgo_now());
    }
    int close(std::uint32_t handle) {
        std::lock_guard<std::mutex> lock(mutex);
        if (handle != 1) return playgo_error(9);
        if (!initialized) return playgo_error(5);
        opened = false;
        return 0;
    }
    int terminate() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) return playgo_error(5);
        initialized = false;
        opened = false;
        return 0;
    }
};
inline PlaygoMetadata playgo_metadata;
inline void playgo_failure(const char* nid, std::uint64_t caller, const std::exception& e) {
    std::fprintf(stderr, "NID=%s library=libScePlayGo caller=0x%llx unsupported: %s\n", nid,
                 static_cast<unsigned long long>(caller), e.what());
    std::fflush(stderr);
    ExitProcess(4);
}
extern "C" int ctr_playgo_init(const PlaygoInit* p, std::uint64_t caller) {
    try {
        return playgo_metadata.initialize(p);
    } catch (const std::exception& e) {
        playgo_failure("ts6GlZOKRrE", caller, e);
        return 0;
    }
}
extern "C" int ctr_playgo_open(std::uint32_t* p, const void* param, std::uint64_t caller) {
    try {
        return playgo_metadata.open(p, param);
    } catch (const std::exception& e) {
        playgo_failure("M1Gma1ocrGE", caller, e);
        return 0;
    }
}
extern "C" int ctr_playgo_chunks(std::uint32_t handle, std::uint16_t* p, std::uint32_t n, std::uint32_t* count,
                                 std::uint64_t caller) {
    try {
        return playgo_metadata.chunk_ids(handle, p, n, count);
    } catch (const std::exception& e) {
        playgo_failure("73fF1MFU8hA", caller, e);
        return 0;
    }
}
extern "C" int ctr_playgo_get_todo(std::uint32_t handle, void* entries, std::uint32_t count, std::uint32_t* written,
                                   std::uint64_t caller) {
    try {
        return playgo_metadata.get_todo(handle, entries, count, written);
    } catch (const std::exception& e) {
        playgo_failure("Nn7zKwnA5q0", caller, e);
        return 0;
    }
}
extern "C" void ctr_sysv_playgo_get_todo();
extern "C" int ctr_playgo_todo(std::uint32_t handle, const void* entries, std::uint32_t count, std::uint64_t caller) {
    try {
        return playgo_metadata.set_todo(handle, entries, count);
    } catch (const std::exception& e) {
        playgo_failure("gUPGiOQ1tmQ", caller, e);
        return 0;
    }
}
extern "C" void ctr_sysv_playgo_todo();
extern "C" int ctr_playgo_locus(std::uint32_t handle, const std::uint16_t* ids, std::uint32_t count,
                                std::int8_t* output, std::uint64_t caller) {
    try {
        return playgo_metadata.get_locus(handle, ids, count, output);
    } catch (const std::exception& e) {
        playgo_failure("uWIYLFkkwqk", caller, e);
        return 0;
    }
}
extern "C" void ctr_sysv_playgo_locus();
extern "C" int ctr_playgo_get_speed(std::uint32_t handle, std::int32_t* p, std::uint64_t caller) {
    try {
        return playgo_metadata.get_speed(handle, p);
    } catch (const std::exception& e) {
        playgo_failure("rvBSfTimejE", caller, e);
        return 0;
    }
}
extern "C" int ctr_playgo_set_speed(std::uint32_t handle, int value, std::uint64_t caller) {
    try {
        return playgo_metadata.set_speed(handle, value);
    } catch (const std::exception& e) {
        playgo_failure("4AAcTU9R3XM", caller, e);
        return 0;
    }
}
extern "C" void ctr_sysv_playgo_get_speed();
extern "C" void ctr_sysv_playgo_set_speed();
extern "C" int ctr_playgo_close(std::uint32_t handle, std::uint64_t caller) {
    try {
        return playgo_metadata.close(handle);
    } catch (const std::exception& e) {
        playgo_failure("Uco1I0dlDi8", caller, e);
        return 0;
    }
}
extern "C" int ctr_playgo_term(std::uint64_t caller) {
    try {
        return playgo_metadata.terminate();
    } catch (const std::exception& e) {
        playgo_failure("MPe0EeBGM-E", caller, e);
        return 0;
    }
}
extern "C" void ctr_sysv_playgo_init();
extern "C" void ctr_sysv_playgo_open();
extern "C" void ctr_sysv_playgo_chunks();
extern "C" void ctr_sysv_playgo_close();
extern "C" void ctr_sysv_playgo_term();
}
