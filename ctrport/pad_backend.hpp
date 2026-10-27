// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "settings/hot_trace.hpp"
#include "kernel_time.hpp"
#include "settings/native_graphics.hpp"
#include <Xinput.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include "keyboard_keys.hpp"
#include "pad_test_input.hpp"
#include "user_service.hpp"
extern "C" std::uint64_t ctr_kernel_process_time();
namespace ctrport {
// Public PadData layout/button constants: pinned shadPS4 pad.h/pad.cpp,
// e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
// Windows axes map their full signed range to unsigned PS4 sticks; Y is inverted.
// XInput has no motion/touch sensor; their fields remain at the neutral values.
inline std::array<unsigned char, 120> pad_connected_sample(const XINPUT_GAMEPAD& g, unsigned char connections,
                                                           uint64_t time) {
    std::array<unsigned char, 120> out{};
    uint32_t buttons = 0;
    const std::pair<DWORD, uint32_t> map[] = {{XINPUT_GAMEPAD_LEFT_THUMB, 2},
                                              {XINPUT_GAMEPAD_RIGHT_THUMB, 4},
                                              {XINPUT_GAMEPAD_START, 8},
                                              {XINPUT_GAMEPAD_DPAD_UP, 0x10},
                                              {XINPUT_GAMEPAD_DPAD_RIGHT, 0x20},
                                              {XINPUT_GAMEPAD_DPAD_DOWN, 0x40},
                                              {XINPUT_GAMEPAD_DPAD_LEFT, 0x80},
                                              {XINPUT_GAMEPAD_LEFT_SHOULDER, 0x400},
                                              {XINPUT_GAMEPAD_RIGHT_SHOULDER, 0x800},
                                              {XINPUT_GAMEPAD_Y, 0x1000},
                                              {XINPUT_GAMEPAD_B, 0x2000},
                                              {XINPUT_GAMEPAD_A, 0x4000},
                                              {XINPUT_GAMEPAD_X, 0x8000},
                                              {XINPUT_GAMEPAD_BACK, 0x100000}};
    for (const auto& item : map)
        if (g.wButtons & item.first) buttons |= item.second;
    if (g.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) buttons |= 0x100;
    if (g.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) buttons |= 0x200;
    std::memcpy(out.data(), &buttons, 4);
    const auto axis = [](SHORT value, bool inverted) {
        const unsigned shifted = static_cast<unsigned>(int(value) + 32768);
        return static_cast<unsigned char>((inverted ? 65535 - shifted : shifted) >> 8);
    };
    out[4] = axis(g.sThumbLX, false);
    out[5] = axis(g.sThumbLY, true);
    out[6] = axis(g.sThumbRX, false);
    out[7] = axis(g.sThumbRY, true);
    out[8] = g.bLeftTrigger;
    out[9] = g.bRightTrigger;
    const float identity = 1;
    std::memcpy(out.data() + 24, &identity, 4);
    out[76] = 1;
    std::memcpy(out.data() + 80, &time, 8);
    out[104] = connections;
    return out;
}

// Buttons are OR-ed, triggers take the larger value, each stick the larger deflection.
inline void merge_gamepad(XINPUT_GAMEPAD& out, const XINPUT_GAMEPAD& g) {
    const auto stick = [](SHORT& x, SHORT& y, SHORT nx, SHORT ny) {
        if (static_cast<long long>(nx) * nx + static_cast<long long>(ny) * ny >
            static_cast<long long>(x) * x + static_cast<long long>(y) * y) {
            x = nx;
            y = ny;
        }
    };
    out.wButtons |= g.wButtons;
    out.bLeftTrigger = (std::max)(out.bLeftTrigger, g.bLeftTrigger);
    out.bRightTrigger = (std::max)(out.bRightTrigger, g.bRightTrigger);
    stick(out.sThumbLX, out.sThumbLY, g.sThumbLX, g.sThumbLY);
    stick(out.sThumbRX, out.sThumbRY, g.sThumbRX, g.sThumbRY);
}
// Without split screen every connected XInput slot feeds the one local user. Observed case
// (2026-10-16): a Steam Controller emulated as Xbox 360 by Steamless Controller showed slots 0
// and 1 connected on one PC, with no input reaching the game while only slot 0 was read.
inline DWORD merge_xinput(const std::array<DWORD, 4>& results, const std::array<XINPUT_STATE, 4>& states,
                          XINPUT_GAMEPAD& out) {
    out = {};
    DWORD merged = ERROR_DEVICE_NOT_CONNECTED;
    for (std::size_t i = 0; i < results.size(); ++i) {
        if (results[i] != ERROR_SUCCESS) continue;
        merged = ERROR_SUCCESS;
        merge_gamepad(out, states[i].Gamepad);
    }
    return merged;
}

// Keyboard as a controller, in the XINPUT_GAMEPAD layout of the other devices. Inputs, default
// keys and key names: keyboard_keys.hpp. CTR_KEYBOARD_MAP names a file of "input=Key,Key" lines
// that replace an input's keys (the launcher's KEYBOARD section writes it).
class KeyboardMap {
public:
    enum Input {
        CROSS,
        CIRCLE,
        SQUARE,
        TRIANGLE,
        L1,
        R1,
        L2,
        R2,
        L3,
        R3,
        OPTIONS,
        TOUCHPAD,
        UP,
        DOWN,
        LEFT,
        RIGHT,
        MOVE_UP,
        MOVE_DOWN,
        MOVE_LEFT,
        MOVE_RIGHT,
        LOOK_UP,
        LOOK_DOWN,
        LOOK_LEFT,
        LOOK_RIGHT,
        COUNT
    };
    static_assert(COUNT == keyboard_keys::count);
    static constexpr const auto& names = keyboard_keys::names;
    std::array<std::vector<int>, COUNT> keys;
    KeyboardMap() {
        for (int i = 0; i < COUNT; ++i) keys[i] = {keyboard_keys::defaults[i]};
    }
    static int key_code(const std::string& name) { return keyboard_keys::key_code(name); }
    // Applies "input=Key,Key" lines; returns the lines it could not use (logged by the caller).
    std::vector<std::string> load(std::istream& in) {
        std::vector<std::string> rejected;
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            const auto eq = line.find('=');
            int input = -1;
            if (eq != std::string::npos)
                for (int i = 0; i < COUNT; ++i)
                    if (line.compare(0, eq, names[i]) == 0 && std::strlen(names[i]) == eq) input = i;
            if (input < 0) {
                rejected.push_back(line);
                continue;
            }
            std::vector<int> codes;
            std::stringstream list(line.substr(eq + 1));
            std::string key;
            bool bad = false;
            while (std::getline(list, key, ',')) {
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                if (key.empty()) continue;
                if (const int code = key_code(key))
                    codes.push_back(code);
                else
                    bad = true;
            }
            if (bad) {
                rejected.push_back(line);
                continue;
            }
            keys[input] = codes;
        }
        return rejected;
    }
    // The pad the held keys describe; a held direction key moves its stick all the way.
    template <class Down> XINPUT_GAMEPAD state(Down down) const {
        const auto held = [&](int input) {
            for (int code : keys[input])
                if (down(code)) return true;
            return false;
        };
        XINPUT_GAMEPAD g{};
        const std::pair<int, int> buttons[] = {
            {CROSS, XINPUT_GAMEPAD_A},        {CIRCLE, XINPUT_GAMEPAD_B},         {SQUARE, XINPUT_GAMEPAD_X},
            {TRIANGLE, XINPUT_GAMEPAD_Y},     {L1, XINPUT_GAMEPAD_LEFT_SHOULDER}, {R1, XINPUT_GAMEPAD_RIGHT_SHOULDER},
            {L3, XINPUT_GAMEPAD_LEFT_THUMB},  {R3, XINPUT_GAMEPAD_RIGHT_THUMB},   {OPTIONS, XINPUT_GAMEPAD_START},
            {TOUCHPAD, XINPUT_GAMEPAD_BACK},  {UP, XINPUT_GAMEPAD_DPAD_UP},       {DOWN, XINPUT_GAMEPAD_DPAD_DOWN},
            {LEFT, XINPUT_GAMEPAD_DPAD_LEFT}, {RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT}};
        for (const auto& [input, bit] : buttons)
            if (held(input)) g.wButtons = static_cast<WORD>(g.wButtons | bit);
        if (held(L2)) g.bLeftTrigger = 255;
        if (held(R2)) g.bRightTrigger = 255;
        const auto axis = [&](int negative, int positive) {
            return static_cast<SHORT>((held(negative) ? -32768 : 0) + (held(positive) ? 32767 : 0));
        };
        g.sThumbLX = axis(MOVE_LEFT, MOVE_RIGHT);
        g.sThumbLY = axis(MOVE_DOWN, MOVE_UP);
        g.sThumbRX = axis(LOOK_LEFT, LOOK_RIGHT);
        g.sThumbRY = axis(LOOK_DOWN, LOOK_UP);
        return g;
    }
};

// One input device: XInput slot 0..3 (ids 0..3), the keyboard (0x200), or a PlayStation
// controller read by ctr_gpu.dll (high bit | SDL joystick id; SDL ids start at 1, so the bare
// high bit is the merged snapshot of an older DLL without per-controller entries).
struct PadDevice {
    std::uint32_t id;
    XINPUT_GAMEPAD pad;
};
constexpr std::uint32_t keyboard_device = 0x200, playstation_flag = 0x80000000u,
                        playstation_merged_device = playstation_flag;
inline bool is_playstation_device(std::uint32_t id) {
    return (id & playstation_flag) != 0;
}
inline std::uint32_t sdl_joystick_id(std::uint32_t id) {
    return id & ~playstation_flag;
}

// Split screen: which device plays on which controller port. Port 0 is the primary user. While
// it has no device, every unassigned device drives it; the first one that is used takes it. A
// further device then joins as the next port by holding a button for join_hold_ms, as a PS4
// player presses the PS button to log in. A device that only mirrors an assigned one (one pad
// seen through two XInput slots, see merge_xinput) never joins: its buttons always match.
// A device that disconnects leaves its port and that port's user logs out.
// The launcher can also fix a device to a port (CTR_PAD_PLAYERS): that port takes only that
// device, logs in while it is connected, and other devices never join it; a port set to
// "none" stays empty. Ports left on "auto" keep the rules above.
class PortAssigner {
    static constexpr std::uint64_t join_hold_ms = 300;
    std::optional<std::uint32_t> candidate;
    std::uint64_t candidate_since = 0;
    static bool pressed(const XINPUT_GAMEPAD& g) {
        return g.wButtons || g.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ||
               g.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    }
    static bool active(const XINPUT_GAMEPAD& g) {
        const auto pushed = [](SHORT x, SHORT y) {
            return static_cast<long long>(x) * x + static_cast<long long>(y) * y > 16000LL * 16000;
        };
        return pressed(g) || pushed(g.sThumbLX, g.sThumbLY) || pushed(g.sThumbRX, g.sThumbRY);
    }
    static const PadDevice* find(const std::vector<PadDevice>& devices, std::uint32_t id) {
        for (const auto& d : devices)
            if (d.id == id) return &d;
        return nullptr;
    }

public:
    struct Change {
        int port;
        std::uint32_t device;
        bool login;
    };
    std::array<std::optional<std::uint32_t>, 4> owner, fixed;
    std::array<bool, 4> blocked{};
    bool assigned(std::uint32_t id) const {
        for (const auto& o : owner)
            if (o == id) return true;
        for (const auto& f : fixed)
            if (f == id) return true;
        return false;
    }
    std::vector<Change> update(const std::vector<PadDevice>& devices, std::uint64_t now_ms) {
        std::vector<Change> changes;
        for (int port = 0; port < 4; ++port) {
            if (fixed[port]) {
                const bool present = find(devices, *fixed[port]) != nullptr;
                if (present && owner[port] != fixed[port]) {
                    if (!owner[port]) changes.push_back({port, *fixed[port], true});
                    owner[port] = fixed[port];
                } else if (!present && owner[port]) {
                    changes.push_back({port, *owner[port], false});
                    owner[port].reset();
                }
            } else if (owner[port] && (!find(devices, *owner[port]) || blocked[port])) {
                changes.push_back({port, *owner[port], false});
                owner[port].reset();
            }
        }
        if (!owner[0] && !fixed[0]) {
            for (const auto& d : devices)
                if (!assigned(d.id) && active(d.pad)) {
                    owner[0] = d.id;
                    changes.push_back({0, d.id, true});
                    break;
                }
            candidate.reset();
            return changes;
        }
        const auto mirrors = [&](const PadDevice& d) {
            for (const auto& o : owner)
                if (o)
                    if (const auto* a = find(devices, *o))
                        if (a->pad.wButtons == d.pad.wButtons && pressed(a->pad) == pressed(d.pad)) return true;
            return false;
        };
        const auto eligible = [&](const PadDevice& d) { return !assigned(d.id) && pressed(d.pad) && !mirrors(d); };
        if (candidate) {
            const auto* d = find(devices, *candidate);
            if (!d || !eligible(*d))
                candidate.reset();
            else if (now_ms - candidate_since >= join_hold_ms) {
                for (int port = 1; port < 4; ++port)
                    if (!owner[port] && !fixed[port] && !blocked[port]) {
                        owner[port] = d->id;
                        changes.push_back({port, d->id, true});
                        break;
                    }
                candidate.reset();
                return changes;
            }
        }
        if (!candidate)
            for (const auto& d : devices)
                if (eligible(d)) {
                    candidate = d.id;
                    candidate_since = now_ms;
                    break;
                }
        return changes;
    }
    // The pad a port reads: its device, or for port 0 without one every unassigned device.
    bool state(int port, const std::vector<PadDevice>& devices, XINPUT_GAMEPAD& out) const {
        out = {};
        if (owner[port]) {
            if (const auto* d = find(devices, *owner[port])) {
                out = d->pad;
                return true;
            }
            return false;
        }
        if (port || fixed[0]) return false;
        bool any = false;
        for (const auto& d : devices)
            if (!assigned(d.id)) {
                merge_gamepad(out, d.pad);
                any = true;
            }
        return any;
    }
    // Devices whose rumble and light bar belong to a port (same rule as state).
    std::vector<std::uint32_t> devices_of(int port, const std::vector<PadDevice>& devices) const {
        std::vector<std::uint32_t> ids;
        if (owner[port]) {
            ids.push_back(*owner[port]);
            return ids;
        }
        if (!port && !fixed[0])
            for (const auto& d : devices)
                if (!assigned(d.id)) ids.push_back(d.id);
        return ids;
    }
};
inline std::string pad_device_name(std::uint32_t id) {
    if (id == keyboard_device) return "keyboard";
    if (id == playstation_merged_device) return "playstation";
    if (is_playstation_device(id)) return "playstation" + std::to_string(sdl_joystick_id(id));
    return "xinput" + std::to_string(id);
}

// New Windows backend, not a compatibility stub. Public PS4 Init lifecycle:
// pinned shadPS4 pad.cpp e3ce810f3a653f43ac64ebab63023de281a4103a.
// XInput API/types from installed Windows SDK. No fabricated connected device, motion data,
// PS4 read/open behavior or game input state. Up to four local users each open a port
// (CTR_SPLIT_SCREEN=1); without split screen every device drives the one primary user.
// The loaded renderer library, which reads PlayStation controllers: ctr_gpu.dll (Vulkan) or
// ctr_d3d.dll (Direct3D 11).
inline HMODULE renderer_library() {
    if (auto library = GetModuleHandleW(L"ctr_gpu.dll")) return library;
    return GetModuleHandleW(L"ctr_d3d.dll");
}
class PadBackend {
    using GetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
    HMODULE module = nullptr;
    GetState get_state = nullptr;
    std::mutex mutex;
    struct Port {
        int user;
        int index;
        int handle;
        bool connected = false;
        unsigned char connections = 0;
    };
    std::vector<Port> ports;
    int next_handle = 1;
    std::array<DWORD, 4> status{};
    std::vector<PadDevice> devices;
    PortAssigner assigner;
    bool split = false, keyboard = true;
    KeyboardMap keys;
    // CTR_PAD_PLAYERS: four choices separated by '|', for players 1..4: "auto", "none",
    // "keyboard", "xinput:N" (slot 0..3) or "hid:<device path>" (a controller read by ctr_gpu).
    std::array<std::string, 4> choices{"auto", "auto", "auto", "auto"};
    std::map<std::uint32_t, std::string> device_paths;
    struct PlayStationApi {
        int (*merged)(XINPUT_GAMEPAD*) = nullptr;
        int (*list)(void*, int) = nullptr;
        void (*rumble_all)(std::uint16_t, std::uint16_t) = nullptr;
        void (*rumble_one)(std::uint32_t, std::uint16_t, std::uint16_t) = nullptr;
        void (*light)(std::uint32_t, std::uint8_t, std::uint8_t, std::uint8_t) = nullptr;
        int (*path)(std::uint32_t, char*, int) = nullptr;
    };
    // The renderer library reads PlayStation controllers through SDL's HID drivers in the XINPUT_GAMEPAD
    // layout (ctrport/gpu/ctr_gpu.h). Looked up until the DLL is loaded.
    static const PlayStationApi& playstation() {
        static PlayStationApi api;
        if (!api.merged)
            if (auto library = renderer_library()) {
                api.list = reinterpret_cast<int (*)(void*, int)>(GetProcAddress(library, "ctr_gpu_gamepad_list"));
                api.rumble_all = reinterpret_cast<void (*)(std::uint16_t, std::uint16_t)>(
                    GetProcAddress(library, "ctr_gpu_gamepad_rumble"));
                api.rumble_one = reinterpret_cast<void (*)(std::uint32_t, std::uint16_t, std::uint16_t)>(
                    GetProcAddress(library, "ctr_gpu_gamepad_rumble_one"));
                api.light = reinterpret_cast<void (*)(std::uint32_t, std::uint8_t, std::uint8_t, std::uint8_t)>(
                    GetProcAddress(library, "ctr_gpu_gamepad_light"));
                api.path = reinterpret_cast<int (*)(std::uint32_t, char*, int)>(
                    GetProcAddress(library, "ctr_gpu_gamepad_path"));
                api.merged =
                    reinterpret_cast<int (*)(XINPUT_GAMEPAD*)>(GetProcAddress(library, "ctr_gpu_gamepad_state"));
            }
        return api;
    }
    // Keys count only while one of this process's windows is in front.
    static bool focused() {
        DWORD process = 0;
        const HWND foreground = GetForegroundWindow();
        return foreground && GetWindowThreadProcessId(foreground, &process) && process == GetCurrentProcessId();
    }
    // Polls every device (caller holds the mutex); in split screen also assigns ports and logs
    // the matching local users in and out.
    void poll(const char* failure) {
        devices.clear();
        for (DWORD i = 0; i < 4; ++i) {
            XINPUT_STATE state{};
            const DWORD result = get_state(i, &state);
            if (result != ERROR_SUCCESS && result != ERROR_DEVICE_NOT_CONNECTED)
                throw std::runtime_error(std::string(failure) + " status=" + std::to_string(result));
            status[i] = result;
            if (result == ERROR_SUCCESS) devices.push_back({i, state.Gamepad});
        }
        const auto& ps = playstation();
        if (ps.list) {
            struct Entry {
                std::uint32_t id;
                XINPUT_GAMEPAD pad;
            };
            static_assert(sizeof(Entry) == 16);
            std::array<Entry, 8> entries{};
            const int count = ps.list(entries.data(), static_cast<int>(entries.size()));
            for (int i = 0; i < count; ++i) devices.push_back({playstation_flag | entries[i].id, entries[i].pad});
        } else if (ps.merged) {
            XINPUT_GAMEPAD pad{};
            if (ps.merged(&pad) > 0) devices.push_back({playstation_merged_device, pad});
        }
        if (keyboard) {
            const bool front = focused();
            devices.push_back(
                {keyboard_device, front ? keys.state([](int code) { return (GetAsyncKeyState(code) & 0x8000) != 0; })
                                        : XINPUT_GAMEPAD{}});
        }
        resolve_choices();
        if (!split) return;
        for (const auto& change : assigner.update(devices, GetTickCount64())) {
            std::printf("{\"scope\":\"split_screen_port\",\"port\":%d,\"device\":\"%s\",\"assigned\":%s}\n",
                        change.port, pad_device_name(change.device).c_str(), change.login ? "true" : "false");
            std::fflush(stdout);
            if (change.port && user_service.set_extra_user(change.port, change.login)) {
                std::printf("{\"scope\":\"split_screen_user\",\"user\":%d,\"login\":%s}\n",
                            UserService::extra_user_id(change.port), change.login ? "true" : "false");
                std::fflush(stdout);
            }
        }
    }
    // The device each launcher choice names right now (a PlayStation controller's SDL id changes
    // when it reconnects, so paths are matched again at every poll).
    void resolve_choices() {
        const auto& ps = playstation();
        for (int port = 0; port < 4; ++port) {
            const std::string& choice = choices[port];
            std::optional<std::uint32_t> device;
            if (choice == "keyboard")
                device = keyboard_device;
            else if (choice.rfind("xinput:", 0) == 0)
                device = static_cast<std::uint32_t>(std::atoi(choice.c_str() + 7) & 3);
            else if (choice.rfind("hid:", 0) == 0) {
                device = 0x7fffffffu; // not connected
                for (const auto& d : devices)
                    if (is_playstation_device(d.id) && d.id != playstation_merged_device) {
                        auto known = device_paths.find(d.id);
                        if (known == device_paths.end()) {
                            char path[1024]{};
                            if (ps.path) ps.path(sdl_joystick_id(d.id), path, sizeof(path));
                            known = device_paths.emplace(d.id, path).first;
                        }
                        if (!known->second.empty() && _stricmp(known->second.c_str(), choice.c_str() + 4) == 0) {
                            device = d.id;
                            break;
                        }
                    }
            }
            assigner.fixed[port] = device;
            assigner.blocked[port] = port && choice == "none";
        }
    }
    // The pad of a controller port after polling.
    bool port_state(int index, XINPUT_GAMEPAD& out) const {
        if (split) return assigner.state(index, devices, out);
        if (assigner.fixed[0]) {
            out = {};
            for (const auto& d : devices)
                if (d.id == *assigner.fixed[0]) {
                    out = d.pad;
                    return true;
                }
            return false;
        }
        out = {};
        bool any = false;
        for (const auto& d : devices) {
            merge_gamepad(out, d.pad);
            any = true;
        }
        return any;
    }
    std::vector<std::uint32_t> port_devices(int index) const {
        if (split) return assigner.devices_of(index, devices);
        if (assigner.fixed[0]) return {*assigner.fixed[0]};
        std::vector<std::uint32_t> ids;
        for (const auto& d : devices) ids.push_back(d.id);
        return ids;
    }
    std::vector<Port>::iterator find_port(int handle) {
        return std::find_if(ports.begin(), ports.end(), [handle](const Port& p) { return p.handle == handle; });
    }
    static bool environment_flag(const char* name, bool fallback) {
        char value[8]{};
        const DWORD length = GetEnvironmentVariableA(name, value, sizeof(value));
        return length == 1 ? value[0] == '1' : fallback;
    }
    void load_keyboard_map() {
        char path[MAX_PATH]{};
        const DWORD length = GetEnvironmentVariableA("CTR_KEYBOARD_MAP", path, sizeof(path));
        if (!length || length >= sizeof(path)) return;
        std::ifstream file(path);
        if (!file) {
            std::printf("{\"scope\":\"keyboard_map\",\"file\":\"missing\"}\n");
            std::fflush(stdout);
            return;
        }
        const auto rejected = keys.load(file);
        std::printf("{\"scope\":\"keyboard_map\",\"file\":\"loaded\",\"rejected_lines\":%zu}\n", rejected.size());
        for (const auto& line : rejected)
            std::fprintf(stderr, "ctrport: keyboard map line not used: %s\n", line.c_str());
        std::fflush(stdout);
    }

public:
    PadBackend() = default;
    PadBackend(const PadBackend&) = delete;
    ~PadBackend() {
        if (module) FreeLibrary(module);
    }
    int initialize() {
        std::lock_guard<std::mutex> guard(mutex);
        if (module) return 0;
        HMODULE candidate = LoadLibraryExW(L"xinput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!candidate)
            throw std::runtime_error("Pad XInput system backend unavailable Win32=" + std::to_string(GetLastError()));
        FARPROC symbol = GetProcAddress(candidate, "XInputGetState");
        GetState call = nullptr;
        static_assert(sizeof(call) == sizeof(symbol));
        std::memcpy(&call, &symbol, sizeof(call));
        if (!call) {
            FreeLibrary(candidate);
            throw std::runtime_error("Pad XInputGetState unavailable");
        }
        std::array<DWORD, 4> prepared_status{};
        for (DWORD i = 0; i < 4; ++i) {
            XINPUT_STATE prepared{};
            prepared_status[i] = call(i, &prepared);
            if (prepared_status[i] != ERROR_SUCCESS && prepared_status[i] != ERROR_DEVICE_NOT_CONNECTED) {
                const auto error = prepared_status[i];
                FreeLibrary(candidate);
                throw std::runtime_error("Pad XInput polling failed status=" + std::to_string(error));
            }
        }
        module = candidate;
        get_state = call;
        status = prepared_status;
        split = environment_flag("CTR_SPLIT_SCREEN", false);
        keyboard = environment_flag("CTR_KEYBOARD", true);
        if (keyboard) load_keyboard_map();
        char players[4096]{};
        if (const DWORD length = GetEnvironmentVariableA("CTR_PAD_PLAYERS", players, sizeof(players));
            length && length < sizeof(players)) {
            std::stringstream list(players);
            std::string choice;
            for (int port = 0; port < 4 && std::getline(list, choice, '|'); ++port)
                if (!choice.empty()) choices[port] = choice;
            std::printf(
                "{\"scope\":\"pad_players\",\"player1\":\"%s\",\"player2\":\"%s\",\"player3\":\"%s\",\"player4\":\"%s\"}\n",
                choices[0].substr(0, 64).c_str(), choices[1].substr(0, 64).c_str(), choices[2].substr(0, 64).c_str(),
                choices[3].substr(0, 64).c_str());
            std::fflush(stdout);
        }
        return 0;
    }
    // Test configuration of an isolated backend (controls only).
    void configure(bool split_screen, bool use_keyboard) {
        std::lock_guard<std::mutex> guard(mutex);
        split = split_screen;
        keyboard = use_keyboard;
    }
    int open(int user, int type, int index, const void* params) {
        std::lock_guard<std::mutex> guard(mutex);
        if (!module || !get_state) return static_cast<int>(0x80920005U);
        if (user < 0 || user == 255) return static_cast<int>(0x809b0001U);
        const int port_index = user_service.port_of_user(user);
        if (port_index < 0) return static_cast<int>(0x809b0081U);
        if (type != 0 || index != 0 || params)
            throw std::runtime_error("PadOpen only observed standard port0/index0/null parameters implemented");
        for (const auto& port : ports)
            if (port.user == user) return static_cast<int>(0x80920004U);
        poll("PadOpen real XInput poll failed");
        XINPUT_GAMEPAD pad{};
        const bool connected = port_state(port_index, pad);
        const int handle = next_handle++;
        ports.push_back({user, port_index, handle, connected, static_cast<unsigned char>(connected ? 1 : 0)});
        return handle;
    }
    // scePadClose: pinned shadPS4 pad.cpp46-58, an unknown handle is ORBIS_PAD_ERROR_INVALID_HANDLE.
    int close(int handle) {
        std::lock_guard<std::mutex> guard(mutex);
        const auto port = find_port(handle);
        if (port == ports.end()) return static_cast<int>(0x80920003U);
        ports.erase(port);
        return 0;
    }
    // Public disconnected sample: pinned pad.cpp ProcessStates368-374/readState467-470,
    // pad.h OrbisPadData size120, quaternion offset12/w24, connected76.
    // GPL-2.0-or-later upstream e3ce810f3a653f43ac64ebab63023de281a4103a.
    int read_state(int handle, void* output) {
        if (!output) return static_cast<int>(0x80920001U);
        std::lock_guard<std::mutex> guard(mutex);
        auto port = find_port(handle);
        if (port == ports.end()) return static_cast<int>(0x80920003U);
        poll("XInput read failed");
        port = find_port(handle);
        XINPUT_GAMEPAD pad{};
        const bool connected = port_state(port->index, pad);
        std::array<unsigned char, 120> sample{};
        const float identity = 1.0f;
        std::memcpy(sample.data() + 24, &identity, 4);
        if (connected) {
            if (port->index == 0) apply_pad_test_input(pad);
            // Host exit confirmation consumes input; preserve physical connection and time.
            using CaptureQuery = int (*)();
            static CaptureQuery capture = [] {
                auto library = renderer_library();
                return library ? reinterpret_cast<CaptureQuery>(GetProcAddress(library, "ctr_gpu_input_captured"))
                               : nullptr;
            }();
            if (capture && capture()) pad = {};
            if (!port->connected) {
                if (port->connections != 255) ++port->connections;
            }
            sample = pad_connected_sample(pad, port->connections, ctr_kernel_process_time());
        }
        port->connected = connected;
        require_time_output(output, sample.size());
        std::memcpy(output, sample.data(), sample.size());
        return 0;
    }
    // scePadSetVibration (shadPS4 pad.cpp651-664; OrbisPadVibrationParam {u8 largeMotor;
    // u8 smallMotor}): XInput low-frequency (left) motor = large, high-frequency = small.
    int set_vibration(int handle, const void* parameter) {
        std::lock_guard<std::mutex> guard(mutex);
        const auto port = find_port(handle);
        if (port == ports.end()) return static_cast<int>(0x80920003U);
        if (!parameter) return static_cast<int>(0x80920001U);
        unsigned char motors[2]{};
        SIZE_T read = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), parameter, motors, 2, &read) || read != 2)
            return static_cast<int>(0x80920001U);
        using SetState = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);
        static const SetState set_state = [this] {
            FARPROC symbol = module ? GetProcAddress(module, "XInputSetState") : nullptr;
            SetState call = nullptr;
            std::memcpy(&call, &symbol, sizeof(call));
            return call;
        }();
        if (!port->connected) return 0;
        const auto low = static_cast<std::uint16_t>(motors[0] * 257),
                   high = static_cast<std::uint16_t>(motors[1] * 257);
        const auto& ps = playstation();
        for (const auto id : port_devices(port->index)) {
            XINPUT_VIBRATION vibration{low, high};
            if (id < 4) {
                if (set_state) set_state(id, &vibration);
            } else if (id == playstation_merged_device || (is_playstation_device(id) && !ps.rumble_one)) {
                if (ps.rumble_all) ps.rumble_all(low, high);
            } else if (is_playstation_device(id))
                ps.rumble_one(sdl_joystick_id(id), low, high);
        }
        return 0;
    }
    // scePadSetLightBar / scePadResetLightBar: pinned shadPS4 pad.cpp478-496 and 566-586
    // (OrbisPadLightBarParam {u8 r,g,b,reserved}; all three components below 0x0d are
    // ORBIS_PAD_ERROR_INVALID_LIGHTBAR_SETTING; reset shows the user colour of the port: blue,
    // red, green, pink, input/controller.h g_user_colours). Only PlayStation controllers have
    // a light bar; for the other devices of the port the call has no visible effect.
    int set_light_bar(int handle, const void* parameter) {
        std::lock_guard<std::mutex> guard(mutex);
        const auto port = find_port(handle);
        if (port == ports.end()) return static_cast<int>(0x80920003U);
        if (!parameter) return static_cast<int>(0x80920001U);
        unsigned char colour[3]{};
        SIZE_T read = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), parameter, colour, 3, &read) || read != 3)
            return static_cast<int>(0x80920001U);
        if (colour[0] < 0xd && colour[1] < 0xd && colour[2] < 0xd) return static_cast<int>(0x80920006U);
        light(port->index, colour[0], colour[1], colour[2]);
        return 0;
    }
    int reset_light_bar(int handle) {
        std::lock_guard<std::mutex> guard(mutex);
        const auto port = find_port(handle);
        if (port == ports.end()) return static_cast<int>(0x80920003U);
        static constexpr unsigned char colours[4][3] = {{0, 0, 255}, {255, 0, 0}, {0, 255, 0}, {255, 0, 255}};
        const auto& c = colours[port->index];
        light(port->index, c[0], c[1], c[2]);
        return 0;
    }
    void light(int index, unsigned char r, unsigned char g, unsigned char b) {
        const auto& ps = playstation();
        if (!ps.light) return;
        for (const auto id : port_devices(index))
            if (is_playstation_device(id) && id != playstation_merged_device) ps.light(sdl_joystick_id(id), r, g, b);
    }
    // Public 28-byte controller information, backed by the port's actual devices.
    // No touchpad capability is advertised for an XInput standard controller.
    int information(int handle, void* output) {
        if (!output) return static_cast<int>(0x80920001U);
        std::lock_guard<std::mutex> guard(mutex);
        auto port = find_port(handle);
        if (port == ports.end()) return static_cast<int>(0x80920003U);
        poll("XInput information poll failed");
        port = find_port(handle);
        XINPUT_GAMEPAD pad{};
        const bool connected = port_state(port->index, pad);
        if (connected && !port->connected && port->connections != 255) ++port->connections;
        port->connected = connected;
        std::array<unsigned char, 28> info{};
        info[8] = static_cast<unsigned char>(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE >> 8);
        info[9] = static_cast<unsigned char>(XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE >> 8);
        info[11] = port->connections;
        info[12] = port->connected ? 1 : 0;
        require_time_output(output, info.size());
        std::memcpy(output, info.data(), info.size());
        return 0;
    }
    unsigned connected_count() {
        std::lock_guard<std::mutex> guard(mutex);
        if (!module || !get_state) throw std::runtime_error("Pad not initialized");
        unsigned n = 0;
        for (auto result : status)
            if (result == ERROR_SUCCESS) ++n;
        return n;
    }
    bool initialized() {
        std::lock_guard<std::mutex> guard(mutex);
        return module && get_state;
    }
};
inline PadBackend pad_backend;
extern "C" int ctr_pad_init(uint64_t caller) {
    try {
        const int result = pad_backend.initialize();
        std::printf(
            "{\"scope\":\"original_PadInit_return\",\"caller\":%llu,\"result\":%d,\"XInput_backend\":true,\"initial_connected_devices\":%u,\"guest_read_bound\":false}\n",
            static_cast<unsigned long long>(caller), result, pad_backend.connected_count());
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=hv1luiJrqQM library=libScePad caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_pad_init();
extern "C" int ctr_pad_open(int user, int type, int index, const void* params, uint64_t caller) {
    try {
        const int result = pad_backend.open(user, type, index, params);
        std::printf(
            "{\"scope\":\"original_PadOpen_return\",\"caller\":%llu,\"user\":%d,\"type\":%d,\"index\":%d,\"result\":%d,\"connected_devices\":%u}\n",
            static_cast<unsigned long long>(caller), user, type, index, result, pad_backend.connected_count());
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=xk0AcarP3V4 library=libScePad caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_pad_open();
}

namespace ctrport {
extern "C" int ctr_pad_read_state(int handle, void* output, uint64_t caller) {
    settings::native_graphics::apply();
    try {
        const int result = pad_backend.read_state(handle, output);
        if (ctrport::settings::hot_trace()) {
            std::printf("{\"scope\":\"original_PadReadState_return\",\"caller\":%llu,\"result\":%d,\"connected\":%s}\n",
                        static_cast<unsigned long long>(caller), result,
                        (!result && static_cast<unsigned char*>(output)[76]) ? "true" : "false");
            std::fflush(stdout);
        }
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=YndgXqQVV7c library=libScePad caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_pad_read_state();
extern "C" int ctr_pad_set_vibration(int handle, const void* parameter, uint64_t caller) {
    try {
        return pad_backend.set_vibration(handle, parameter);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=yFVnOdGxvZY library=libScePad caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_pad_set_vibration();
extern "C" int ctr_pad_close(int handle, uint64_t caller) {
    try {
        const int result = pad_backend.close(handle);
        std::printf("{\"scope\":\"original_PadClose_return\",\"caller\":%llu,\"handle\":%d,\"result\":%d}\n",
                    static_cast<unsigned long long>(caller), handle, result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=6ncge5+l5Qs library=libScePad caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_pad_close();
extern "C" int ctr_pad_set_light_bar(int handle, const void* parameter, uint64_t caller) {
    try {
        const int result = pad_backend.set_light_bar(handle, parameter);
        std::printf("{\"scope\":\"original_PadSetLightBar_return\",\"caller\":%llu,\"handle\":%d,\"result\":%d}\n",
                    static_cast<unsigned long long>(caller), handle, result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=RR4novUEENY library=libScePad caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_pad_set_light_bar();
extern "C" int ctr_pad_reset_light_bar(int handle, uint64_t caller) {
    try {
        const int result = pad_backend.reset_light_bar(handle);
        std::printf("{\"scope\":\"original_PadResetLightBar_return\",\"caller\":%llu,\"handle\":%d,\"result\":%d}\n",
                    static_cast<unsigned long long>(caller), handle, result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=DscD1i9HX1w library=libScePad caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_pad_reset_light_bar();
}

namespace ctrport {
extern "C" int ctr_pad_information(int handle, void* output, uint64_t caller) {
    try {
        const int result = pad_backend.information(handle, output);
        if (ctrport::settings::hot_trace()) {
            std::printf("{\"scope\":\"original_PadGetControllerInformation_return\",\"caller\":%llu,\"result\":%d}\n",
                        static_cast<unsigned long long>(caller), result);
            std::fflush(stdout);
        }
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=gjP9-KQzoUk library=libScePad caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_pad_information();
}
