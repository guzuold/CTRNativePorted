// SPDX-License-Identifier: GPL-2.0-or-later
#include "friends_dialog.hpp"
#include "online_server/friend_identity.hpp"
#include "online_server/friend_store.hpp"
#include "online_server/friend_verifier.hpp"
#include "online_server/launcher_online.hpp"
#include <commctrl.h>
#include <memory>
#include <future>
#include <cstring>
#include <thread>
#include <vector>

namespace ctr_online {
namespace {

constexpr UINT WM_SEARCH_RESULT = WM_APP + 10;

std::wstring to_wide(const std::string& str) {
    if (str.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), out.data(), n);
    return out;
}

std::string to_narrow(const std::wstring& str) {
    if (str.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), out.data(), n, nullptr, nullptr);
    return out;
}

void copy_to_clipboard(HWND owner, const std::wstring& text) {
    if (!OpenClipboard(owner)) return;
    EmptyClipboard();
    const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        if (void* data = GlobalLock(hMem)) {
            std::memcpy(data, text.c_str(), bytes);
            GlobalUnlock(hMem);
            if (!SetClipboardData(CF_UNICODETEXT, hMem)) GlobalFree(hMem);
        } else GlobalFree(hMem);
    }
    CloseClipboard();
}

struct DialogState {
    std::filesystem::path root;
    std::string my_code;
    FriendStore store;
    std::vector<FriendEntry> cached_friends;
    std::wstring selected_ip;
    bool chosen = false;
    bool search_running = false;
    DhtService* shared_dht = nullptr;
    struct Result { int action; std::string code, detail; PartySelection party; Json snapshot=Json::object(); bool ok=false; };
    std::future<Result> result;
    PartySelection selection;
    std::string player_name;
    std::string searching_code;

    HWND dlg = nullptr;
    HWND my_code_edit = nullptr;
    HWND copy_btn = nullptr;
    HWND listbox = nullptr;
    HWND search_btn = nullptr;
    HWND use_btn = nullptr, invite_btn = nullptr, inbox_btn = nullptr, new_btn = nullptr;
    HWND remove_btn = nullptr;
    HWND nick_edit = nullptr;
    HWND code_edit = nullptr;
    HWND add_btn = nullptr;
    HWND status_lbl = nullptr;
};

void refresh_list(DialogState& state) {
    const auto selected=SendMessageW(state.listbox, LB_GETCURSEL,0,0);
    state.cached_friends = state.store.entries();
    SendMessageW(state.listbox, LB_RESETCONTENT, 0, 0);
    for (const auto& f : state.cached_friends) {
        std::wstring line = to_wide(f.nickname) + L" · " + to_wide(f.friend_code.substr(0, 15)) + L"...";
        if (!f.last_endpoint.empty()) {
            line += L" · [" + to_wide(f.last_endpoint) + L"]";
        }
        if (!f.last_status.empty()) {
            line += L" (" + to_wide(f.last_status) + L")";
        }
        SendMessageW(state.listbox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
    }
    if (selected>=0) SendMessageW(state.listbox,LB_SETCURSEL,selected,0);
    const bool has_items = !state.cached_friends.empty();
    EnableWindow(state.search_btn, has_items && !state.search_running);
    EnableWindow(state.use_btn, has_items && !state.search_running);
    EnableWindow(state.remove_btn, has_items && !state.search_running);
    EnableWindow(state.invite_btn, has_items && !state.search_running);
    EnableWindow(state.inbox_btn, !state.search_running);
    EnableWindow(state.new_btn, !state.search_running);
}

LRESULT CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = static_cast<DialogState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        state->dlg = hwnd;

        HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        // Group 1: My friend code
        CreateWindowExW(0, L"STATIC", L"Il tuo codice amico (condividilo per aggiungervi reciprocamente):",
                        WS_CHILD | WS_VISIBLE, 16, 12, 480, 18, hwnd, nullptr, nullptr, nullptr);
        state->my_code_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", to_wide(state->my_code).c_str(),
                                              WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL,
                                              16, 32, 400, 24, hwnd, nullptr, nullptr, nullptr);
        state->copy_btn = CreateWindowExW(0, L"BUTTON", L"Copia", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                          424, 32, 76, 24, hwnd, reinterpret_cast<HMENU>(101), nullptr, nullptr);

        // Group 2: Saved friends
        CreateWindowExW(0, L"STATIC", L"Rubrica amici:", WS_CHILD | WS_VISIBLE, 16, 68, 480, 18, hwnd, nullptr, nullptr, nullptr);
        state->listbox = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                         WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
                                         16, 88, 484, 130, hwnd, reinterpret_cast<HMENU>(102), nullptr, nullptr);

        state->search_btn = CreateWindowExW(0, L"BUTTON", L"Aggiorna stato", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                           16, 226, 140, 28, hwnd, reinterpret_cast<HMENU>(103), nullptr, nullptr);
        state->use_btn = CreateWindowExW(0, L"BUTTON", L"Entra nella lobby", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                         164, 226, 140, 28, hwnd, reinterpret_cast<HMENU>(104), nullptr, nullptr);
        state->remove_btn = CreateWindowExW(0, L"BUTTON", L"Rimuovi amico", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                           360, 226, 140, 28, hwnd, reinterpret_cast<HMENU>(105), nullptr, nullptr);

        state->invite_btn = CreateWindowExW(0,L"BUTTON",L"Invita nella lobby",WS_CHILD|WS_VISIBLE|WS_TABSTOP,
            16,264,140,28,hwnd,reinterpret_cast<HMENU>(107),nullptr,nullptr);
        state->inbox_btn = CreateWindowExW(0,L"BUTTON",L"Inviti ricevuti",WS_CHILD|WS_VISIBLE|WS_TABSTOP,
            164,264,140,28,hwnd,reinterpret_cast<HMENU>(108),nullptr,nullptr);
        state->new_btn = CreateWindowExW(0,L"BUTTON",L"Nuova lobby",WS_CHILD|WS_VISIBLE|WS_TABSTOP,
            360,264,140,28,hwnd,reinterpret_cast<HMENU>(109),nullptr,nullptr);
        // Group 3: Add new friend
        CreateWindowExW(0, L"STATIC", L"Aggiungi amico alla rubrica:", WS_CHILD | WS_VISIBLE, 16, 308, 480, 18, hwnd, nullptr, nullptr, nullptr);
        CreateWindowExW(0, L"STATIC", L"Soprannome:", WS_CHILD | WS_VISIBLE, 16, 332, 100, 18, hwnd, nullptr, nullptr, nullptr);
        state->nick_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                           120, 330, 160, 24, hwnd, nullptr, nullptr, nullptr);

        CreateWindowExW(0, L"STATIC", L"Codice amico:", WS_CHILD | WS_VISIBLE, 16, 362, 100, 18, hwnd, nullptr, nullptr, nullptr);
        state->code_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                           120, 360, 380, 24, hwnd, nullptr, nullptr, nullptr);

        state->add_btn = CreateWindowExW(0, L"BUTTON", L"Salva in rubrica", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        120, 392, 140, 28, hwnd, reinterpret_cast<HMENU>(106), nullptr, nullptr);

        // Status bar / notification text
        state->status_lbl = CreateWindowExW(0, L"STATIC", L"Lobby: massimo 8 giocatori. Invita un amico o entra nella sua lobby.",
                                            WS_CHILD | WS_VISIBLE, 16, 440, 484, 38, hwnd, nullptr, nullptr, nullptr);

        std::wstring roster=L"Lobby attuale (massimo 8):\n";
        if(const auto* people=state->selection.snapshot.find("members"))
            for(const auto& member:people->items) roster+=to_wide(member.at("name").text)+L"   ";
        HWND roster_label=CreateWindowExW(0,L"STATIC",roster.c_str(),WS_CHILD|WS_VISIBLE,16,484,484,62,hwnd,nullptr,nullptr,nullptr);
        SendMessageW(roster_label,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        // Apply font
        for (HWND child : {state->my_code_edit, state->copy_btn, state->listbox, state->search_btn, state->use_btn,
                           state->invite_btn, state->inbox_btn, state->new_btn, state->remove_btn, state->nick_edit, state->code_edit, state->add_btn, state->status_lbl}) {
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }

        refresh_list(*state);
        return 0;
    }
    case WM_COMMAND: {
        if (!state) break;
        const int id = LOWORD(wParam);
        if (id == 101) { // Copy my code
            copy_to_clipboard(hwnd, to_wide(state->my_code));
            SetWindowTextW(state->status_lbl, L"Codice amico copiato negli appunti!");
        } else if (id==103 || id==104 || id==107 || id==108 || id==109) {
            if (state->search_running) return 0;
            const int sel=static_cast<int>(SendMessageW(state->listbox,LB_GETCURSEL,0,0));
            if (id!=108 && id!=109 && (sel<0 || sel>=static_cast<int>(state->cached_friends.size()))) return 0;
            const std::string code=(id==108 || id==109) ? "" : state->cached_friends[sel].friend_code;
            state->search_running=true;
            refresh_list(*state);
            EnableWindow(state->add_btn,FALSE);
            SetWindowTextW(state->status_lbl,L"Collegamento in corso...");
            state->result=std::async(std::launch::async,[root=state->root,code,id,dht=state->shared_dht,
                                                        selection=state->selection,name=state->player_name] {
                DialogState::Result out; out.action=id; out.code=code;
                try {
                    const auto self=local_friend_code(root);
                    auto command=[](const char* action) { Json j=Json::object();j.set("action",Json::of(action));return j; };
                    if(id==108) {
                        out.snapshot=party_request(root,"127.0.0.1",self,command("status"));
                    } else if(id==109) {
                        if(selection.owner!=self) { try {party_request(root,selection.ip,selection.owner,command("leave"));} catch(...) {} }
                        party_request(root,"127.0.0.1",self,command("new"));
                        Json req=command("configure");req.set("name",Json::of(name));
                        out.snapshot=party_request(root,"127.0.0.1",self,req);
                        out.party={"127.0.0.1",self,out.snapshot.at("room").text,out.snapshot};
                    } else {
                        if(code==self) throw std::runtime_error("Sei gia nella tua lobby: seleziona un altro amico");
                        auto found=find_friend_server(root,code,8000,3000,{},dht);
                        if(found.status!=VerifyStatus::Verified) throw std::runtime_error(found.detail);
                        auto status=party_request(root,found.ip,code,command("status"));
                        out.snapshot=status;
                        if(id==107) {
                            Json invite=command("invite");invite.set("target",Json::of(code));
                            party_request(root,selection.ip,selection.owner,invite);
                            Json notify=command("notify");notify.set("owner",Json::of(selection.owner));
                            party_request(root,found.ip,code,notify);
                        } else if(id==104) {
                            const std::string owner=status.at("owner").text;
                            if(owner==selection.owner) throw std::runtime_error("Sei gia nella stessa lobby di questo amico");
                            if(owner!=code) {
                                found=find_friend_server(root,owner,8000,3000,{},dht);
                                if(found.status!=VerifyStatus::Verified) throw std::runtime_error(found.detail);
                            }
                            Json join=command("join");join.set("name",Json::of(name));
                            out.snapshot=party_request(root,found.ip,owner,join);
                            try {
                                Json dest=command("destination");dest.set("owner",Json::of(owner));
                                party_request(root,"127.0.0.1",self,dest);
                            } catch (...) { party_request(root,found.ip,owner,command("leave"));throw; }
                            if(selection.owner!=self) {
                                try {party_request(root,selection.ip,selection.owner,command("leave"));} catch(...) {}
                            }
                            out.party={found.ip,owner,out.snapshot.at("room").text,out.snapshot};
                        }
                    }
                    out.ok=true;
                } catch(const std::exception& e) {out.detail=e.what();}
                return out;
            });
            SetTimer(hwnd,1,100,nullptr);
        } else if (id == 105) { // Remove friend
            const int sel = static_cast<int>(SendMessageW(state->listbox, LB_GETCURSEL, 0, 0));
            if (sel >= 0 && sel < static_cast<int>(state->cached_friends.size())) {
                const std::string code = state->cached_friends[sel].friend_code;
                state->store.remove(code);
                refresh_list(*state);
                SetWindowTextW(state->status_lbl, L"Amico rimosso dalla rubrica.");
            }
        } else if (id == 106) { // Add friend
            wchar_t nick_buf[128] = {}, code_buf[256] = {};
            GetWindowTextW(state->nick_edit, nick_buf, 128);
            GetWindowTextW(state->code_edit, code_buf, 256);
            std::string err;
            if (state->store.add_or_update(to_narrow(code_buf), to_narrow(nick_buf), &err)) {
                SetWindowTextW(state->nick_edit, L"");
                SetWindowTextW(state->code_edit, L"");
                refresh_list(*state);
                SetWindowTextW(state->status_lbl, L"Amico salvato con successo nella rubrica!");
            } else {
                MessageBoxW(hwnd, (L"Codice non valido:\n" + to_wide(err)).c_str(), L"Errore codice amico", MB_ICONERROR);
            }
        }
        break;
    }
    case WM_TIMER:
        if (state && state->search_running && state->result.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            KillTimer(hwnd, 1);
            auto result = state->result.get();
            SendMessageW(hwnd, WM_SEARCH_RESULT, 0, reinterpret_cast<LPARAM>(&result));
        }
        return 0;
    case WM_SEARCH_RESULT: {
        if(!state) break;
        const auto& result=*reinterpret_cast<DialogState::Result*>(lParam);
        state->search_running=false;
        state->store.load(state->root / "runtime/user/online/friends.json");
        EnableWindow(state->add_btn,TRUE);
        refresh_list(*state);
        if(!result.ok) { SetWindowTextW(state->status_lbl,to_wide(result.detail).c_str());return 0; }
        if(result.action==104 || result.action==109) {
            state->selection=result.party; state->chosen=true; DestroyWindow(hwnd);
        } else if(result.action==107) {
            SetWindowTextW(state->status_lbl,L"Invito inviato. L'amico puo accettarlo da Inviti ricevuti.");
        } else if(result.action==108) {
            const auto& invitations=result.snapshot.at("invitations").items;
            if(invitations.empty()) SetWindowTextW(state->status_lbl,L"Nessun invito ricevuto.");
            else {
                // Keep invitations visible in the address book: accepting uses the
                // same authenticated join action and rechecks owner capacity.
                const auto& invitation=invitations.back();
                const std::string owner=invitation.at("owner").text;
                if(MessageBoxW(hwnd,L"Hai ricevuto un invito. Entrare nella lobby?",L"Invito lobby",MB_YESNO|MB_ICONQUESTION)==IDYES) {
                    state->store.add_or_update(owner,"Lobby invitata");refresh_list(*state);
                    for(std::size_t i=0;i<state->cached_friends.size();++i)
                        if(state->cached_friends[i].friend_code==owner) SendMessageW(state->listbox,LB_SETCURSEL,i,0);
                    SendMessageW(hwnd,WM_COMMAND,104,0);
                }
            }
        } else {
            const auto count=result.snapshot.at("count").as_u64();
            for(const auto& entry:state->store.entries()) if(entry.friend_code==result.code)
                state->store.update_endpoint(result.code,entry.last_endpoint,"Online - lobby " + std::to_string(count) + "/8 (ultimo controllo)");
            refresh_list(*state);
            SetWindowTextW(state->status_lbl,(L"Amico online · Lobby " + std::to_wstring(count) + L"/8").c_str());
        }
        return 0;
    }
    case WM_CLOSE:
        // Keep the host-owned DHT alive until the bounded search has returned.
        if (state && state->search_running) {
            SetWindowTextW(state->status_lbl, L"Ricerca in corso: attendi il risultato prima di chiudere.");
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

bool show_friends_dialog(HWND parent, const std::filesystem::path& root, PartySelection& selection,
                         const std::string& player_name, DhtService* shared_dht) {
    DialogState state;
    state.root = root;
    state.selection=selection;
    state.player_name=player_name;
    state.shared_dht = shared_dht;
    state.my_code = local_friend_code(root);
    const std::filesystem::path friends_path = root / "runtime" / "user" / "online" / "friends.json";
    state.store.load(friends_path);

    WNDCLASSW wc{};
    wc.lpfnWndProc = DlgProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"CTRFriendsDialog";
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);

    RECT pr{};
    GetWindowRect(parent, &pr);
    const int w = 530, h = 595;
    const int x = pr.left + (pr.right - pr.left - w) / 2;
    const int y = pr.top + (pr.bottom - pr.top - h) / 2;

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CTRFriendsDialog", L"Amici e lobby",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                               x, y, w, h, parent, nullptr, GetModuleHandleW(nullptr), &state);

    if (!dlg) return false;

    EnableWindow(parent, FALSE);
    MSG msg{};
    while (IsWindow(dlg)) {
        const int result = GetMessageW(&msg, nullptr, 0, 0);
        if (result <= 0) { if (result == 0) PostQuitMessage(static_cast<int>(msg.wParam)); break; }
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);

    if (state.chosen) { selection=state.selection;return true; }
    return false;
}

} // namespace ctr_online
