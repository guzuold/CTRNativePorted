// SPDX-License-Identifier: GPL-2.0-or-later
#include "invitation_ui.h"
#include <windows.h>
#include <mutex>
#include <stdexcept>
namespace ctr_hle::invitation {
namespace {
struct State { HWND list = nullptr; unsigned maximum = 1; std::vector<int> selected; bool done = false; };
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<State*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state) return DefWindowProcW(window, message, wparam, lparam);
    if (message == WM_COMMAND && LOWORD(wparam) == IDOK) {
        const auto count = SendMessageW(state->list, LB_GETSELCOUNT, 0, 0);
        if (count <= 0 || static_cast<unsigned>(count) > state->maximum) {
            const auto message = L"Seleziona da 1 a " + std::to_wstring(state->maximum) + L" destinatari.";
            MessageBoxW(window, message.c_str(), L"CTR", MB_OK);
            return 0;
        }
        state->selected.resize(static_cast<std::size_t>(count));
        SendMessageW(state->list, LB_GETSELITEMS, count, reinterpret_cast<LPARAM>(state->selected.data()));
        DestroyWindow(window); return 0;
    }
    if (message == WM_CLOSE || (message == WM_COMMAND && LOWORD(wparam) == IDCANCEL)) {
        state->selected.clear(); DestroyWindow(window); return 0;
    }
    if (message == WM_DESTROY) { state->done = true; return 0; }
    return DefWindowProcW(window, message, wparam, lparam);
}
}
std::vector<int> choose(const std::vector<std::wstring>& rows, unsigned maximum,
                       const wchar_t* title, const wchar_t* action) {
    static std::once_flag once;
    std::call_once(once, [] {
        WNDCLASSW cls{}; cls.lpfnWndProc = procedure; cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpszClassName = L"CTRInvitationPicker"; cls.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("cannot register invitation picker");
    });
    State state; state.maximum = maximum;
    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CTRInvitationPicker", title,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 520, 430,
        nullptr, nullptr, GetModuleHandleW(nullptr), &state);
    if (!window) throw std::runtime_error("cannot create invitation picker");
    state.list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_EXTENDEDSEL | LBS_NOINTEGRALHEIGHT,
        14, 14, 475, 310, window, reinterpret_cast<HMENU>(100), GetModuleHandleW(nullptr), nullptr);
    auto ok = CreateWindowW(L"BUTTON", action, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        265, 342, 110, 30, window, reinterpret_cast<HMENU>(IDOK), GetModuleHandleW(nullptr), nullptr);
    auto cancel = CreateWindowW(L"BUTTON", L"Annulla", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        385, 342, 105, 30, window, reinterpret_cast<HMENU>(IDCANCEL), GetModuleHandleW(nullptr), nullptr);
    if (!state.list || !ok || !cancel) { DestroyWindow(window); throw std::runtime_error("cannot create picker controls"); }
    for (HWND control : {state.list, ok, cancel})
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    for (const auto& row : rows) SendMessageW(state.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.c_str()));
    ShowWindow(window, SW_SHOW); SetForegroundWindow(window); SetFocus(state.list);
    MSG message{};
    while (!state.done && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if (IsWindow(window)) DestroyWindow(window);
    return state.selected;
}
}

