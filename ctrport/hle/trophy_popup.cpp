// SPDX-License-Identifier: GPL-2.0-only
// Trophy notice: a layered, topmost window that does not take focus or input, drawn with
// GDI+ over the top right corner of the game window.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "trophy_popup.h"
#include <windows.h>
#include <shlwapi.h>
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
namespace Gdiplus {
using std::min;
using std::max;
}
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")

namespace ctr_hle::trophy {
namespace {
struct Notice {
    std::vector<unsigned char> png;
    std::wstring title, name;
};
std::mutex mutex;
std::condition_variable wake;
std::deque<Notice> queue;
bool worker_started = false;

struct Search {
    DWORD pid;
    HWND best = nullptr;
    long area = 0;
};
BOOL CALLBACK find_game_window(HWND window, LPARAM raw) {
    auto& s = *reinterpret_cast<Search*>(raw);
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    RECT r{};
    if (pid != s.pid || !IsWindowVisible(window) || IsIconic(window) || !GetClientRect(window, &r)) return TRUE;
    const long area = (r.right - r.left) * (r.bottom - r.top);
    if (area > s.area) {
        s.area = area;
        s.best = window;
    }
    return TRUE;
}

void present(const Notice& notice) {
    using namespace Gdiplus;
    Search search{GetCurrentProcessId()};
    EnumWindows(find_game_window, reinterpret_cast<LPARAM>(&search));
    if (!search.best) return;
    RECT client{};
    GetClientRect(search.best, &client);
    POINT corner{client.right, client.top};
    ClientToScreen(search.best, &corner);
    const float scale = std::max(1.0f, (client.bottom - client.top) / 720.0f);
    const int width = static_cast<int>(380 * scale), height = static_cast<int>(88 * scale),
              margin = static_cast<int>(24 * scale);

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP dib = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!dib) {
        DeleteDC(dc);
        return;
    }
    const HGDIOBJ previous = SelectObject(dc, dib);
    {
        Bitmap canvas(width, height, width * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(bits));
        Graphics g(&canvas);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.Clear(Color(0, 0, 0, 0));
        g.ScaleTransform(scale, scale);
        GraphicsPath shape;
        const REAL r = 14;
        shape.AddArc(0.f, 0.f, 2 * r, 2 * r, 180, 90);
        shape.AddArc(380 - 2 * r - 1, 0.f, 2 * r, 2 * r, 270, 90);
        shape.AddArc(380 - 2 * r - 1, 88 - 2 * r - 1, 2 * r, 2 * r, 0, 90);
        shape.AddArc(0.f, 88 - 2 * r - 1, 2 * r, 2 * r, 90, 90);
        shape.CloseFigure();
        SolidBrush back(Color(235, 27, 20, 70));
        g.FillPath(&back, &shape);
        Pen border(Color(255, 255, 201, 60), 2.f);
        g.DrawPath(&border, &shape);
        if (!notice.png.empty()) {
            if (IStream* stream = SHCreateMemStream(notice.png.data(), static_cast<UINT>(notice.png.size()))) {
                std::unique_ptr<Bitmap> icon(Bitmap::FromStream(stream));
                if (icon && icon->GetLastStatus() == Ok) g.DrawImage(icon.get(), 12.f, 12.f, 64.f, 64.f);
                stream->Release();
            }
        }
        FontFamily family(L"Segoe UI");
        Font title(&family, 12, FontStyleBold, UnitPixel), name(&family, 16, FontStyleBold, UnitPixel);
        SolidBrush gold(Color(255, 255, 201, 60)), light(Color(255, 255, 248, 236));
        StringFormat format;
        format.SetTrimming(StringTrimmingEllipsisCharacter);
        g.DrawString(notice.title.c_str(), -1, &title, RectF(88, 14, 280, 18), &format, &gold);
        g.DrawString(notice.name.c_str(), -1, &name, RectF(88, 34, 280, 44), &format, &light);
    }
    HWND window =
        CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                        L"STATIC", L"", WS_POPUP, corner.x - width - margin, corner.y + margin, width, height, nullptr,
                        nullptr, GetModuleHandleW(nullptr), nullptr);
    if (window) {
        POINT origin{corner.x - width - margin, corner.y + margin}, source{0, 0};
        SIZE size{width, height};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(window, nullptr, &origin, &size, dc, &source, 0, &blend, ULW_ALPHA);
        ShowWindow(window, SW_SHOWNOACTIVATE);
        const ULONGLONG start = GetTickCount64();
        for (;;) {
            const ULONGLONG elapsed = GetTickCount64() - start;
            if (elapsed >= 5000) break;
            if (elapsed > 4600) {
                blend.SourceConstantAlpha = static_cast<BYTE>(255 * (5000 - elapsed) / 400);
                UpdateLayeredWindow(window, nullptr, nullptr, nullptr, nullptr, nullptr, 0, &blend, ULW_ALPHA);
            }
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            Sleep(30);
        }
        DestroyWindow(window);
    }
    SelectObject(dc, previous);
    DeleteObject(dib);
    DeleteDC(dc);
}

void worker() {
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) return;
    for (;;) {
        Notice notice;
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait(lock, [] { return !queue.empty(); });
            notice = std::move(queue.front());
            queue.pop_front();
        }
        present(notice);
    }
}
} // namespace

void show_popup(std::vector<unsigned char> png, std::wstring title, std::wstring name) {
    std::lock_guard<std::mutex> lock(mutex);
    queue.push_back({std::move(png), std::move(title), std::move(name)});
    if (!worker_started) {
        worker_started = true;
        std::thread(worker).detach();
    }
    wake.notify_one();
}
}
