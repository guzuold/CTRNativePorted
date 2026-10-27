// SPDX-License-Identifier: GPL-2.0-or-later
// libSceNpTrophy without trophy data. Behaviour follows pinned shadPS4 v0.18.0
// src/core/libraries/np/np_trophy.cpp when no trophy files are installed: contexts and
// handles are real (1-based ids, argument checks and error codes as there), registration
// succeeds, info queries return ORBIS_OK without output and icons ICON_FILE_NOT_FOUND.
// With the trophy file of the user's dump (trophy_data.hpp) the unlock state is kept in
// runtime/user/trophies/<NP communication ID>.txt and reported to the game, an unlocked
// trophy returns TROPHY_ALREADY_UNLOCKED, the platinum unlocks with the last base trophy (as
// shadPS4 np_trophy.cpp), and each unlock shows a notice over the game (trophy_popup.cpp).
// Without the file, unlocks are only recorded in runtime/user/trophies/unlocks.jsonl.
// ShowTrophyList, AbortHandle and CaptureScreenshot are declared compatibility stubs
// (ctrport/compat_stubs.json, tag R8).
#include "../trophy_data.hpp"
#include "trophy_popup.h"
#include "ui_text.h"
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <set>
#include <vector>
#include "ctr_hle.h"
// windows.h (through trophy_data.hpp) defines Win32 codes with two of the names below.
#undef ERROR_INVALID_HANDLE
#undef ERROR_ALREADY_REGISTERED

namespace {
constexpr int OK = 0;
constexpr int ERROR_INVALID_ARGUMENT = static_cast<int>(0x80551604);
constexpr int ERROR_EXCEEDS_MAX = static_cast<int>(0x80551606);
constexpr int ERROR_INVALID_HANDLE = static_cast<int>(0x80551608);
constexpr int ERROR_INVALID_CONTEXT = static_cast<int>(0x80551609);
constexpr int ERROR_INVALID_TROPHY_ID = static_cast<int>(0x8055160A);
constexpr int ERROR_NOT_REGISTERED = static_cast<int>(0x8055160F);
constexpr int ERROR_ALREADY_REGISTERED = static_cast<int>(0x80551610);
constexpr int ERROR_CONTEXT_ALREADY_EXISTS = static_cast<int>(0x80551613);
constexpr int ERROR_ICON_FILE_NOT_FOUND = static_cast<int>(0x80551614);
constexpr int ERROR_TROPHY_ALREADY_UNLOCKED = static_cast<int>(0x8055160C);
constexpr int ERROR_PLATINUM_CANNOT_UNLOCK = static_cast<int>(0x8055160D);
constexpr int NUM_MAX = 128;
constexpr std::size_t MAX_CONTEXTS = 8, MAX_HANDLES = 4;
constexpr const char* LIB = "libSceNpTrophy";

struct Context {
    bool allocated = false, registered = false;
    std::int32_t user_id = 0;
    std::uint32_t service_label = 0;
};
std::mutex mutex;
std::vector<Context> contexts;
std::vector<bool> handles;

// The trophy set of the dump and its unlocks, loaded at the first registration.
struct Trophies {
    bool tried = false, ok = false;
    ctr_trophy::Set set;
    std::filesystem::path file;
    std::map<int, std::int64_t> unlocked;
};
Trophies trophies;
std::string env(const char* name) {
    const char* value = std::getenv(name);
    return value ? value : "";
}
void load_trophies() {
    if (trophies.tried) return;
    trophies.tried = true;
    try {
        const int language = std::atoi(env("CTR_SYSTEM_LANGUAGE").c_str());
        trophies.set = ctr_trophy::load({env("CTR_PATCH_DIR"), env("CTR_BASE_DIR")}, env("CTR_TROPHY_KEY"),
                                        language ? language : 1);
        const std::filesystem::path user = "runtime/user";
        trophies.file = ctr_trophy::unlock_file(user, trophies.set.np_comm_id);
        ctr_trophy::import_jsonl(user, trophies.file);
        trophies.unlocked = ctr_trophy::read_unlocks(trophies.file);
        trophies.ok = !trophies.set.trophies.empty();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Trophies: %s; unlocks are only recorded\n", error.what());
    }
    char extra[160];
    std::snprintf(extra, sizeof(extra), ",\"trophies\":%zu,\"unlocked\":%zu,\"texts\":%s", trophies.set.trophies.size(),
                  trophies.unlocked.size(), trophies.set.texts ? "true" : "false");
    ctr_hle::log_call(LIB, "trophy_data", trophies.ok ? 0 : -1, extra);
    if (!trophies.set.error.empty())
        std::fprintf(stderr, "Trophies: texts unavailable: %s\n", trophies.set.error.c_str());
}
const ctr_trophy::Trophy* find_trophy(int id) {
    for (const auto& t : trophies.set.trophies)
        if (t.id == id) return &t;
    return nullptr;
}
void notify(const ctr_trophy::Trophy& t) {
    std::wstring name;
    if (!t.name.empty()) {
        const int n = MultiByteToWideChar(CP_UTF8, 0, t.name.data(), static_cast<int>(t.name.size()), nullptr, 0);
        name.resize(static_cast<std::size_t>(n));
        MultiByteToWideChar(CP_UTF8, 0, t.name.data(), static_cast<int>(t.name.size()), name.data(), n);
    } else {
        name = ctr_ui::ui_format(ctr_ui::Text::Trophy, std::to_wstring(t.id));
    }
    std::vector<unsigned char> png;
    try {
        png = ctr_trophy::icon(trophies.set, t.id);
    } catch (const std::exception&) {}
    ctr_hle::trophy::show_popup(std::move(png), ctr_ui::ui_text(ctr_ui::Text::TrophyUnlocked), std::move(name));
}

int log(const char* function, int result, const char* extra = "") {
    ctr_hle::log_call(LIB, function, result, extra);
    return result;
}
Context* find_context(std::int32_t context) {
    if (context <= 0 || std::size_t(context) > contexts.size() || !contexts[context - 1].allocated) {
        return nullptr;
    }
    return &contexts[context - 1];
}
bool valid_handle(std::int32_t handle) {
    return handle > 0 && std::size_t(handle) <= handles.size() && handles[handle - 1];
}
// Common checks of the info/state functions: context, handle, registration.
int check(std::int32_t context, std::int32_t handle, Context** out = nullptr) {
    if (context == -1) {
        return ERROR_INVALID_CONTEXT;
    }
    if (handle == -1) {
        return ERROR_INVALID_HANDLE;
    }
    Context* ctx = find_context(context);
    if (!ctx) {
        return ERROR_INVALID_CONTEXT;
    }
    if (!valid_handle(handle)) {
        return ERROR_INVALID_HANDLE;
    }
    if (!ctx->registered) {
        return ERROR_NOT_REGISTERED;
    }
    if (out) {
        *out = ctx;
    }
    return OK;
}
} // namespace

CTR_SYSV int sceNpTrophyCreateContext(std::int32_t* context, std::int32_t user_id, std::uint32_t service_label,
                                      std::uint64_t options) {
    std::scoped_lock lock{mutex};
    if (!context || options) {
        return log("sceNpTrophyCreateContext", ERROR_INVALID_ARGUMENT);
    }
    for (const auto& c : contexts) {
        if (c.allocated && c.user_id == user_id && c.service_label == service_label) {
            return log("sceNpTrophyCreateContext", ERROR_CONTEXT_ALREADY_EXISTS);
        }
    }
    std::size_t slot = 0;
    while (slot < contexts.size() && contexts[slot].allocated) {
        ++slot;
    }
    if (slot >= MAX_CONTEXTS) {
        return log("sceNpTrophyCreateContext", ERROR_EXCEEDS_MAX);
    }
    if (slot == contexts.size()) {
        contexts.emplace_back();
    }
    contexts[slot] = Context{true, false, user_id, service_label};
    *context = static_cast<std::int32_t>(slot + 1);
    char extra[96];
    std::snprintf(extra, sizeof(extra), ",\"context\":%d,\"user_id\":%d,\"service_label\":%u", *context, user_id,
                  service_label);
    return log("sceNpTrophyCreateContext", OK, extra);
}
CTR_HLE_REGISTER("XbkjbobZlCY", "libSceNpTrophy", sceNpTrophyCreateContext);

CTR_SYSV int sceNpTrophyDestroyContext(std::int32_t context) {
    std::scoped_lock lock{mutex};
    Context* ctx = context == -1 ? nullptr : find_context(context);
    if (!ctx) {
        return log("sceNpTrophyDestroyContext", ERROR_INVALID_CONTEXT);
    }
    *ctx = Context{};
    return log("sceNpTrophyDestroyContext", OK);
}
CTR_HLE_REGISTER("E1Wrwd07Lr8", "libSceNpTrophy", sceNpTrophyDestroyContext);

CTR_SYSV int sceNpTrophyCreateHandle(std::int32_t* handle) {
    std::scoped_lock lock{mutex};
    if (!handle) {
        return log("sceNpTrophyCreateHandle", ERROR_INVALID_ARGUMENT);
    }
    std::size_t slot = 0;
    while (slot < handles.size() && handles[slot]) {
        ++slot;
    }
    if (slot >= MAX_HANDLES) {
        return log("sceNpTrophyCreateHandle", ERROR_EXCEEDS_MAX);
    }
    if (slot == handles.size()) {
        handles.push_back(false);
    }
    handles[slot] = true;
    *handle = static_cast<std::int32_t>(slot + 1);
    char extra[48];
    std::snprintf(extra, sizeof(extra), ",\"handle\":%d", *handle);
    return log("sceNpTrophyCreateHandle", OK, extra);
}
CTR_HLE_REGISTER("q7U6tEAQf7c", "libSceNpTrophy", sceNpTrophyCreateHandle);

CTR_SYSV int sceNpTrophyDestroyHandle(std::int32_t handle) {
    std::scoped_lock lock{mutex};
    if (handle == -1 || !valid_handle(handle)) {
        return log("sceNpTrophyDestroyHandle", ERROR_INVALID_HANDLE);
    }
    handles[handle - 1] = false;
    return log("sceNpTrophyDestroyHandle", OK);
}
CTR_HLE_REGISTER("GNcF4oidY0Y", "libSceNpTrophy", sceNpTrophyDestroyHandle);

CTR_SYSV int sceNpTrophyRegisterContext(std::int32_t context, std::int32_t handle, std::uint64_t options) {
    std::scoped_lock lock{mutex};
    if (options) {
        return log("sceNpTrophyRegisterContext", ERROR_INVALID_ARGUMENT);
    }
    if (context == -1) {
        return log("sceNpTrophyRegisterContext", ERROR_INVALID_CONTEXT);
    }
    if (handle == -1) {
        return log("sceNpTrophyRegisterContext", ERROR_INVALID_HANDLE);
    }
    Context* ctx = find_context(context);
    if (!ctx) {
        return log("sceNpTrophyRegisterContext", ERROR_INVALID_CONTEXT);
    }
    if (!valid_handle(handle)) {
        return log("sceNpTrophyRegisterContext", ERROR_INVALID_HANDLE);
    }
    if (ctx->registered) {
        return log("sceNpTrophyRegisterContext", ERROR_ALREADY_REGISTERED);
    }
    ctx->registered = true;
    load_trophies();
    return log("sceNpTrophyRegisterContext", OK, trophies.ok ? ",\"trophy_data\":true" : ",\"trophy_data\":false");
}
CTR_HLE_REGISTER("TJCAxto9SEU", "libSceNpTrophy", sceNpTrophyRegisterContext);

CTR_SYSV int sceNpTrophyGetTrophyUnlockState(std::int32_t context, std::int32_t handle, std::uint32_t* flags,
                                             std::uint32_t* count) {
    std::scoped_lock lock{mutex};
    if (!flags || !count) {
        return log("sceNpTrophyGetTrophyUnlockState", ERROR_INVALID_ARGUMENT);
    }
    const int status = check(context, handle);
    if (status) {
        return log("sceNpTrophyGetTrophyUnlockState", status);
    }
    std::memset(flags, 0, NUM_MAX / 8);
    *count = 0;
    if (!trophies.ok) return log("sceNpTrophyGetTrophyUnlockState", OK, ",\"count\":0,\"trophy_data\":false");
    for (const auto& [id, when] : trophies.unlocked) flags[id / 32] |= 1u << (id % 32);
    *count = static_cast<std::uint32_t>(trophies.set.trophies.size());
    char extra[96];
    std::snprintf(extra, sizeof(extra), ",\"count\":%u,\"unlocked\":%zu", *count, trophies.unlocked.size());
    return log("sceNpTrophyGetTrophyUnlockState", OK, extra);
}
CTR_HLE_REGISTER("LHuSmO3SLd8", "libSceNpTrophy", sceNpTrophyGetTrophyUnlockState);

CTR_SYSV int sceNpTrophyUnlockTrophy(std::int32_t context, std::int32_t handle, std::int32_t trophy,
                                     std::int32_t* platinum) {
    std::scoped_lock lock{mutex};
    if (context == -1) {
        return log("sceNpTrophyUnlockTrophy", ERROR_INVALID_CONTEXT);
    }
    if (handle == -1) {
        return log("sceNpTrophyUnlockTrophy", ERROR_INVALID_HANDLE);
    }
    if (trophy >= NUM_MAX) {
        return log("sceNpTrophyUnlockTrophy", ERROR_INVALID_TROPHY_ID);
    }
    if (!platinum) {
        return log("sceNpTrophyUnlockTrophy", ERROR_INVALID_ARGUMENT);
    }
    const int status = check(context, handle);
    if (status) {
        return log("sceNpTrophyUnlockTrophy", status);
    }
    *platinum = -1;
    if (trophies.ok) {
        const auto* trophy_entry = find_trophy(trophy);
        if (!trophy_entry || trophy < 0) return log("sceNpTrophyUnlockTrophy", ERROR_INVALID_TROPHY_ID);
        if (trophies.unlocked.count(trophy)) return log("sceNpTrophyUnlockTrophy", ERROR_TROPHY_ALREADY_UNLOCKED);
        const int platinum_id = trophies.set.platinum();
        if (trophy == platinum_id) return log("sceNpTrophyUnlockTrophy", ERROR_PLATINUM_CANNOT_UNLOCK);
        const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));
        trophies.unlocked[trophy] = now;
        // The platinum comes with the last trophy of the base game.
        bool platinum_now = false;
        if (platinum_id >= 0 && !trophies.unlocked.count(platinum_id)) {
            platinum_now = true;
            for (const auto& t : trophies.set.trophies)
                if (t.id != platinum_id && t.group == -1 && !trophies.unlocked.count(t.id)) {
                    platinum_now = false;
                    break;
                }
            if (platinum_now) {
                trophies.unlocked[platinum_id] = now;
                *platinum = platinum_id;
            }
        }
        try {
            ctr_trophy::write_unlocks(trophies.file, trophies.unlocked);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "Trophies: %s\n", error.what());
        }
        notify(*trophy_entry);
        if (platinum_now)
            if (const auto* p = find_trophy(platinum_id)) notify(*p);
        char extra[64];
        std::snprintf(extra, sizeof(extra), ",\"trophy\":%d,\"platinum\":%d", trophy, *platinum);
        return log("sceNpTrophyUnlockTrophy", OK, extra);
    }
    std::filesystem::create_directories("runtime/user/trophies");
    if (FILE* file = std::fopen("runtime/user/trophies/unlocks.jsonl", "ab")) {
        std::fprintf(file, "{\"trophy\":%d,\"time\":%lld}\n", trophy, static_cast<long long>(std::time(nullptr)));
        std::fclose(file);
    }
    char extra[48];
    std::snprintf(extra, sizeof(extra), ",\"trophy\":%d", trophy);
    return log("sceNpTrophyUnlockTrophy", OK, extra);
}
CTR_HLE_REGISTER("28xmRUFao68", "libSceNpTrophy", sceNpTrophyUnlockTrophy);

// Info queries: with no trophy files shadPS4 returns ORBIS_OK after the checks and
// writes no output.
static int info(const char* name, std::int32_t context, std::int32_t handle, const void* a, const void* b) {
    std::scoped_lock lock{mutex};
    const int status = check(context, handle);
    if (status) {
        return log(name, status);
    }
    if (!a || !b) {
        return log(name, ERROR_INVALID_ARGUMENT);
    }
    return log(name, OK, ",\"trophy_data\":false,\"output_written\":false");
}
CTR_SYSV int sceNpTrophyGetGameInfo(std::int32_t context, std::int32_t handle, void* details, void* data) {
    return info("sceNpTrophyGetGameInfo", context, handle, details, data);
}
CTR_HLE_REGISTER("YYP3f2W09og", "libSceNpTrophy", sceNpTrophyGetGameInfo);
CTR_SYSV int sceNpTrophyGetGroupInfo(std::int32_t context, std::int32_t handle, std::int32_t, void* details,
                                     void* data) {
    return info("sceNpTrophyGetGroupInfo", context, handle, details, data);
}
CTR_HLE_REGISTER("wTUwGfspKic", "libSceNpTrophy", sceNpTrophyGetGroupInfo);
CTR_SYSV int sceNpTrophyGetTrophyInfo(std::int32_t context, std::int32_t handle, std::int32_t trophy, void* details,
                                      void* data) {
    if (trophy < 0 || trophy >= NUM_MAX) {
        return log("sceNpTrophyGetTrophyInfo", ERROR_INVALID_TROPHY_ID);
    }
    return info("sceNpTrophyGetTrophyInfo", context, handle, details, data);
}
CTR_HLE_REGISTER("qqUVGDgQBm0", "libSceNpTrophy", sceNpTrophyGetTrophyInfo);

static int icon(const char* name, std::int32_t context, std::int32_t handle) {
    std::scoped_lock lock{mutex};
    const int status = check(context, handle);
    return log(name, status ? status : ERROR_ICON_FILE_NOT_FOUND);
}
CTR_SYSV int sceNpTrophyGetGameIcon(std::int32_t context, std::int32_t handle, void*, std::uint64_t*) {
    return icon("sceNpTrophyGetGameIcon", context, handle);
}
CTR_HLE_REGISTER("HLwz1fRIycA", "libSceNpTrophy", sceNpTrophyGetGameIcon);
CTR_SYSV int sceNpTrophyGetGroupIcon(std::int32_t context, std::int32_t handle, std::int32_t, void*, std::uint64_t*) {
    return icon("sceNpTrophyGetGroupIcon", context, handle);
}
CTR_HLE_REGISTER("w4uMPmErD4I", "libSceNpTrophy", sceNpTrophyGetGroupIcon);
CTR_SYSV int sceNpTrophyGetTrophyIcon(std::int32_t context, std::int32_t handle, std::int32_t, void*, std::uint64_t*) {
    return icon("sceNpTrophyGetTrophyIcon", context, handle);
}
CTR_HLE_REGISTER("eBL+l6HG9xk", "libSceNpTrophy", sceNpTrophyGetTrophyIcon);

// Declared compatibility stubs (compat_stubs.json, R8): system UI and capture features.
CTR_SYSV int sceNpTrophyShowTrophyList(std::int32_t, std::int32_t) {
    return log("sceNpTrophyShowTrophyList", OK, ",\"declared_compatibility_stub\":\"R8\"");
}
CTR_HLE_REGISTER("d9jpdPz5f-8", "libSceNpTrophy", sceNpTrophyShowTrophyList);
CTR_SYSV int sceNpTrophyAbortHandle(std::int32_t) {
    return log("sceNpTrophyAbortHandle", OK, ",\"declared_compatibility_stub\":\"R8\"");
}
CTR_HLE_REGISTER("aTnHs7W-9Uk", "libSceNpTrophy", sceNpTrophyAbortHandle);
CTR_SYSV int sceNpTrophyCaptureScreenshot() {
    return log("sceNpTrophyCaptureScreenshot", OK, ",\"declared_compatibility_stub\":\"R8\"");
}
CTR_HLE_REGISTER("cqGkYAN-gRw", "libSceNpTrophy", sceNpTrophyCaptureScreenshot);
