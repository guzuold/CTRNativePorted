// SPDX-License-Identifier: GPL-2.0-or-later
// libSceHttp in the online runtime (R8, active only with CTR_ONLINE=1).
//
// The game's Demonware client and the NpToolkit module use libSceHttp for HTTPS services
// (*.demonware.net). Each request whose host the R8 policy allows (online_config.h) is sent
// in plain HTTP to the project server (online_http.h) with the original Host header and
// "X-Ctr-Original-Url", so the server can tell the services apart; any other host fails with
// RESOLVER_ENOHOST and is logged, so Sony and Activision are never contacted. There is no
// TLS between the runtime and the project server: the HTTPS certificate callback is kept but
// never invoked, because no certificate is presented.
// IDs (templates, connections and requests share one ID space and headers are inherited
// from template to connection to request), structures, method numbers, error codes and
// header parsing follow pinned shadPS4 v0.18.0 src/core/libraries/network/http.cpp, http.h
// and http_error.h (GPL-2.0-or-later). Requests are blocking (no non-blocking imports).
#include <cctype>
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
constexpr int kBeforeInit = static_cast<int>(0x80431001u);
constexpr int kInvalidId = static_cast<int>(0x80431100u);
constexpr int kInvalidValue = static_cast<int>(0x804311feu);
constexpr int kInvalidUrl = static_cast<int>(0x80433060u);
constexpr int kUnknownScheme = static_cast<int>(0x80431061u);
constexpr int kBeforeSend = static_cast<int>(0x80431065u);
constexpr int kAborted = static_cast<int>(0x80431080u);
constexpr int kNoContentLength = static_cast<int>(0x80431071u);
constexpr int kOutOfMemory = static_cast<int>(0x80431022u);
constexpr int kResolverNoHost = static_cast<int>(0x80436007u);
constexpr int kParseNotFound = static_cast<int>(0x80432025u);
constexpr int kParseInvalidResponse = static_cast<int>(0x80432060u);
constexpr int kParseInvalidValue = static_cast<int>(0x804321feu);
constexpr int kHeaderOverwrite = 0;
constexpr int kHeaderAdd = 1;

using Headers = std::vector<std::pair<std::string, std::string>>;
enum class Kind { Template, Connection, Request };
struct Object {
    Kind kind;
    int parent = 0;
    Headers headers;
    std::string user_agent;
    std::string url; // connection and request
    std::string method;
    std::uint64_t content_length = 0;
    bool chunked = false;
    bool aborted = false;
    bool sent = false;
    ctr_hle::online::HttpResponse response;
    std::size_t read_offset = 0;
};

std::mutex mutex;
std::map<int, bool> contexts;
std::map<int, std::shared_ptr<Object>> objects;
int next_context = 0, next_id = 0;

struct Url {
    std::string scheme, host, path;
    int port = 0;
};
bool parse_url(const std::string& text, Url* url) {
    const auto scheme_end = text.find("://");
    if (scheme_end == std::string::npos) {
        return false;
    }
    url->scheme = ctr_hle::online::lower(text.substr(0, scheme_end));
    const auto host_start = scheme_end + 3;
    const auto path_start = text.find('/', host_start);
    std::string authority = text.substr(host_start, path_start - host_start);
    url->path = path_start == std::string::npos ? "/" : text.substr(path_start);
    if (const auto at = authority.rfind('@'); at != std::string::npos) {
        authority = authority.substr(at + 1);
    }
    const auto colon = authority.rfind(':');
    url->port = url->scheme == "https" ? 443 : 80;
    if (colon != std::string::npos) {
        url->port = std::atoi(authority.c_str() + colon + 1);
        authority = authority.substr(0, colon);
    }
    url->host = authority;
    return !url->host.empty();
}

std::shared_ptr<Object> find(int id) {
    const auto it = objects.find(id);
    return it == objects.end() ? nullptr : it->second;
}

// Request headers: template, then connection, then request (later ones override by name).
Headers effective_headers(const Object& request) {
    std::vector<const Object*> chain{&request};
    for (const Object* o = &request; o->parent;) {
        const auto parent = find(o->parent);
        if (!parent) {
            break;
        }
        chain.insert(chain.begin(), parent.get());
        o = parent.get();
    }
    Headers result;
    for (const Object* o : chain) {
        for (const auto& [name, value] : o->headers) {
            const std::string key = ctr_hle::online::lower(name);
            std::erase_if(result, [&](const auto& kv) { return ctr_hle::online::lower(kv.first) == key; });
            result.emplace_back(name, value);
        }
    }
    return result;
}

int create_object(Kind kind, int parent, std::shared_ptr<Object> object) {
    object->kind = kind;
    object->parent = parent;
    const int id = ++next_id;
    objects[id] = std::move(object);
    return id;
}

void log_http(const char* function, long long result, const std::string& extra = "") {
    ctr_hle::log_call("libSceHttp", function, result, extra.c_str());
}

const char* method_name(int method) {
    static const char* names[] = {"GET", "POST", "HEAD", "OPTIONS", "PUT", "DELETE", "TRACE", "CONNECT"};
    return method >= 0 && method < 8 ? names[method] : nullptr;
}

int create_request(int connection, const std::string& method, const char* url, std::uint64_t content_length) {
    std::scoped_lock lock{mutex};
    if (contexts.empty()) {
        return kBeforeInit;
    }
    const auto parent = find(connection);
    if (!parent || parent->kind != Kind::Connection) {
        return kInvalidId;
    }
    auto request = std::make_shared<Object>();
    request->url = url ? url : parent->url;
    request->method = method;
    request->content_length = content_length;
    Url parsed;
    if (!parse_url(request->url, &parsed)) {
        return kInvalidUrl;
    }
    const int id = create_object(Kind::Request, connection, request);
    log_http("sceHttpCreateRequestWithURL", id,
             ",\"method\":\"" + method + "\",\"url\":\"" + ctr_hle::online::json_escape(request->url) + "\"");
    return id;
}
} // namespace

CTR_SYSV int sceHttpInit(int net_pool, int ssl_context, std::uint64_t pool_size) {
    if (!pool_size) {
        return static_cast<int>(0x80020016u); // kernel EINVAL (shadPS4 sceHttpInit)
    }
    std::scoped_lock lock{mutex};
    const int id = ++next_context;
    contexts[id] = true;
    log_http("sceHttpInit", id, ",\"pool_size\":" + std::to_string(pool_size));
    return id;
}
CTR_HLE_REGISTER_ONLINE("A9cVMUtEp4Y", "libSceHttp", sceHttpInit);

CTR_SYSV int sceHttpTerm(int context) {
    std::scoped_lock lock{mutex};
    return contexts.erase(context) ? 0 : kInvalidId;
}
CTR_HLE_REGISTER_ONLINE("Ik-KpLTlf7Q", "libSceHttp", sceHttpTerm);

struct MemoryPoolStats {
    std::uint64_t pool_size, max_inuse_size, current_inuse_size;
    std::int32_t reserved;
};
CTR_SYSV int sceHttpGetMemoryPoolStats(int context, MemoryPoolStats* stats) {
    std::scoped_lock lock{mutex};
    if (!contexts.count(context)) {
        return kInvalidId;
    }
    if (!stats) {
        return kInvalidValue;
    }
    // Request buffers live in host memory; nothing is taken from the guest pool.
    *stats = {0, 0, 0, 0};
    return 0;
}
CTR_HLE_REGISTER_ONLINE("16sMmVuOvgU", "libSceHttp", sceHttpGetMemoryPoolStats);

CTR_SYSV int sceHttpCreateTemplate(int context, const char* user_agent, int version, int auto_proxy) {
    std::scoped_lock lock{mutex};
    if (!contexts.count(context)) {
        return contexts.empty() ? kBeforeInit : kInvalidId;
    }
    auto object = std::make_shared<Object>();
    object->user_agent = user_agent ? user_agent : "";
    if (!object->user_agent.empty()) {
        object->headers.emplace_back("User-Agent", object->user_agent);
    }
    const int id = create_object(Kind::Template, 0, object);
    log_http("sceHttpCreateTemplate", id,
             ",\"user_agent\":\"" + ctr_hle::online::json_escape(object->user_agent) + "\"");
    return id;
}
CTR_HLE_REGISTER_ONLINE("0gYjPTR-6cY", "libSceHttp", sceHttpCreateTemplate);

CTR_SYSV int sceHttpCreateConnectionWithURL(int template_id, const char* url, bool keep_alive) {
    if (!url) {
        return kInvalidUrl;
    }
    Url parsed;
    if (!parse_url(url, &parsed)) {
        return kInvalidUrl;
    }
    if (parsed.scheme != "http" && parsed.scheme != "https") {
        return kUnknownScheme;
    }
    std::scoped_lock lock{mutex};
    const auto parent = find(template_id);
    if (!parent || parent->kind != Kind::Template) {
        return kInvalidId;
    }
    auto object = std::make_shared<Object>();
    object->url = url;
    const int id = create_object(Kind::Connection, template_id, object);
    log_http("sceHttpCreateConnectionWithURL", id, ",\"url\":\"" + ctr_hle::online::json_escape(url) + "\"");
    return id;
}
CTR_HLE_REGISTER_ONLINE("qgxDBjorUxs", "libSceHttp", sceHttpCreateConnectionWithURL);

CTR_SYSV int sceHttpCreateRequestWithURL(int connection, int method, const char* url, std::uint64_t content_length) {
    const char* name = method_name(method);
    if (!name) {
        return kInvalidValue;
    }
    return create_request(connection, name, url, content_length);
}
CTR_HLE_REGISTER_ONLINE("Aeu5wVKkF9w", "libSceHttp", sceHttpCreateRequestWithURL);

CTR_SYSV int sceHttpCreateRequestWithURL2(int connection, const char* method, const char* url,
                                          std::uint64_t content_length) {
    if (!method || !*method) {
        return kInvalidValue;
    }
    return create_request(connection, method, url, content_length);
}
CTR_HLE_REGISTER_ONLINE("Cnp77podkCU", "libSceHttp", sceHttpCreateRequestWithURL2);

namespace {
int delete_object(int id, Kind kind) {
    std::scoped_lock lock{mutex};
    const auto object = find(id);
    if (!object || object->kind != kind) {
        return kInvalidId;
    }
    objects.erase(id);
    return 0;
}
} // namespace

CTR_SYSV int sceHttpDeleteTemplate(int id) {
    return delete_object(id, Kind::Template);
}
CTR_HLE_REGISTER_ONLINE("4I8vEpuEhZ8", "libSceHttp", sceHttpDeleteTemplate);

CTR_SYSV int sceHttpDeleteConnection(int id) {
    return delete_object(id, Kind::Connection);
}
CTR_HLE_REGISTER_ONLINE("P6A3ytpsiYc", "libSceHttp", sceHttpDeleteConnection);

CTR_SYSV int sceHttpDeleteRequest(int id) {
    return delete_object(id, Kind::Request);
}
CTR_HLE_REGISTER_ONLINE("qe7oZ+v4PWA", "libSceHttp", sceHttpDeleteRequest);

CTR_SYSV int sceHttpAddRequestHeader(int id, const char* name, const char* value, int mode) {
    if ((mode != kHeaderOverwrite && mode != kHeaderAdd) || !name || !value) {
        return kInvalidValue;
    }
    std::scoped_lock lock{mutex};
    const auto object = find(id);
    if (!object) {
        return kInvalidId;
    }
    if (mode == kHeaderOverwrite) {
        const std::string key = ctr_hle::online::lower(name);
        std::erase_if(object->headers, [&](const auto& kv) { return ctr_hle::online::lower(kv.first) == key; });
    }
    object->headers.emplace_back(name, value);
    return 0;
}
CTR_HLE_REGISTER_ONLINE("EY28T2bkN7k", "libSceHttp", sceHttpAddRequestHeader);

CTR_SYSV int sceHttpSetConnectTimeOut(int id, std::uint32_t microseconds) {
    std::scoped_lock lock{mutex};
    return find(id) ? 0 : kInvalidId; // the project server is local or LAN; timeout unused
}
CTR_HLE_REGISTER_ONLINE("0S9tTH0uqTU", "libSceHttp", sceHttpSetConnectTimeOut);

CTR_SYSV int sceHttpSetRequestContentLength(int id, std::uint64_t length) {
    std::scoped_lock lock{mutex};
    const auto object = find(id);
    if (!object || object->kind != Kind::Request) {
        return kInvalidId;
    }
    object->content_length = length;
    return 0;
}
CTR_HLE_REGISTER_ONLINE("PTiFIUxCpJc", "libSceHttp", sceHttpSetRequestContentLength);

// The body is always sent with Content-Length to the project server; the flag is kept.
CTR_SYSV int sceHttpSetChunkedTransferEnabled(int id, int enabled) {
    std::scoped_lock lock{mutex};
    const auto object = find(id);
    if (!object) {
        return kInvalidId;
    }
    object->chunked = enabled != 0;
    return 0;
}
CTR_HLE_REGISTER_ONLINE("PDxS48xGQLs", "libSceHttp", sceHttpSetChunkedTransferEnabled);

CTR_SYSV int sceHttpsSetSslCallback(int id, const void* callback, void* argument) {
    std::scoped_lock lock{mutex};
    if (!find(id)) {
        return kInvalidId;
    }
    log_http("sceHttpsSetSslCallback", 0, ",\"invoked\":false");
    return 0;
}
CTR_HLE_REGISTER_ONLINE("htyBOoWeS58", "libSceHttp", sceHttpsSetSslCallback);

CTR_SYSV int sceHttpSendRequest(int id, const void* data, std::uint64_t size) {
    std::shared_ptr<Object> request;
    Headers headers;
    {
        std::scoped_lock lock{mutex};
        request = find(id);
        if (!request || request->kind != Kind::Request) {
            return kInvalidId;
        }
        if (request->aborted) {
            return kAborted;
        }
        headers = effective_headers(*request);
    }
    Url url;
    parse_url(request->url, &url);
    std::uint32_t address = 0;
    const char* reason = "";
    if (!ctr_hle::online::resolve(url.host.c_str(), &address, &reason)) {
        log_http("sceHttpSendRequest", kResolverNoHost,
                 ",\"url\":\"" + ctr_hle::online::json_escape(request->url) + "\",\"blocked_by_policy\":true");
        return kResolverNoHost;
    }
    std::erase_if(headers, [](const auto& kv) { return ctr_hle::online::lower(kv.first) == "host"; });
    const bool default_port = url.port == (url.scheme == "https" ? 443 : 80);
    headers.emplace_back("Host", default_port ? url.host : url.host + ":" + std::to_string(url.port));
    headers.emplace_back("X-Ctr-Original-Url", request->url);
    const std::string body = data && size ? std::string(static_cast<const char*>(data), size) : "";
    ctr_hle::online::HttpResponse response;
    const bool reached = ctr_hle::online::http_exchange(request->method, url.path, headers, body, &response);
    {
        std::scoped_lock lock{mutex};
        request->sent = true;
        request->response = reached ? std::move(response) : ctr_hle::online::HttpResponse{};
        request->read_offset = 0;
    }
    const int result = reached ? 0 : kBeforeSend;
    log_http("sceHttpSendRequest", result,
             ",\"method\":\"" + request->method + "\",\"url\":\"" + ctr_hle::online::json_escape(request->url) +
                 "\",\"status\":" + std::to_string(request->response.status) + ",\"request_body\":\"" +
                 ctr_hle::online::json_escape(body.substr(0, 512)) + "\",\"response_body\":\"" +
                 ctr_hle::online::json_escape(request->response.body.substr(0, 512)) + "\"");
    return result;
}
CTR_HLE_REGISTER_ONLINE("1e2BNwI-XzE", "libSceHttp", sceHttpSendRequest);

namespace {
std::shared_ptr<Object> sent_request(int id, int* error) {
    const auto request = find(id);
    if (!request || request->kind != Kind::Request) {
        *error = kInvalidId;
        return nullptr;
    }
    if (!request->sent || !request->response.status) {
        *error = kBeforeSend;
        return nullptr;
    }
    return request;
}
} // namespace

CTR_SYSV int sceHttpGetStatusCode(int id, int* status) {
    if (!status) {
        return kInvalidValue;
    }
    std::scoped_lock lock{mutex};
    int error = 0;
    const auto request = sent_request(id, &error);
    if (!request) {
        return error;
    }
    *status = request->response.status;
    return 0;
}
CTR_HLE_REGISTER_ONLINE("0a2TBNfE3BU", "libSceHttp", sceHttpGetStatusCode);

CTR_SYSV int sceHttpGetResponseContentLength(int id, int* result, std::uint64_t* length) {
    if (!result || !length) {
        return kInvalidValue;
    }
    std::scoped_lock lock{mutex};
    int error = 0;
    const auto request = sent_request(id, &error);
    if (!request) {
        return error;
    }
    const auto it = request->response.headers.find("content-length");
    *result = it == request->response.headers.end() ? kNoContentLength : 0;
    *length = it == request->response.headers.end() ? 0 : std::strtoull(it->second.c_str(), nullptr, 10);
    return 0;
}
CTR_HLE_REGISTER_ONLINE("yuO2H2Uvnos", "libSceHttp", sceHttpGetResponseContentLength);

CTR_SYSV int sceHttpReadData(int id, void* data, std::uint64_t size) {
    if (!data) {
        return kInvalidValue;
    }
    std::scoped_lock lock{mutex};
    int error = 0;
    const auto request = sent_request(id, &error);
    if (!request) {
        return error;
    }
    const std::size_t remaining = request->response.body.size() - request->read_offset;
    const std::size_t n = remaining < size ? remaining : static_cast<std::size_t>(size);
    std::memcpy(data, request->response.body.data() + request->read_offset, n);
    request->read_offset += n;
    return static_cast<int>(n);
}
CTR_HLE_REGISTER_ONLINE("P5pdoykPYTk", "libSceHttp", sceHttpReadData);

CTR_SYSV int sceHttpGetAllResponseHeaders(int id, char** header, std::uint64_t* size) {
    if (!header || !size) {
        return kInvalidValue;
    }
    std::scoped_lock lock{mutex};
    int error = 0;
    const auto request = sent_request(id, &error);
    if (!request) {
        return error;
    }
    auto& block = request->response.header_block;
    *header = block.empty() ? nullptr : block.data();
    *size = block.size();
    return 0;
}
CTR_HLE_REGISTER_ONLINE("aCYPMSUIaP8", "libSceHttp", sceHttpGetAllResponseHeaders);

CTR_SYSV int sceHttpAbortRequest(int id) {
    std::scoped_lock lock{mutex};
    const auto request = find(id);
    if (!request || request->kind != Kind::Request) {
        return kInvalidId;
    }
    request->aborted = true;
    return 0;
}
CTR_HLE_REGISTER_ONLINE("hvG6GfBMXg8", "libSceHttp", sceHttpAbortRequest);

// Header block parser (shadPS4 sceHttpParseResponseHeader): returns the offset just past the
// value's line, the value pointer and its length without the trailing CR; folded lines
// continue the value.
CTR_SYSV int sceHttpParseResponseHeader(const char* header, std::uint64_t length, const char* field, const char** value,
                                        std::uint64_t* value_length) {
    if (!header) {
        return kParseInvalidResponse;
    }
    if (!field || !value || !value_length) {
        return kParseInvalidValue;
    }
    const std::uint64_t field_length = strnlen(field, 0xfff);
    const auto space = [](unsigned char c) { return c < 0x80 && std::isspace(c); };
    std::uint64_t offset = 0;
    bool found = false, line_start = true;
    for (std::uint64_t i = 0; i < length; ++i) {
        if (line_start && !space(static_cast<unsigned char>(header[i])) && field_length < length - i &&
            header[i + field_length] == ':') {
            bool equal = true;
            for (std::uint64_t k = 0; k < field_length && equal; ++k) {
                equal = std::tolower(static_cast<unsigned char>(field[k])) ==
                        std::tolower(static_cast<unsigned char>(header[i + k]));
            }
            if (equal) {
                offset = i + field_length + 1;
                found = true;
                break;
            }
        }
        line_start = header[i] == '\n';
    }
    if (!found) {
        return kParseNotFound;
    }
    while (offset < length && space(static_cast<unsigned char>(header[offset]))) {
        if (header[offset++] == '\n') {
            break;
        }
    }
    const std::uint64_t start = offset;
    std::uint64_t scan = offset, line_end = length, value_end = length;
    while (scan < length) {
        while (scan < length && header[scan] != '\n') {
            ++scan;
        }
        if (scan >= length) {
            break;
        }
        const bool carriage = scan > start && header[scan - 1] == '\r';
        if (scan + 1 < length && (header[scan + 1] == ' ' || header[scan + 1] == '\t')) {
            ++scan;
            continue;
        }
        line_end = scan + 1;
        value_end = carriage ? scan - 1 : scan;
        break;
    }
    if (value_end <= start) {
        *value = nullptr;
        *value_length = 0;
        return 0;
    }
    *value = header + start;
    *value_length = value_end - start;
    return static_cast<int>(line_end);
}
CTR_HLE_REGISTER_ONLINE("hPTXo3bICzI", "libSceHttp", sceHttpParseResponseHeader);

// Percent-encodes everything except unreserved characters (shadPS4 sceHttpUriEscape).
CTR_SYSV int sceHttpUriEscape(char* out, std::uint64_t* require, std::uint64_t prepare, const char* in) {
    if (!in) {
        return kInvalidValue;
    }
    const auto unreserved = [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
    };
    std::uint64_t needed = 1;
    for (const char* p = in; *p; ++p) {
        needed += unreserved(static_cast<unsigned char>(*p)) ? 1 : 3;
    }
    if (require) {
        *require = needed;
    }
    if (!out) {
        return 0;
    }
    if (prepare < needed) {
        return kOutOfMemory;
    }
    static const char hex[] = "0123456789ABCDEF";
    char* d = out;
    for (const char* p = in; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (unreserved(c)) {
            *d++ = *p;
        } else {
            *d++ = '%';
            *d++ = hex[c >> 4];
            *d++ = hex[c & 15];
        }
    }
    *d = 0;
    return 0;
}
CTR_HLE_REGISTER_ONLINE("YuOW3dDAKYc", "libSceHttp", sceHttpUriEscape);
