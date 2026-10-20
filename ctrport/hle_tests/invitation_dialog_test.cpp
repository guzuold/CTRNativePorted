// SPDX-License-Identifier: GPL-2.0-or-later
// SysV lifecycle + actual native confirmation UI; transport is a mock, no recipients contacted.
#include "../hle/ctr_hle.h"
#include "../hle/invitation_types.h"
#include "../hle/invitation_flow.h"
#include "../hle/online_http.h"
#include "../online_server/json.hpp"
#include <windows.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
using namespace ctr_hle::invitation;
namespace {
std::atomic<int> failures{0};
bool used = false;
std::atomic<int> notice_prompts{0}, accept_attempts{0}, fixture_phase{0};
int response_status = 201;
std::atomic<int> requests{0};
std::string request_body;
void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", what); if (!ok) ++failures; }
int common(int operation) {
    if (!operation) return 1;
    if (operation == 1) { if (used) return 0; used = true; return 1; }
    used = false; return 0;
}
template<typename T> T bind(const char* nid) {
    auto p = ctr_hle_resolve(nid, "libSceInvitationDialog");
    if (!p) std::exit(2);
    return reinterpret_cast<T>(p);
}
BOOL CALLBACK answer(HWND window, LPARAM button) {
    DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
    wchar_t title[64]{}; GetWindowTextW(window, title, 64);
    if (pid == GetCurrentProcessId() && !std::wcscmp(title, L"CTR - Invito")) {
        if (button == IDYES) ++notice_prompts;
        if (button == IDOK && GetDlgItem(window, 100)) SendDlgItemMessageW(window, 100, LB_SETSEL, TRUE, 0);
        PostMessageW(window, WM_COMMAND, static_cast<WPARAM>(button), 0);
    }
    if (pid == GetCurrentProcessId() && !std::wcscmp(title, L"CTR - Errore invito"))
        PostMessageW(window, WM_COMMAND, IDCANCEL, 0);
    return TRUE;
}
}
namespace ctr_hle::online {
std::string json_escape(const std::string& text) { return text; } // fixture uses ASCII hex only
bool http_exchange(const std::string& method, const std::string& path,
                   const std::vector<std::pair<std::string,std::string>>&, const std::string& body,
                   HttpResponse* response) {
    if (method == "GET" && path.find("/friendList?") != std::string::npos) {
        response->status = 200;
        response->body = R"({"friendList":[{"user":{"accountId":"43","onlineId":"FixtureFriend"}}]})";
        return true;
    }
    if (method == "GET" && path == "/ctr/v1/session-invitations") {
        response->status = 200;
        response->body = R"([{"invitationId":"receive-fixture","sessionId":"0123456789abcdef0123456789abcdef","onlineId":"FixtureHost"}])";
        if (fixture_phase == 1) response->body = R"([{"invitationId":"failed-fixture","sessionId":"0123456789abcdef0123456789abcdef","onlineId":"FixtureHost"}])";
        return true;
    }
    if (method == "PUT" && path == "/ctr/v1/session-invitations/receive-fixture") {
        response->status = 200;
        response->body = R"({"invitationId":"receive-fixture","sessionId":"0123456789abcdef0123456789abcdef"})";
        return true;
    }
    if (method == "PUT" && path == "/ctr/v1/session-invitations/failed-fixture") {
        ++accept_attempts; response->status=502;response->body="{}";return true;
    }
    check(method == "POST" && path == "/ctr/v1/session-invitations", "project invitation route");
    request_body = body; ++requests; response->status = response_status;
    auto receipt = ctr_online::Json::array();
    const auto request = ctr_online::parse_json(body);
    for (const auto& id : request.at("recipients").items) {
        auto row = ctr_online::Json::object();
        row.set("accountId", ctr_online::Json::of(std::to_string(id.as_u64())));
        row.set("invitationId", ctr_online::Json::of("fixture-" + std::to_string(id.as_u64())));
        receipt.items.push_back(row);
    }
    response->body = ctr_online::dump_json(receipt); return true;
}
}
extern "C" int ctr_hle_invitation_load(int(*)(int));
int main() {
    _putenv_s("CTR_ONLINE", "1");
    SetEnvironmentVariableA("CTR_ONLINE", "1");
    using Call = int(CTR_SYSV*)();
    auto init = bind<Call>("XvA5KS56wcs"), term = bind<Call>("B6HVJtDYxEE");
    auto update = bind<Call>("9+g9iOq+7kg");
    auto close = bind<Call>("WWtCL5lzi7Y");
    check(close != nullptr, "close bound");
    auto open = bind<int(CTR_SYSV*)(const Param*)>("sAxbHhAWMXM");
    auto result = bind<int(CTR_SYSV*)(Result*)>("WuuUhuKOxwQ");
    auto unload = reinterpret_cast<int(CTR_SYSV*)(unsigned short)>(ctr_hle_resolve("eR2bZFAAU0Q", "libSceSysmodule"));
    check(unload != nullptr, "system module unload bound");
    check(ctr_hle_invitation_load(common) == 0 && init() == 0 && used, "shared dialog owner");
    check(unload(0xa2) == static_cast<int>(0x805A10FFu) && used, "initialized dialog cannot unload");
    check(open(nullptr) == static_cast<int>(0x80B8000Du), "null parameter rejected");
    SessionId session{}; std::memcpy(session.data, "0123456789abcdef0123456789abcdef", 33);
    std::uint64_t accounts[]{43,44};
    Data data{}; data.send.session = &session; data.send.address.type = 1;
    data.send.address.info.fixed.accounts = accounts; data.send.address.info.fixed.count = 2;
    Param p{}; p.size = sizeof(p); p.mode = 1; p.data = &data; p.callback = &p;
    data.send.address.type = 2; data.send.address.info.maximum = 0;
    check(open(&p) == static_cast<int>(0x80B8000Au) && requests == 0, "zero picker limit rejected");
    data.send.address.type = 1; data.send.address.info.fixed.accounts = accounts; data.send.address.info.fixed.count = 2;
    accounts[1] = accounts[0];
    check(open(&p) == static_cast<int>(0x80B8000Au), "duplicate recipients rejected");
    accounts[1] = 44;
    // Fixed recipients are sent without any window: only the recipient accepts.
    for (int scenario : {1, 2}) {
        const int before = requests;
        response_status = scenario == 2 ? 400 : 201;
        check(open(&p) == 0, "asynchronous dialog opened");
        for (int i = 0; i < 300 && update() != 3; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        check(update() == 3, "fixed-recipient send completed without a confirmation window");
        if (update() != 3) ExitProcess(3); // Bounded fixture timeout.
        Users users{}; Result out{}; out.sent = &users;
        const int rc = result(&out);
        check(out.callback == &p, "callback preserved");
        check(requests == before + 1, "one send request per invitation");
        if (scenario == 1)
            check(rc == 0 && out.result == 0 && users.count == 2 && users.users[1].account == 44 &&
                  request_body == "{\"sessionId\":\"0123456789abcdef0123456789abcdef\",\"recipients\":[43,44]}",
                  "transport success returns actual recipient IDs");
        else
            check(rc != 0 && out.error != 0 && users.count == 0, "server refusal never reported as sent");
    }
    response_status = 201;
    for (int mode : {1, 2}) {
        p.mode = mode; data.send.address.type = 2; data.send.address.info.maximum = 1;
        check(open(&p) == 0, "picker/receive dialog opened");
        for (int i = 0; i < 300 && update() != 3; ++i) {
            EnumWindows(answer, IDOK);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (update() != 3) ExitProcess(3);
        Users users{}; Result out{}; out.sent = &users;
        check(result(&out) == 0 && !out.error, "picker/receive completed");
        if (mode == 1) check(users.count == 1 && users.users[0].account == 43 &&
            !std::strcmp(users.users[0].online_id, "FixtureFriend"), "picker returns selected identity");
        else {
            struct Event { int type; unsigned char data[8192]; } event{};
            auto event_read = reinterpret_cast<int(CTR_SYSV*)(Event*)>(ctr_hle_resolve("656LMQSrg6U", "libSceSystemService"));
            check(event_read && event_read(&event) == 0 && event.type == 0x10000002 &&
                !std::strcmp(reinterpret_cast<char*>(event.data), session.data) &&
                !std::strcmp(reinterpret_cast<char*>(event.data + 48), "receive-fixture"), "acceptance delivers session invitation event");
            check(event_read(&event) == static_cast<int>(0x80A10004u), "event delivered once");
            std::int32_t recipient_user = 0;
            std::memcpy(&recipient_user, event.data + 140, sizeof(recipient_user));
            check(recipient_user == 1, "retail invitation payload identifies the active local recipient");
        }
    }
    check(term() == 0 && !used, "completed dialog releases owner");
    check(unload(0xa2) == 0, "terminated invitation module unloads");
    check(unload(0xa2) == static_cast<int>(0x805A1001u), "double unload reports not loaded");
    check(init() == static_cast<int>(0x80B80001u), "unloaded dialog cannot initialize");
    check(ctr_hle_invitation_load(common) == 0 && init() == 0 && used,
          "invitation module reloads for the next invite");
    check(term() == 0 && unload(0xa2) == 0 && !used, "reloaded module releases ownership");
    set_owner(common);
    auto drive=[](int milliseconds) {
        const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);
        while(std::chrono::steady_clock::now()<end) {
            tick();EnumWindows(answer,IDYES);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    };
    drive(3500);
    check(notice_prompts==0,"accepted invitation is not prompted again by stale poll");
    fixture_phase=1;notice_prompts=0;
    drive(7000);
    check(accept_attempts==1,"failed automatic acceptance is not silently retried in a popup loop");
    std::printf("invitation_dialog_test failures=%d\n", failures.load());
    return failures ? 1 : 0;
}
