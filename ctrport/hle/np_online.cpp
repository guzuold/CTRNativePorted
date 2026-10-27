// SPDX-License-Identifier: GPL-2.0-or-later
// libSceNpManager and libSceNpAuth in the online runtime (R8, active only with CTR_ONLINE=1).
// The local user (user ID 1, the runtime's only login) is signed in with the project's
// local identity (online_config.h) instead of a PSN account; no Sony service is contacted.
// Request lifecycle, structures and error codes follow pinned shadPS4 v0.18.0
// src/core/libraries/np/np_manager.cpp, np_auth.cpp, np_manager.h, np_auth.h, np_types.h and
// np_error.h (GPL-2.0-or-later). The authorization code ("ctrlocal:" + online ID) and the ID
// token (local_id_token) are tokens of the project's server, which accepts them in place of
// PSN credentials; they carry no Sony credential.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include "ctr_hle.h"
#include "online_config.h"

namespace ctr_hle::online { void dispatch_webapi_push(); }
namespace {
constexpr int kLocalUser = 1;
constexpr int kInvalidArgument = static_cast<int>(0x80550003u);
constexpr int kUserNotFound = static_cast<int>(0x80550007u);
constexpr int kCallbackNotRegistered = static_cast<int>(0x80550009u);
constexpr int kInvalidSize = static_cast<int>(0x80550011u);
constexpr int kAborted = static_cast<int>(0x80550012u);
constexpr int kRequestNotFound = static_cast<int>(0x80550014u);
constexpr int kInvalidId = static_cast<int>(0x80550015u);
constexpr int kAuthInvalidArgument = static_cast<int>(0x80550301u);
constexpr int kAuthInvalidSize = static_cast<int>(0x80550302u);
constexpr int kAuthAborted = static_cast<int>(0x80550304u);
constexpr int kAuthRequestNotFound = static_cast<int>(0x80550306u);
constexpr int kAuthInvalidId = static_cast<int>(0x80550307u);
constexpr int kStateSignedIn = 2;

struct OnlineId {
    char data[16];
    char term;
    char dummy[3];
};
struct NpId {
    OnlineId handle;
    unsigned char opt[8];
    unsigned char reserved[8];
};
static_assert(sizeof(NpId) == 36);
struct CreateAsyncParameter {
    std::uint64_t size;
    std::uint64_t cpu_affinity_mask;
    std::int32_t thread_priority;
    unsigned char padding[4];
};
struct CheckPlusParameter {
    std::uint64_t size;
    std::int32_t user_id;
    unsigned char padding[4];
    std::uint64_t features;
    unsigned char reserved[32];
};
struct CheckPlusResult {
    bool authorized;
    unsigned char reserved[32];
};
struct ParentalControlInfo {
    bool content_restriction;
    bool chat_restriction;
    bool user_generated_content_restriction;
};
struct AuthCodeParameterA {
    std::uint64_t size;
    std::int32_t user_id;
    unsigned char padding[4];
    const void* client_id;
    const char* scope;
};
struct IdTokenParameterA {
    std::uint64_t size;
    std::int32_t user_id;
    unsigned char padding[4];
    const void* client_id;
    const void* client_secret;
    const char* scope;
};
struct AuthorizationCode {
    char code[129];
    unsigned char padding[7];
};
struct IdToken {
    char token[4097];
    unsigned char padding[7];
};

// Requests (shadPS4 NpRequest / NpAuthRequest): IDs offset 0x20000000 (manager) and
// 0x10000000 (auth); async requests complete immediately and report through PollAsync.
enum class RequestState { None, Ready, Aborted, Complete };
struct Request {
    RequestState state = RequestState::None;
    bool async = false;
    int result = 0;
};
struct RequestTable {
    int offset;
    int not_found;
    int invalid_argument;
    int aborted;
    int invalid_id;
    std::mutex mutex;
    std::vector<Request> requests;

    int create(bool async) {
        std::scoped_lock lock{mutex};
        std::size_t index = 0;
        while (index < requests.size() && requests[index].state != RequestState::None) {
            ++index;
        }
        if (index == requests.size()) {
            requests.emplace_back();
        }
        requests[index] = {RequestState::Ready, async, 0};
        return offset + static_cast<int>(index) + 1;
    }
    Request* find(int id) {
        const int index = id - offset - 1;
        if (index < 0 || index >= static_cast<int>(requests.size()) ||
            requests[index].state == RequestState::None) {
            return nullptr;
        }
        return &requests[index];
    }
    // Runs an operation on a Ready request; returns the call result.
    template <typename Operation>
    int run(int id, Operation operation) {
        std::scoped_lock lock{mutex};
        Request* request = find(id);
        if (!request) {
            return not_found;
        }
        if (request->state == RequestState::Complete) {
            request->result = invalid_argument;
            return invalid_argument;
        }
        if (request->state == RequestState::Aborted) {
            request->result = aborted;
            return aborted;
        }
        const int result = operation();
        request->state = RequestState::Complete;
        request->result = result;
        return request->async ? 0 : result;
    }
    int remove(int id) {
        std::scoped_lock lock{mutex};
        Request* request = find(id);
        if (!request) {
            return not_found;
        }
        *request = {};
        return 0;
    }
    int abort(int id) {
        std::scoped_lock lock{mutex};
        Request* request = find(id);
        if (!request) {
            return not_found;
        }
        if (request->state != RequestState::Complete) {
            request->state = RequestState::Aborted;
        }
        return 0;
    }
    int poll(int id, int* result) {
        std::scoped_lock lock{mutex};
        Request* request = find(id);
        if (!request) {
            return not_found;
        }
        if (!request->async || request->state == RequestState::Ready) {
            return invalid_id;
        }
        *result = request->result;
        return 0;
    }
};
RequestTable manager_requests{0x20000000, kRequestNotFound, kInvalidArgument, kAborted,
                              kInvalidId};
RequestTable auth_requests{0x10000000, kAuthRequestNotFound, kAuthInvalidArgument, kAuthAborted,
                           kAuthInvalidId};

// State callback of the NpToolkit module: a SignedIn event is queued when it registers and
// delivered by the next sceNpCheckCallback / sceNpCheckCallbackForLib (shadPS4 delivers the
// login state the same way through NotifyNpStateFromUserServiceEvent).
using ToolkitStateCallback = void(CTR_SYSV*)(int user_id, int state, void* userdata);
std::mutex state_mutex;
ToolkitStateCallback toolkit_callback = nullptr;
void* toolkit_userdata = nullptr;
std::deque<int> pending_signed_in;

void dispatch_state_events() {
    ToolkitStateCallback callback;
    void* userdata;
    std::deque<int> events;
    {
        std::scoped_lock lock{state_mutex};
        callback = toolkit_callback;
        userdata = toolkit_userdata;
        if (!callback) {
            return;
        }
        events.swap(pending_signed_in);
    }
    for (const int user : events) {
        callback(user, kStateSignedIn, userdata);
        ctr_hle::log_call("libSceNpManagerForToolkit", "state_callback_dispatch", 0,
                          ",\"user_id\":1,\"state\":\"SignedIn\"");
    }
}

void fill_online_id(OnlineId* output) {
    std::memset(output, 0, sizeof(*output));
    const auto& id = ctr_hle::online::identity().online_id;
    std::memcpy(output->data, id.data(), id.size() < 16 ? id.size() : 16);
}

std::string local_token() {
    return "ctrlocal:" + ctr_hle::online::identity().online_id;
}

std::string base64(const std::string& data) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < data.size(); i += 3) {
        const std::uint32_t chunk =
            static_cast<unsigned char>(data[i]) << 16 |
            (i + 1 < data.size() ? static_cast<unsigned char>(data[i + 1]) << 8 : 0) |
            (i + 2 < data.size() ? static_cast<unsigned char>(data[i + 2]) : 0);
        out.push_back(alphabet[chunk >> 18 & 63]);
        out.push_back(alphabet[chunk >> 12 & 63]);
        out.push_back(i + 1 < data.size() ? alphabet[chunk >> 6 & 63] : '=');
        out.push_back(i + 2 < data.size() ? alphabet[chunk & 63] : '=');
    }
    return out;
}

// ID token in the shape the game's Demonware client reads (retail v01.21, decoded payload of
// a "header.payload.signature" token: "env_iss_id", "duid" of 64 hex digits, "iat", "exp").
// Issued by the runtime for the project server, which
// accepts it; the signature part is a fixed marker, not a Sony signature. Each JSON part is
// padded with spaces to a multiple of 3 bytes so its base64 has no '=' padding.
std::string local_id_token() {
    const auto& identity = ctr_hle::online::identity();
    const auto now = static_cast<long long>(std::time(nullptr));
    char duid[65];
    std::uint64_t mix = identity.account_id;
    for (int i = 0; i < 64; i += 16) {
        mix = (mix ^ 0x9e3779b97f4a7c15ull) * 0xbf58476d1ce4e5b9ull;
        std::snprintf(duid + i, 17, "%016llx", static_cast<unsigned long long>(mix));
    }
    const auto part = [](std::string json) {
        while (json.size() % 3) {
            json.push_back(' ');
        }
        return base64(json);
    };
    return part("{\"alg\":\"none\",\"typ\":\"JWT\"}") + "." +
           part("{\"sub\":\"" + std::to_string(identity.account_id) + "\",\"online_id\":\"" +
                identity.online_id + "\",\"env_iss_id\":256,\"duid\":\"" + duid +
                "\",\"iat\":" + std::to_string(now) + ",\"exp\":" + std::to_string(now + 3600) +
                "}") +
           ".ctrlocal";
}

void log_np(const char* library, const char* function, long long result,
            const std::string& extra = "") {
    ctr_hle::log_call(library, function, result, extra.c_str());
}
} // namespace

// --- libSceNpManager ---

CTR_SYSV int sceNpGetState(int user_id, int* state) {
    if (!state) {
        return kInvalidArgument;
    }
    *state = user_id == kLocalUser ? kStateSignedIn : 1;
    return 0;
}
CTR_HLE_REGISTER_ONLINE("eQH7nWPcAgc", "libSceNpManager", sceNpGetState);

CTR_SYSV int sceNpGetOnlineId(int user_id, OnlineId* online_id) {
    if (user_id == -1 || !online_id) {
        return kInvalidArgument;
    }
    if (user_id != kLocalUser) {
        return kUserNotFound;
    }
    fill_online_id(online_id);
    log_np("libSceNpManager", "sceNpGetOnlineId", 0,
           ",\"online_id\":\"" + ctr_hle::online::identity().online_id + "\"");
    return 0;
}
CTR_HLE_REGISTER_ONLINE("XDncXQIJUSk", "libSceNpManager", sceNpGetOnlineId);

CTR_SYSV int sceNpGetNpId(int user_id, NpId* np_id) {
    if (user_id == -1 || !np_id) {
        return kInvalidArgument;
    }
    if (user_id != kLocalUser) {
        return kUserNotFound;
    }
    std::memset(np_id, 0, sizeof(*np_id));
    fill_online_id(&np_id->handle);
    log_np("libSceNpManager", "sceNpGetNpId", 0,
           ",\"online_id\":\"" + ctr_hle::online::identity().online_id + "\"");
    return 0;
}
CTR_HLE_REGISTER_ONLINE("p-o74CnoNzY", "libSceNpManager", sceNpGetNpId);

CTR_SYSV int sceNpGetAccountIdA(int user_id, std::uint64_t* account_id) {
    if (user_id == -1 || !account_id) {
        return kInvalidArgument;
    }
    if (user_id != kLocalUser) {
        *account_id = 0;
        return kUserNotFound;
    }
    *account_id = ctr_hle::online::identity().account_id;
    log_np("libSceNpManager", "sceNpGetAccountIdA", 0,
           ",\"account_id\":" + std::to_string(*account_id));
    return 0;
}
CTR_HLE_REGISTER_ONLINE("rbknaUjpqWo", "libSceNpManager", sceNpGetAccountIdA);

CTR_SYSV int sceNpGetUserIdByAccountId(std::uint64_t account_id, int* user_id) {
    if (!account_id || !user_id) {
        return kInvalidArgument;
    }
    if (account_id != ctr_hle::online::identity().account_id) {
        return kUserNotFound;
    }
    *user_id = kLocalUser;
    return 0;
}
CTR_HLE_REGISTER_ONLINE("VgYczPGB5ss", "libSceNpManager", sceNpGetUserIdByAccountId);

CTR_SYSV int sceNpCreateRequest() {
    return manager_requests.create(false);
}
CTR_HLE_REGISTER_ONLINE("GpLQDNKICac", "libSceNpManager", sceNpCreateRequest);

CTR_SYSV int sceNpCreateAsyncRequest(const CreateAsyncParameter* parameter) {
    if (!parameter) {
        return kInvalidArgument;
    }
    if (parameter->size != sizeof(CreateAsyncParameter)) {
        return kInvalidSize;
    }
    return manager_requests.create(true);
}
CTR_HLE_REGISTER_ONLINE("eiqMCt9UshI", "libSceNpManager", sceNpCreateAsyncRequest);

CTR_SYSV int sceNpDeleteRequest(int id) {
    return manager_requests.remove(id);
}
CTR_HLE_REGISTER_ONLINE("S7QTn72PrDw", "libSceNpManager", sceNpDeleteRequest);

CTR_SYSV int sceNpAbortRequest(int id) {
    return manager_requests.abort(id);
}
CTR_HLE_REGISTER_ONLINE("OzKvTvg3ZYU", "libSceNpManager", sceNpAbortRequest);

CTR_SYSV int sceNpPollAsync(int id, int* result) {
    if (!result) {
        return kInvalidArgument;
    }
    return manager_requests.poll(id, result);
}
CTR_HLE_REGISTER_ONLINE("uqcPJLWL08M", "libSceNpManager", sceNpPollAsync);

CTR_SYSV int sceNpCheckNpAvailabilityA(int id, int user_id) {
    if (user_id == -1) {
        return kInvalidArgument;
    }
    const int result =
        manager_requests.run(id, [&] { return user_id == kLocalUser ? 0 : kUserNotFound; });
    log_np("libSceNpManager", "sceNpCheckNpAvailabilityA", result);
    return result;
}
CTR_HLE_REGISTER_ONLINE("8Z2Jc5GvGDI", "libSceNpManager", sceNpCheckNpAvailabilityA);

// Online play needs no subscription in this runtime: every feature is authorized.
CTR_SYSV int sceNpCheckPlus(int id, const CheckPlusParameter* parameter, CheckPlusResult* output) {
    if (!id || !parameter || !output) {
        return kInvalidArgument;
    }
    if (parameter->size != sizeof(CheckPlusParameter)) {
        return kInvalidSize;
    }
    if (parameter->user_id == -1 || parameter->features < 1 || parameter->features > 3) {
        return kInvalidArgument;
    }
    const int result = manager_requests.run(id, [&] {
        if (parameter->user_id != kLocalUser) {
            return kUserNotFound;
        }
        std::memset(output, 0, sizeof(*output));
        output->authorized = true;
        return 0;
    });
    log_np("libSceNpManager", "sceNpCheckPlus", result,
           ",\"features\":" + std::to_string(parameter->features));
    return result;
}
CTR_HLE_REGISTER_ONLINE("r6MyYJkryz8", "libSceNpManager", sceNpCheckPlus);

// Notification to the system that a Plus feature is in use; no state in this runtime.
CTR_SYSV int sceNpNotifyPlusFeature(const CheckPlusParameter* parameter) {
    if (!parameter) {
        return kInvalidArgument;
    }
    log_np("libSceNpManager", "sceNpNotifyPlusFeature", 0,
           ",\"features\":" + std::to_string(parameter->features));
    return 0;
}
CTR_HLE_REGISTER_ONLINE("Gaxrp3EWY-M", "libSceNpManager", sceNpNotifyPlusFeature);

CTR_SYSV int sceNpGetParentalControlInfoA(int id, int user_id, signed char* age,
                                          ParentalControlInfo* info) {
    if (!age || !info || user_id == -1) {
        return kInvalidArgument;
    }
    const int result = manager_requests.run(id, [&] {
        if (user_id != kLocalUser) {
            return kUserNotFound;
        }
        *age = 18;
        std::memset(info, 0, sizeof(*info));
        return 0;
    });
    log_np("libSceNpManager", "sceNpGetParentalControlInfoA", result);
    return result;
}
CTR_HLE_REGISTER_ONLINE("m9L3O6yst-U", "libSceNpManager", sceNpGetParentalControlInfoA);

struct ContentRestriction {
    std::uint64_t size;
    signed char default_age_restriction;
    unsigned char padding[3];
    std::int32_t age_restriction_count;
    const void* age_restriction;
};
CTR_SYSV int sceNpSetContentRestriction(const ContentRestriction* restriction) {
    if (!restriction) {
        return kInvalidArgument;
    }
    if (restriction->size != sizeof(ContentRestriction)) {
        return kInvalidSize;
    }
    if (restriction->default_age_restriction < 0 || restriction->age_restriction_count > 0x100 ||
        (restriction->age_restriction_count > 0 && !restriction->age_restriction)) {
        return kInvalidArgument;
    }
    return 0;
}
CTR_HLE_REGISTER_ONLINE("A2CQ3kgSopQ", "libSceNpManager", sceNpSetContentRestriction);

// The NP title ID identifies the title to PSN services, which this runtime never contacts.
CTR_SYSV int sceNpSetNpTitleId(const char* title_id, const void* secret) {
    if (!title_id || !secret) {
        return kInvalidArgument;
    }
    char text[13] = {};
    std::memcpy(text, title_id, 12);
    log_np("libSceNpManager", "sceNpSetNpTitleId", 0, std::string(",\"title_id\":\"") + text + "\"");
    return 0;
}
CTR_HLE_REGISTER_ONLINE("Ec63y59l9tw", "libSceNpManager", sceNpSetNpTitleId);

CTR_SYSV int sceNpCheckCallback() {
    dispatch_state_events();
    ctr_hle::online::dispatch_webapi_push();
    return 0;
}
CTR_HLE_REGISTER_ONLINE("3Zl8BePTh9Y", "libSceNpManager", sceNpCheckCallback);

// --- libSceNpManagerForToolkit ---

CTR_SYSV int sceNpCheckCallbackForLib() {
    dispatch_state_events();
    ctr_hle::online::dispatch_webapi_push();
    return 0;
}
CTR_HLE_REGISTER_ONLINE("JELHf4xPufo", "libSceNpManagerForToolkit", sceNpCheckCallbackForLib);

CTR_SYSV int sceNpRegisterStateCallbackForToolkit(ToolkitStateCallback callback, void* userdata) {
    if (!callback) {
        return kInvalidArgument;
    }
    {
        std::scoped_lock lock{state_mutex};
        toolkit_callback = callback;
        toolkit_userdata = userdata;
        pending_signed_in.push_back(kLocalUser);
    }
    log_np("libSceNpManagerForToolkit", "sceNpRegisterStateCallbackForToolkit", 0);
    return 0;
}
CTR_HLE_REGISTER_ONLINE("0c7HbXRKUt4", "libSceNpManagerForToolkit",
                        sceNpRegisterStateCallbackForToolkit);

CTR_SYSV int sceNpUnregisterStateCallbackForToolkit() {
    std::scoped_lock lock{state_mutex};
    if (!toolkit_callback) {
        return kCallbackNotRegistered;
    }
    toolkit_callback = nullptr;
    toolkit_userdata = nullptr;
    return 0;
}
CTR_HLE_REGISTER_ONLINE("YIvqqvJyjEc", "libSceNpManagerForToolkit",
                        sceNpUnregisterStateCallbackForToolkit);

// --- libSceNpAuth ---

CTR_SYSV int sceNpAuthCreateRequest() {
    return auth_requests.create(false);
}
CTR_HLE_REGISTER_ONLINE("6bwFkosYRQg", "libSceNpAuth", sceNpAuthCreateRequest);

CTR_SYSV int sceNpAuthCreateAsyncRequest(const CreateAsyncParameter* parameter) {
    if (!parameter) {
        return kAuthInvalidArgument;
    }
    if (parameter->size != sizeof(CreateAsyncParameter)) {
        return kAuthInvalidSize;
    }
    return auth_requests.create(true);
}
CTR_HLE_REGISTER_ONLINE("N+mr7GjTvr8", "libSceNpAuth", sceNpAuthCreateAsyncRequest);

CTR_SYSV int sceNpAuthDeleteRequest(int id) {
    return auth_requests.remove(id);
}
CTR_HLE_REGISTER_ONLINE("H8wG9Bk-nPc", "libSceNpAuth", sceNpAuthDeleteRequest);

CTR_SYSV int sceNpAuthAbortRequest(int id) {
    return auth_requests.abort(id);
}
CTR_HLE_REGISTER_ONLINE("cE7wIsqXdZ8", "libSceNpAuth", sceNpAuthAbortRequest);

CTR_SYSV int sceNpAuthPollAsync(int id, int* result) {
    if (!result) {
        return kAuthInvalidArgument;
    }
    return auth_requests.poll(id, result);
}
CTR_HLE_REGISTER_ONLINE("gjSyfzSsDcE", "libSceNpAuth", sceNpAuthPollAsync);

CTR_SYSV int sceNpAuthGetAuthorizationCodeA(int id, const AuthCodeParameterA* parameter,
                                            AuthorizationCode* code, int* issuer_id) {
    if (!parameter || !code) {
        return kAuthInvalidArgument;
    }
    if (parameter->size != sizeof(AuthCodeParameterA)) {
        return kAuthInvalidSize;
    }
    if (parameter->user_id == -1 || !parameter->client_id || !parameter->scope) {
        return kAuthInvalidArgument;
    }
    const int result = auth_requests.run(id, [&] {
        if (parameter->user_id != kLocalUser) {
            return kUserNotFound;
        }
        std::memset(code, 0, sizeof(*code));
        const std::string token = local_token();
        std::memcpy(code->code, token.data(), token.size() < 128 ? token.size() : 128);
        if (issuer_id) {
            *issuer_id = 256; // production environment issuer
        }
        return 0;
    });
    log_np("libSceNpAuth", "sceNpAuthGetAuthorizationCodeA", result,
           ",\"client_id\":\"" + std::string(static_cast<const char*>(parameter->client_id)) +
               "\",\"scope\":\"" + parameter->scope + "\"");
    return result;
}
CTR_HLE_REGISTER_ONLINE("qAUXQ9GdWp8", "libSceNpAuth", sceNpAuthGetAuthorizationCodeA);

CTR_SYSV int sceNpAuthGetIdTokenA(int id, const IdTokenParameterA* parameter, IdToken* token) {
    if (!parameter || !token) {
        return kAuthInvalidArgument;
    }
    if (parameter->size != sizeof(IdTokenParameterA)) {
        return kAuthInvalidSize;
    }
    if (parameter->user_id == -1 || !parameter->client_id || !parameter->client_secret ||
        !parameter->scope) {
        return kAuthInvalidArgument;
    }
    const int result = auth_requests.run(id, [&] {
        if (parameter->user_id != kLocalUser) {
            return kUserNotFound;
        }
        std::memset(token, 0, sizeof(*token));
        const std::string text = local_id_token();
        std::memcpy(token->token, text.data(), text.size() < 4096 ? text.size() : 4096);
        return 0;
    });
    log_np("libSceNpAuth", "sceNpAuthGetIdTokenA", result,
           ",\"client_id\":\"" + std::string(static_cast<const char*>(parameter->client_id)) +
               "\",\"scope\":\"" + parameter->scope + "\"");
    return result;
}
CTR_HLE_REGISTER_ONLINE("CocbHVIKPE8", "libSceNpAuth", sceNpAuthGetIdTokenA);
