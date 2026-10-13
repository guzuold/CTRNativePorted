// SPDX-License-Identifier: GPL-2.0-or-later
// libSceNpTrophy without trophy data. Behaviour follows pinned shadPS4 v0.18.0
// src/core/libraries/np/np_trophy.cpp when no trophy files are installed: contexts and
// handles are real (1-based ids, argument checks and error codes as there), registration
// succeeds, the unlock state reports zero trophies, info queries return ORBIS_OK without
// output and icons ICON_FILE_NOT_FOUND. Unlocks are recorded in
// runtime/user/trophies/unlocks.jsonl. ShowTrophyList, AbortHandle and CaptureScreenshot
// are declared compatibility stubs (ctrport/compat_stubs.json, tag R8).
#include <cstring>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <set>
#include <vector>
#include "ctr_hle.h"

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

CTR_SYSV int sceNpTrophyCreateContext(std::int32_t* context, std::int32_t user_id,
                                      std::uint32_t service_label, std::uint64_t options) {
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
    std::snprintf(extra, sizeof(extra), ",\"context\":%d,\"user_id\":%d,\"service_label\":%u",
                  *context, user_id, service_label);
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

CTR_SYSV int sceNpTrophyRegisterContext(std::int32_t context, std::int32_t handle,
                                        std::uint64_t options) {
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
    return log("sceNpTrophyRegisterContext", OK, ",\"trophy_data\":false");
}
CTR_HLE_REGISTER("TJCAxto9SEU", "libSceNpTrophy", sceNpTrophyRegisterContext);

CTR_SYSV int sceNpTrophyGetTrophyUnlockState(std::int32_t context, std::int32_t handle,
                                             std::uint32_t* flags, std::uint32_t* count) {
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
    return log("sceNpTrophyGetTrophyUnlockState", OK, ",\"count\":0,\"trophy_data\":false");
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
    std::filesystem::create_directories("runtime/user/trophies");
    if (FILE* file = std::fopen("runtime/user/trophies/unlocks.jsonl", "ab")) {
        std::fprintf(file, "{\"trophy\":%d,\"time\":%lld}\n", trophy,
                     static_cast<long long>(std::time(nullptr)));
        std::fclose(file);
    }
    char extra[48];
    std::snprintf(extra, sizeof(extra), ",\"trophy\":%d", trophy);
    return log("sceNpTrophyUnlockTrophy", OK, extra);
}
CTR_HLE_REGISTER("28xmRUFao68", "libSceNpTrophy", sceNpTrophyUnlockTrophy);

// Info queries: with no trophy files shadPS4 returns ORBIS_OK after the checks and
// writes no output.
static int info(const char* name, std::int32_t context, std::int32_t handle, const void* a,
                const void* b) {
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
CTR_SYSV int sceNpTrophyGetGameInfo(std::int32_t context, std::int32_t handle, void* details,
                                    void* data) {
    return info("sceNpTrophyGetGameInfo", context, handle, details, data);
}
CTR_HLE_REGISTER("YYP3f2W09og", "libSceNpTrophy", sceNpTrophyGetGameInfo);
CTR_SYSV int sceNpTrophyGetGroupInfo(std::int32_t context, std::int32_t handle, std::int32_t,
                                     void* details, void* data) {
    return info("sceNpTrophyGetGroupInfo", context, handle, details, data);
}
CTR_HLE_REGISTER("wTUwGfspKic", "libSceNpTrophy", sceNpTrophyGetGroupInfo);
CTR_SYSV int sceNpTrophyGetTrophyInfo(std::int32_t context, std::int32_t handle,
                                      std::int32_t trophy, void* details, void* data) {
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
CTR_SYSV int sceNpTrophyGetGroupIcon(std::int32_t context, std::int32_t handle, std::int32_t, void*,
                                     std::uint64_t*) {
    return icon("sceNpTrophyGetGroupIcon", context, handle);
}
CTR_HLE_REGISTER("w4uMPmErD4I", "libSceNpTrophy", sceNpTrophyGetGroupIcon);
CTR_SYSV int sceNpTrophyGetTrophyIcon(std::int32_t context, std::int32_t handle, std::int32_t, void*,
                                      std::uint64_t*) {
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
