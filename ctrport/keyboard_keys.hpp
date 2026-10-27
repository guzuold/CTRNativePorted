// SPDX-License-Identifier: GPL-2.0-only
// The keyboard layout shared by the runtime (pad_backend.hpp, KeyboardMap) and the launcher's
// KEYBOARD section (launcher_keyboard.hpp): the 24 pad inputs, their default keys and the key
// names of the CTR_KEYBOARD_MAP file ("input=Key,Key" lines).
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <string>

namespace ctrport::keyboard_keys {
constexpr int count = 24;
constexpr const char* names[count] = {"cross",     "circle",     "square",  "triangle",  "l1",        "r1",
                                      "l2",        "r2",         "l3",      "r3",        "options",   "touchpad",
                                      "up",        "down",       "left",    "right",     "move_up",   "move_down",
                                      "move_left", "move_right", "look_up", "look_down", "look_left", "look_right"};
// Default keys as SacredFire's runtime_pad.c (GPL-2.0-or-later) so players of both ports find the
// same layout: WASD left stick, arrow keys right stick, Space Cross, LShift Circle, E Square,
// Q Triangle, 1 L1, 3 R1, R L2, F R2, Z L3, C R3, Enter Options, Tab touch pad, IJKL d-pad.
constexpr int defaults[count] = {VK_SPACE, VK_LSHIFT, 'E',       'Q',    '1',   '3',     'R',     'F',
                                 'Z',      'C',       VK_RETURN, VK_TAB, 'I',   'K',     'J',     'L',
                                 'W',      'S',       'A',       'D',    VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT};

struct Named {
    const char* name;
    int code;
};
// The first name of a code is the one written back.
constexpr Named named[] = {{"space", VK_SPACE},
                           {"enter", VK_RETURN},
                           {"return", VK_RETURN},
                           {"tab", VK_TAB},
                           {"backspace", VK_BACK},
                           {"escape", VK_ESCAPE},
                           {"shift", VK_SHIFT},
                           {"lshift", VK_LSHIFT},
                           {"rshift", VK_RSHIFT},
                           {"ctrl", VK_CONTROL},
                           {"lctrl", VK_LCONTROL},
                           {"rctrl", VK_RCONTROL},
                           {"alt", VK_MENU},
                           {"lalt", VK_LMENU},
                           {"ralt", VK_RMENU},
                           {"up", VK_UP},
                           {"down", VK_DOWN},
                           {"left", VK_LEFT},
                           {"right", VK_RIGHT},
                           {"insert", VK_INSERT},
                           {"delete", VK_DELETE},
                           {"home", VK_HOME},
                           {"end", VK_END},
                           {"pageup", VK_PRIOR},
                           {"pagedown", VK_NEXT},
                           {"capslock", VK_CAPITAL},
                           // punctuation keys by their US-layout position (the launcher shows the character of the
                           // user's own layout)
                           {"semicolon", VK_OEM_1},
                           {"equals", VK_OEM_PLUS},
                           {"comma", VK_OEM_COMMA},
                           {"minus", VK_OEM_MINUS},
                           {"period", VK_OEM_PERIOD},
                           {"slash", VK_OEM_2},
                           {"backquote", VK_OEM_3},
                           {"lbracket", VK_OEM_4},
                           {"backslash", VK_OEM_5},
                           {"rbracket", VK_OEM_6},
                           {"quote", VK_OEM_7},
                           {"oem102", VK_OEM_102},
                           {"numpadmultiply", VK_MULTIPLY},
                           {"numpadadd", VK_ADD},
                           {"numpadsubtract", VK_SUBTRACT},
                           {"numpaddecimal", VK_DECIMAL},
                           {"numpaddivide", VK_DIVIDE}};

// Virtual-key code of a key name (letters, digits, F1-F24, numpad digits, named keys), 0 when unknown.
inline int key_code(std::string name) {
    for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (name.size() == 1 && std::isalnum(static_cast<unsigned char>(name[0])))
        return std::toupper(static_cast<unsigned char>(name[0]));
    if (name.size() >= 2 && name.size() <= 3 && name[0] == 'f' && std::all_of(name.begin() + 1, name.end(), [](char c) {
            return std::isdigit(static_cast<unsigned char>(c)) != 0;
        })) {
        const int n = std::stoi(name.substr(1));
        if (n >= 1 && n <= 24) return VK_F1 + n - 1;
    }
    if (name.size() == 7 && name.rfind("numpad", 0) == 0 && std::isdigit(static_cast<unsigned char>(name[6])))
        return VK_NUMPAD0 + (name[6] - '0');
    for (const auto& n : named)
        if (name == n.name) return n.code;
    return 0;
}
// The file name of a code, empty when the file cannot name it.
inline std::string key_name(int code) {
    if ((code >= 'A' && code <= 'Z') || (code >= '0' && code <= '9')) return std::string(1, static_cast<char>(code));
    if (code >= VK_F1 && code <= VK_F24) return "F" + std::to_string(code - VK_F1 + 1);
    if (code >= VK_NUMPAD0 && code <= VK_NUMPAD9) return "numpad" + std::to_string(code - VK_NUMPAD0);
    for (const auto& n : named)
        if (n.code == code) return n.name;
    return {};
}
} // namespace ctrport::keyboard_keys
