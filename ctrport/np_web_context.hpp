// SPDX-License-Identifier: GPL-2.0-only
// Independent empty library context lifecycle, informed by pinned shadPS4
// np_web_api.cpp initialize/terminate and np_web_api_internal.cpp createLibraryContext.
// Owned local user contexts and unsent request metadata; no PSN identity or network operations.
#pragma once
namespace ctrport {
class NpWebContexts {
    struct Request {
        std::string group, path, type;
        std::uint64_t length = 0, id = 0;
        int method = 0;
        bool sent = false;
    };
    struct UserContext {
        int user = 0, id = 0;
        std::map<std::uint64_t, std::unique_ptr<Request>> requests;
    };
    struct Context {
        std::map<int, std::unique_ptr<UserContext>> users;
        int http = 0;
        size_t capacity = 0;
        void* pool = nullptr;
        ~Context() {
            if (pool) VirtualFree(pool, 0, MEM_RELEASE);
        }
    };
    std::mutex mutex;
    std::map<int, std::unique_ptr<Context>> contexts;
    int next = 1, next_user = 0;
    std::uint64_t next_request = 0;

public:
    int create(int http, size_t bytes) {
        if (!http_contexts.contains(http) || !bytes)
            throw std::runtime_error("NP WebApi requires live owned HTTP context and positive pool size");
        auto c = std::make_unique<Context>();
        c->http = http;
        c->capacity = bytes;
        c->pool = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!c->pool) throw std::runtime_error("NP WebApi pool allocation failed");
        std::lock_guard<std::mutex> lock(mutex);
        if (next >= 0x8000) throw std::runtime_error("NP WebApi context ID wrap unsupported");
        int id = next++;
        contexts.emplace(id, std::move(c));
        return id;
    }
    int create_user(int library, int user) {
        if (library >= 0x8000) return int(0x80552903U);
        if (user == -1) return int(0x80552902U);
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = contexts.find(library);
        if (found == contexts.end()) return int(0x80552904U);
        for (const auto& entry : found->second->users)
            if (entry.second->user == user) return int(0x8055290aU);
        if (next_user >= 0xffff) throw std::runtime_error("NP user context counter wrap unsupported");
        auto object = std::make_unique<UserContext>();
        object->user = user;
        object->id = (library << 16) | (++next_user);
        const int id = object->id;
        found->second->users.emplace(id, std::move(object));
        return id;
    }
    bool owns_user(int library, int id, int user) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto parent = contexts.find(library);
        if (parent == contexts.end()) return false;
        const auto child = parent->second->users.find(id);
        return child != parent->second->users.end() && child->second->user == user;
    }
    static std::string read_text(const char* source) {
        std::string result;
        for (size_t i = 0; i < 4096; ++i) {
            char c = 0;
            SIZE_T read = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), source + i, &c, 1, &read) || read != 1)
                throw std::runtime_error("NP request string inaccessible");
            if (!c) return result;
            result.push_back(c);
        }
        throw std::runtime_error("NP request string exceeds supported4096-byte bound");
    }
    struct Content {
        std::uint64_t length;
        const char* type;
        unsigned char reserved[16];
    };
    int create_request(int user, const char* group, const char* path, int method, const Content* content,
                       std::uint64_t* output) {
        if (!group || !path || method < 0 || method > 3) return int(0x80552902U);
        Content snapshot{};
        if (content) {
            SIZE_T bytes = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), content, &snapshot, sizeof(snapshot), &bytes) ||
                bytes != sizeof(snapshot))
                throw std::runtime_error("NP request content inaccessible");
            if (snapshot.length && !snapshot.type) return int(0x80552908U);
        }
        const auto group_text = read_text(group), path_text = read_text(path);
        const auto type_text = snapshot.type ? read_text(snapshot.type) : std::string{};
        if (output) require_time_output(output, 8);
        std::lock_guard<std::mutex> lock(mutex);
        const auto parent = contexts.find(user >> 16);
        if (parent == contexts.end()) return int(0x80552904U);
        const auto child = parent->second->users.find(user);
        if (child == parent->second->users.end()) return int(0x80552905U);
        if (next_request >= 0xffffffffULL) throw std::runtime_error("NP request counter wrap unsupported");
        auto object = std::make_unique<Request>();
        object->group = group_text;
        object->path = path_text;
        object->type = type_text;
        object->length = snapshot.length;
        object->method = method;
        object->id = (static_cast<std::uint64_t>(user) << 32) | (++next_request);
        const auto id = object->id;
        child->second->requests.emplace(id, std::move(object));
        if (output) *output = id;
        return 0;
    }
    bool owns_request(int user, std::uint64_t id, const char* path) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto p = contexts.find(user >> 16);
        if (p == contexts.end()) return false;
        const auto c = p->second->users.find(user);
        if (c == p->second->users.end()) return false;
        const auto r = c->second->requests.find(id);
        return r != c->second->requests.end() && r->second->path == path;
    }
    int send_offline(std::uint64_t request) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto parent = contexts.find(static_cast<int>(request >> 48));
        if (parent == contexts.end()) return int(0x80552904U);
        const auto child = parent->second->users.find(static_cast<int>(request >> 32));
        if (child == parent->second->users.end()) return int(0x80552905U);
        const auto found = child->second->requests.find(request);
        if (found == child->second->requests.end()) return int(0x80552906U);
        // Public sendRequest670-776: initial timeout is0; mark sent, then fail absent sign-in.
        found->second->sent = true;
        return int(0x80552907U);
    }
    // Independent offline-owned branch of pinned np_web_api_internal.cpp1016-1070.
    // Send is synchronous and never creates HTTP resources: no outstanding worker to abort.
    int delete_request(std::uint64_t request) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto parent = contexts.find(static_cast<int>(request >> 48));
        if (parent == contexts.end()) return int(0x80552904U);
        const auto child = parent->second->users.find(static_cast<int>(request >> 32));
        if (child == parent->second->users.end()) return int(0x80552905U);
        if (!child->second->requests.erase(request)) return int(0x80552906U);
        return 0;
    }
    bool request_attempted(int user, std::uint64_t request) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto p = contexts.find(user >> 16);
        if (p == contexts.end()) return false;
        const auto c = p->second->users.find(user);
        if (c == p->second->users.end()) return false;
        const auto r = c->second->requests.find(request);
        return r != c->second->requests.end() && r->second->sent;
    }
    void destroy(int id) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!contexts.erase(id)) throw std::runtime_error("NP WebApi unknown context ID");
    }
    bool control(int id, size_t bytes) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = contexts.find(id);
        if (it == contexts.end() || it->second->capacity != bytes) return false;
        auto* p = static_cast<unsigned char*>(it->second->pool);
        p[0] = 0x25;
        p[bytes - 1] = 0x81;
        return p[0] == 0x25 && p[bytes - 1] == 0x81;
    }
};
inline NpWebContexts np_web_contexts;
extern "C" int ctr_np_web_init(int http, size_t bytes, uint64_t caller) {
    try {
        int id = np_web_contexts.create(http, bytes);
        std::printf(
            "{\"scope\":\"original_NpWebApiInitialize_return\",\"caller\":%llu,\"http_id\":%d,\"pool_size\":%llu,\"result\":%d,\"owned_empty_library_context\":true,\"connections\":0}\n",
            static_cast<unsigned long long>(caller), http, static_cast<unsigned long long>(bytes), id);
        std::fflush(stdout);
        return id;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=G3AnLNdRBjE library=libSceNpWebApi caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" int ctr_np_web_term(int id, uint64_t caller) {
    try {
        np_web_contexts.destroy(id);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=asz3TtIqGF8 library=libSceNpWebApi caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" int ctr_np_web_create_user(int library, int user, std::uint64_t caller) {
    try {
        const int result = np_web_contexts.create_user(library, user);
        std::printf(
            "{\"scope\":\"owned_np_web_user_context\",\"nid\":\"zk6c65xoyO0\",\"library\":\"libSceNpWebApi\",\"caller\":%llu,\"parent_id\":%d,\"user_id\":%d,\"result\":%d,\"connections\":0}\n",
            static_cast<unsigned long long>(caller), library, user, result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=zk6c65xoyO0 library=libSceNpWebApi caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_np_web_create_user();
extern "C" int ctr_np_web_create_request(int user, const char* group, const char* path, int method,
                                         const NpWebContexts::Content* content, std::uint64_t* output,
                                         std::uint64_t caller) {
    try {
        const int result = np_web_contexts.create_request(user, group, path, method, content, output);
        std::printf(
            "{\"scope\":\"owned_np_web_request\",\"nid\":\"rdgs5Z1MyFw\",\"library\":\"libSceNpWebApi\",\"caller\":%llu,\"user_context\":%d,\"method\":%d,\"result\":%d,\"request_id\":%llu,\"connections\":0}\n",
            static_cast<unsigned long long>(caller), user, method, result,
            static_cast<unsigned long long>(!result && output ? *output : 0));
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=rdgs5Z1MyFw library=libSceNpWebApi caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_np_web_create_request();
extern "C" void ctr_sysv_np_request_fixture();
extern "C" int ctr_np_web_send_offline(std::uint64_t request, const void*, std::uint64_t, void*, std::uint64_t caller) {
    try {
        const auto result = np_web_contexts.send_offline(request);
        std::printf(
            "{\"scope\":\"owned_np_web_offline_send\",\"nid\":\"KjNeZ-29ysQ\",\"library\":\"libSceNpWebApi\",\"caller\":%llu,\"request_id\":%llu,\"result\":%d,\"signed_in\":false,\"connections\":0}\n",
            static_cast<unsigned long long>(caller), static_cast<unsigned long long>(request), result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=KjNeZ-29ysQ library=libSceNpWebApi caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_np_web_send_offline();
extern "C" void ctr_sysv_np_send_fixture();
extern "C" int ctr_np_web_delete_request(std::uint64_t request, std::uint64_t caller) {
    try {
        const int result = np_web_contexts.delete_request(request);
        std::printf(
            "{\"scope\":\"owned_np_web_delete_request\",\"nid\":\"noQgleu+KLE\",\"library\":\"libSceNpWebApi\",\"caller\":%llu,\"request_id\":%llu,\"result\":%d,\"connections\":0}\n",
            static_cast<unsigned long long>(caller), static_cast<unsigned long long>(request), result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=noQgleu+KLE library=libSceNpWebApi caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_np_web_delete_request();
extern "C" void ctr_sysv_np_web_init();
extern "C" void ctr_sysv_np_web_term();
}
