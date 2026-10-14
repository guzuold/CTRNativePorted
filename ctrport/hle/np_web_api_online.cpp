// SPDX-License-Identifier: GPL-2.0-or-later
// libSceNpWebApi in the online runtime (R8, active only with CTR_ONLINE=1).
//
// On the console these calls reach PSN web services over HTTPS. Here every request goes, in
// plain HTTP, to the project's server (online_http.h) as "<METHOD> /webapi/<apiGroup><path>",
// which answers in place of PSN (the model shadPS4 uses with its shadNet web API server).
// Sony is never contacted.
// Context, handle and request IDs, structures, error codes and the mapping of HTTP error
// statuses follow pinned shadPS4 v0.18.0 src/core/libraries/np/np_web_api (np_web_api.cpp,
// np_web_api_internal.cpp, np_web_api.h) and np_error.h (GPL-2.0-or-later).
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "ctr_hle.h"
#include "online_config.h"
#include "online_http.h"

namespace {
constexpr int kInvalidArgument = static_cast<int>(0x80552902u);
constexpr int kInvalidLibContextId = static_cast<int>(0x80552903u);
constexpr int kLibContextNotFound = static_cast<int>(0x80552904u);
constexpr int kUserContextNotFound = static_cast<int>(0x80552905u);
constexpr int kRequestNotFound = static_cast<int>(0x80552906u);
constexpr int kAborted = static_cast<int>(0x80552909u);
constexpr int kUserContextAlreadyExist = static_cast<int>(0x8055290Au);
constexpr int kHandleNotFound = static_cast<int>(0x8055290Du);
constexpr int kExtdFilterNotFound = static_cast<int>(0x8055291Cu);
constexpr int kExtdCallbackNotFound = static_cast<int>(0x8055291Du);
constexpr int kNpInvalidArgument = static_cast<int>(0x80550003u);
constexpr int kServerUnreachable = static_cast<int>(0x82ffffffu);

struct ContentParameter {
    std::uint64_t content_length;
    const char* content_type;
    unsigned char reserved[16];
};
struct ResponseInformation {
    std::int32_t http_status;
    char* error_object;
    std::uint64_t error_object_size;
    std::uint64_t response_data_size;
};
struct MemoryPoolStats {
    std::uint64_t pool_size;
    std::uint64_t max_inuse_size;
    std::uint64_t current_inuse_size;
    std::int32_t reserved;
};
struct NpId {
    char handle[16];
    char term;
    char dummy[3];
    unsigned char opt[8];
    unsigned char reserved[8];
};

struct Request {
    std::string api_group, path;
    int method = 0;
    std::uint64_t content_length = 0;
    std::string content_type;
    std::vector<std::pair<std::string, std::string>> headers;
    bool aborted = false;
    bool sent = false;
    int status = 0;
    std::map<std::string, std::string> response_headers; // lower-case names
    std::string body;
    std::size_t read_offset = 0;
};

std::mutex mutex;
std::map<int, std::uint64_t> libraries; // libCtxId -> pool size
std::map<int, int> user_contexts;       // userCtxId -> user ID
std::map<int, int> handles;             // handleId -> libCtxId
std::map<int, int> extd_filters;        // filterId -> libCtxId
std::map<int, int> extd_callbacks;      // callbackId -> userCtxId
std::map<std::int64_t, std::shared_ptr<Request>> requests;
int library_count = 0, user_context_count = 0, handle_count = 0, filter_count = 0,
    callback_count = 0;
std::int64_t request_count = 0;

const char* method_name(int method) {
    switch (method) {
    case 0: return "GET";
    case 1: return "POST";
    case 2: return "PUT";
    case 3: return "DELETE";
    case 4: return "PATCH";
    default: return "GET";
    }
}

// One exchange with the project server (online_http.h).
bool exchange(Request& request, const std::string& body) {
    auto headers = request.headers;
    if (!request.content_type.empty()) {
        headers.emplace_back("Content-Type", request.content_type);
    }
    ctr_hle::online::HttpResponse response;
    if (!ctr_hle::online::http_exchange(method_name(request.method),
                                        "/webapi/" + request.api_group + request.path, headers,
                                        body, &response)) {
        return false;
    }
    request.status = response.status;
    request.response_headers = std::move(response.headers);
    request.body = std::move(response.body);
    return true;
}

// {"error":{"code":N,...}} -> N (shadPS4 captureWebApiError).
int error_code(const std::string& body) {
    const auto key = body.find("\"code\"");
    if (key == std::string::npos) {
        return 0;
    }
    const auto colon = body.find(':', key);
    return colon == std::string::npos ? 0 : std::atoi(body.c_str() + colon + 1);
}

std::shared_ptr<Request> find_request(std::int64_t id) {
    std::scoped_lock lock{mutex};
    const auto it = requests.find(id);
    return it == requests.end() ? nullptr : it->second;
}

void log_webapi(const char* function, long long result, const std::string& extra = "") {
    ctr_hle::log_call("libSceNpWebApi", function, result, extra.c_str());
}
} // namespace

CTR_SYSV int sceNpWebApiInitialize(int http_context, std::uint64_t pool_size) {
    std::scoped_lock lock{mutex};
    const int id = ++library_count;
    libraries[id] = pool_size;
    log_webapi("sceNpWebApiInitialize", id, ",\"pool_size\":" + std::to_string(pool_size));
    return id;
}
CTR_HLE_REGISTER_ONLINE("G3AnLNdRBjE", "libSceNpWebApi", sceNpWebApiInitialize);

CTR_SYSV int sceNpWebApiTerminate(int library) {
    std::scoped_lock lock{mutex};
    if (!libraries.erase(library)) {
        return kLibContextNotFound;
    }
    return 0;
}
CTR_HLE_REGISTER_ONLINE("asz3TtIqGF8", "libSceNpWebApi", sceNpWebApiTerminate);

CTR_SYSV int sceNpWebApiGetMemoryPoolStats(int library, MemoryPoolStats* stats) {
    std::scoped_lock lock{mutex};
    const auto it = libraries.find(library);
    if (it == libraries.end()) {
        return kLibContextNotFound;
    }
    if (!stats) {
        return kInvalidArgument;
    }
    // Request data lives in host memory; nothing is taken from the guest pool.
    *stats = {it->second, 0, 0, 0};
    return 0;
}
CTR_HLE_REGISTER_ONLINE("3OnubUs02UM", "libSceNpWebApi", sceNpWebApiGetMemoryPoolStats);

CTR_SYSV int sceNpWebApiCreateContextA(int library, int user_id) {
    if (library >= 0x8000) {
        return kInvalidLibContextId;
    }
    std::scoped_lock lock{mutex};
    if (!libraries.count(library)) {
        return kLibContextNotFound;
    }
    for (const auto& [id, user] : user_contexts) {
        if ((id >> 16) == library && user == user_id) {
            return kUserContextAlreadyExist;
        }
    }
    const int id = (library << 16) | (++user_context_count & 0xffff);
    user_contexts[id] = user_id;
    log_webapi("sceNpWebApiCreateContextA", id, ",\"user_id\":" + std::to_string(user_id));
    return id;
}
CTR_HLE_REGISTER_ONLINE("zk6c65xoyO0", "libSceNpWebApi", sceNpWebApiCreateContextA);

CTR_SYSV int sceNpWebApiDeleteContext(int user_context) {
    std::scoped_lock lock{mutex};
    return user_contexts.erase(user_context) ? 0 : kUserContextNotFound;
}
CTR_HLE_REGISTER_ONLINE("XUjdsSTTZ3U", "libSceNpWebApi", sceNpWebApiDeleteContext);

CTR_SYSV int sceNpWebApiCreateHandle(int library) {
    std::scoped_lock lock{mutex};
    if (!libraries.count(library)) {
        return kLibContextNotFound;
    }
    const int id = ++handle_count;
    handles[id] = library;
    return id;
}
CTR_HLE_REGISTER_ONLINE("79M-JqvvGo0", "libSceNpWebApi", sceNpWebApiCreateHandle);

CTR_SYSV int sceNpWebApiDeleteHandle(int library, int handle) {
    std::scoped_lock lock{mutex};
    const auto it = handles.find(handle);
    if (it == handles.end() || it->second != library) {
        return kHandleNotFound;
    }
    handles.erase(it);
    return 0;
}
CTR_HLE_REGISTER_ONLINE("5Mn7TYwpl30", "libSceNpWebApi", sceNpWebApiDeleteHandle);

CTR_SYSV int sceNpWebApiCreateRequest(int user_context, const char* api_group, const char* path,
                                      int method, const ContentParameter* content,
                                      std::int64_t* request_id) {
    if (!api_group || !path || !request_id || method < 0 || method > 4) {
        return kInvalidArgument;
    }
    auto request = std::make_shared<Request>();
    request->api_group = api_group;
    request->path = path;
    request->method = method;
    if (content) {
        request->content_length = content->content_length;
        request->content_type = content->content_type ? content->content_type : "";
    }
    std::int64_t id;
    {
        std::scoped_lock lock{mutex};
        if (!user_contexts.count(user_context)) {
            return kUserContextNotFound;
        }
        id = (static_cast<std::int64_t>(user_context) << 32) | (++request_count & 0xffffffff);
        requests[id] = request;
    }
    *request_id = id;
    log_webapi("sceNpWebApiCreateRequest", 0,
               ",\"request\":" + std::to_string(id) + ",\"method\":\"" + method_name(method) +
                   "\",\"api_group\":\"" + ctr_hle::online::json_escape(request->api_group) + "\",\"path\":\"" +
                   ctr_hle::online::json_escape(request->path) + "\"");
    return 0;
}
CTR_HLE_REGISTER_ONLINE("rdgs5Z1MyFw", "libSceNpWebApi", sceNpWebApiCreateRequest);

CTR_SYSV int sceNpWebApiAddHttpRequestHeader(std::int64_t id, const char* name, const char* value) {
    if (!name || !value) {
        return kInvalidArgument;
    }
    const auto request = find_request(id);
    if (!request) {
        return kRequestNotFound;
    }
    std::scoped_lock lock{mutex};
    request->headers.emplace_back(name, value);
    return 0;
}
CTR_HLE_REGISTER_ONLINE("joRjtRXTFoc", "libSceNpWebApi", sceNpWebApiAddHttpRequestHeader);

CTR_SYSV int sceNpWebApiSendRequest2(std::int64_t id, const void* data, std::uint64_t size,
                                     ResponseInformation* information) {
    const auto request = find_request(id);
    if (!request) {
        return kRequestNotFound;
    }
    if (request->aborted) {
        return kAborted;
    }
    const std::string body = data && size ? std::string(static_cast<const char*>(data), size) : "";
    request->sent = true;
    int result = 0;
    if (!exchange(*request, body)) {
        result = kServerUnreachable;
    } else {
        if (information) {
            information->http_status = request->status;
        }
        if (request->status >= 400) {
            if (information) {
                information->response_data_size = request->body.size();
                if (information->error_object && information->error_object_size) {
                    const std::size_t n = request->body.size() < information->error_object_size - 1
                                              ? request->body.size()
                                              : information->error_object_size - 1;
                    std::memcpy(information->error_object, request->body.data(), n);
                    information->error_object[n] = 0;
                }
            }
            const int code = error_code(request->body);
            result = code > 0 && code < 0xc00000 ? static_cast<int>(0x82000000u | code)
                                                 : static_cast<int>(0x82f00000u | request->status);
        }
    }
    log_webapi("sceNpWebApiSendRequest2", result,
               ",\"request\":" + std::to_string(id) + ",\"method\":\"" +
                   method_name(request->method) + "\",\"api_group\":\"" +
                   ctr_hle::online::json_escape(request->api_group) + "\",\"path\":\"" +
                   ctr_hle::online::json_escape(request->path) + "\",\"status\":" + std::to_string(request->status) +
                   ",\"request_body\":\"" + ctr_hle::online::json_escape(body.substr(0, 512)) +
                   "\",\"response_body\":\"" + ctr_hle::online::json_escape(request->body.substr(0, 512)) + "\"");
    return result;
}
CTR_HLE_REGISTER_ONLINE("KjNeZ-29ysQ", "libSceNpWebApi", sceNpWebApiSendRequest2);

CTR_SYSV int sceNpWebApiReadData(std::int64_t id, void* data, std::uint64_t size) {
    if (!data || !size) {
        return kInvalidArgument;
    }
    const auto request = find_request(id);
    if (!request) {
        return kRequestNotFound;
    }
    std::scoped_lock lock{mutex};
    const std::size_t remaining = request->body.size() - request->read_offset;
    const std::size_t n = remaining < size ? remaining : static_cast<std::size_t>(size);
    std::memcpy(data, request->body.data() + request->read_offset, n);
    request->read_offset += n;
    return static_cast<int>(n);
}
CTR_HLE_REGISTER_ONLINE("CQtPRSF6Ds8", "libSceNpWebApi", sceNpWebApiReadData);

CTR_SYSV int sceNpWebApiGetHttpResponseHeaderValueLength(std::int64_t id, const char* name,
                                                         std::uint64_t* length) {
    if (!name || !length) {
        return kInvalidArgument;
    }
    const auto request = find_request(id);
    if (!request) {
        return kRequestNotFound;
    }
    std::scoped_lock lock{mutex};
    // An absent header reads as empty (shadPS4 getHttpResponseHeaderValue).
    const auto it = request->response_headers.find(ctr_hle::online::lower(name));
    *length = it == request->response_headers.end() ? 0 : it->second.size();
    return 0;
}
CTR_HLE_REGISTER_ONLINE("743ZzEBzlV8", "libSceNpWebApi",
                        sceNpWebApiGetHttpResponseHeaderValueLength);

CTR_SYSV int sceNpWebApiGetHttpResponseHeaderValue(std::int64_t id, const char* name, char* value,
                                                   std::uint64_t size) {
    if (!name || !value || !size) {
        return kInvalidArgument;
    }
    const auto request = find_request(id);
    if (!request) {
        return kRequestNotFound;
    }
    std::scoped_lock lock{mutex};
    // An absent header reads as empty; a longer value is truncated to the buffer.
    const auto it = request->response_headers.find(ctr_hle::online::lower(name));
    const std::string found = it == request->response_headers.end() ? "" : it->second;
    const std::size_t n = found.size() < size - 1 ? found.size() : static_cast<std::size_t>(size - 1);
    std::memcpy(value, found.data(), n);
    value[n] = 0;
    return 0;
}
CTR_HLE_REGISTER_ONLINE("VwJ5L0Higg0", "libSceNpWebApi", sceNpWebApiGetHttpResponseHeaderValue);

CTR_SYSV int sceNpWebApiAbortRequest(std::int64_t id) {
    const auto request = find_request(id);
    if (!request) {
        return kRequestNotFound;
    }
    request->aborted = true;
    return 0;
}
CTR_HLE_REGISTER_ONLINE("JzhYTP2fG18", "libSceNpWebApi", sceNpWebApiAbortRequest);

CTR_SYSV int sceNpWebApiDeleteRequest(std::int64_t id) {
    std::scoped_lock lock{mutex};
    return requests.erase(id) ? 0 : kRequestNotFound;
}
CTR_HLE_REGISTER_ONLINE("noQgleu+KLE", "libSceNpWebApi", sceNpWebApiDeleteRequest);

// Push events: filters and callbacks are kept; the project server has no push channel yet,
// so no event is delivered.
CTR_SYSV int sceNpWebApiCreateExtdPushEventFilter(int library, int handle, const char* service,
                                                  std::uint32_t label, const void* filters,
                                                  std::uint64_t count) {
    if ((service && label == 0xFFFFFFFFu) || !filters || !count) {
        return kInvalidArgument;
    }
    std::scoped_lock lock{mutex};
    if (!libraries.count(library)) {
        return kLibContextNotFound;
    }
    if (!handles.count(handle)) {
        return kHandleNotFound;
    }
    const int id = ++filter_count;
    extd_filters[id] = library;
    log_webapi("sceNpWebApiCreateExtdPushEventFilter", id,
               std::string(",\"service\":\"") + (service ? ctr_hle::online::json_escape(service) : "") + "\"");
    return id;
}
CTR_HLE_REGISTER_ONLINE("M2BUB+DNEGE", "libSceNpWebApi", sceNpWebApiCreateExtdPushEventFilter);

CTR_SYSV int sceNpWebApiDeleteExtdPushEventFilter(int library, int filter) {
    std::scoped_lock lock{mutex};
    const auto it = extd_filters.find(filter);
    if (it == extd_filters.end() || it->second != library) {
        return kExtdFilterNotFound;
    }
    extd_filters.erase(it);
    return 0;
}
CTR_HLE_REGISTER_ONLINE("pfaJtb7SQ80", "libSceNpWebApi", sceNpWebApiDeleteExtdPushEventFilter);

CTR_SYSV int sceNpWebApiRegisterExtdPushEventCallbackA(int user_context, int filter,
                                                       const void* callback, void* argument) {
    if (!callback) {
        return kInvalidArgument;
    }
    std::scoped_lock lock{mutex};
    if (!user_contexts.count(user_context)) {
        return kUserContextNotFound;
    }
    if (!extd_filters.count(filter)) {
        return kExtdFilterNotFound;
    }
    const int id = ++callback_count;
    extd_callbacks[id] = user_context;
    return id;
}
CTR_HLE_REGISTER_ONLINE("jhXKGQJ4egI", "libSceNpWebApi",
                        sceNpWebApiRegisterExtdPushEventCallbackA);

CTR_SYSV int sceNpWebApiUnregisterExtdPushEventCallback(int user_context, int callback) {
    std::scoped_lock lock{mutex};
    const auto it = extd_callbacks.find(callback);
    if (it == extd_callbacks.end() || it->second != user_context) {
        return kExtdCallbackNotFound;
    }
    extd_callbacks.erase(it);
    return 0;
}
CTR_HLE_REGISTER_ONLINE("PqCY25FMzPs", "libSceNpWebApi",
                        sceNpWebApiUnregisterExtdPushEventCallback);

// Serialized NP ID: base64 of "<onlineId>@<opt segments>" (shadPS4 sceNpWebApiUtilityParseNpId).
CTR_SYSV int sceNpWebApiUtilityParseNpId(const char* json, NpId* np_id) {
    if (!json) {
        return kNpInvalidArgument;
    }
    static const std::string alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string decoded;
    int value = 0, bits = -8;
    for (const char* p = json; *p && *p != '='; ++p) {
        const auto index = alphabet.find(*p);
        if (index == std::string::npos) {
            continue;
        }
        value = (value << 6) | static_cast<int>(index);
        bits += 6;
        if (bits >= 0) {
            decoded.push_back(static_cast<char>((value >> bits) & 0xff));
            bits -= 8;
        }
    }
    std::string handle = decoded, opt;
    if (const auto at = decoded.find('@'); at != std::string::npos) {
        handle = decoded.substr(0, at);
        for (std::size_t i = at + 1; i < decoded.size(); ++i) {
            if (decoded[i] != '/' && decoded[i] != '.') {
                opt.push_back(decoded[i]);
            }
        }
    }
    if (np_id) {
        std::memset(np_id, 0, sizeof(*np_id));
        std::memcpy(np_id->handle, handle.data(), handle.size() < 16 ? handle.size() : 16);
        std::memcpy(np_id->opt, opt.data(), opt.size() < 8 ? opt.size() : 8);
        np_id->reserved[0] = 1;
    }
    return 0;
}
CTR_HLE_REGISTER_ONLINE("or0e885BlXo", "libSceNpWebApi", sceNpWebApiUtilityParseNpId);
