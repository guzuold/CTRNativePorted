// SPDX-License-Identifier: GPL-2.0-only
// GPU-safe desktop duplication, persist only the requested process client area.
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <fstream>
#include <vector>
#include <iostream>
using Microsoft::WRL::ComPtr;
struct Target {
    DWORD pid;
    HWND window = nullptr;
};
BOOL CALLBACK find_window(HWND w, LPARAM raw) {
    auto& t = *reinterpret_cast<Target*>(raw);
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid == t.pid && IsWindowVisible(w) && !IsIconic(w)) {
        RECT r{};
        GetClientRect(w, &r);
        if (r.right >= 640 && r.bottom >= 360) {
            t.window = w;
            return FALSE;
        }
    }
    return TRUE;
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 60;
    Target t{static_cast<DWORD>(_wtoi(argv[1]))};
    EnumWindows(find_window, reinterpret_cast<LPARAM>(&t));
    if (!t.window) return 62;
    RECT crop{};
    GetClientRect(t.window, &crop);
    POINT point{};
    ClientToScreen(t.window, &point);
    OffsetRect(&crop, point.x, point.y);
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return 1;
    for (UINT a = 0;; ++a) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(a, &adapter) == DXGI_ERROR_NOT_FOUND) break;
        for (UINT n = 0;; ++n) {
            ComPtr<IDXGIOutput> output;
            if (adapter->EnumOutputs(n, &output) == DXGI_ERROR_NOT_FOUND) break;
            DXGI_OUTPUT_DESC desc{};
            output->GetDesc(&desc);
            auto r = desc.DesktopCoordinates;
            if (crop.left < r.left || crop.top < r.top || crop.right > r.right || crop.bottom > r.bottom) continue;
            if (desc.Rotation != DXGI_MODE_ROTATION_IDENTITY) {
                std::cerr << "DXGI target output rotation=" << desc.Rotation << " pid=" << t.pid << "\n";
                return 88;
            }
            ComPtr<ID3D11Device> device;
            ComPtr<ID3D11DeviceContext> context;
            if (FAILED(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0,
                                         D3D11_SDK_VERSION, &device, nullptr, &context)))
                return 2;
            ComPtr<IDXGIOutput1> out1;
            output.As(&out1);
            ComPtr<IDXGIOutputDuplication> duplicate;
            const HRESULT duplicated = out1->DuplicateOutput(device.Get(), &duplicate);
            if (FAILED(duplicated)) {
                std::cerr << "DXGI DuplicateOutput HRESULT=0x" << std::hex << static_cast<unsigned long>(duplicated)
                          << " pid=" << std::dec << t.pid << "\n";
                return 3;
            }
            DXGI_OUTDUPL_FRAME_INFO info{};
            ComPtr<IDXGIResource> resource;
            HRESULT acquired = duplicate->AcquireNextFrame(5000, &info, &resource);
            if (FAILED(acquired)) return 4;
            ComPtr<ID3D11Texture2D> source;
            resource.As(&source);
            D3D11_TEXTURE2D_DESC td{};
            source->GetDesc(&td);
            if (td.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
                std::cerr << "DXGI target texture format=" << td.Format << " pid=" << t.pid << "\n";
                duplicate->ReleaseFrame();
                return 88;
            }
            td.Width = static_cast<UINT>(crop.right - crop.left);
            td.Height = static_cast<UINT>(crop.bottom - crop.top);
            td.Usage = D3D11_USAGE_STAGING;
            td.BindFlags = 0;
            td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            td.MiscFlags = 0;
            ComPtr<ID3D11Texture2D> staging;
            if (FAILED(device->CreateTexture2D(&td, nullptr, &staging))) {
                duplicate->ReleaseFrame();
                return 5;
            }
            D3D11_BOX box{static_cast<UINT>(crop.left - r.left),  static_cast<UINT>(crop.top - r.top),    0,
                          static_cast<UINT>(crop.right - r.left), static_cast<UINT>(crop.bottom - r.top), 1};
            context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, source.Get(), 0, &box);
            duplicate->ReleaseFrame();
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return 6;
            BITMAPFILEHEADER header{};
            BITMAPINFOHEADER image{};
            image.biSize = sizeof(image);
            image.biWidth = static_cast<LONG>(td.Width);
            image.biHeight = -static_cast<LONG>(td.Height);
            image.biPlanes = 1;
            image.biBitCount = 32;
            image.biCompression = BI_RGB;
            image.biSizeImage = td.Width * td.Height * 4;
            header.bfType = 0x4d42;
            header.bfOffBits = sizeof(header) + sizeof(image);
            header.bfSize = header.bfOffBits + image.biSizeImage;
            std::ofstream file(argv[2], std::ios::binary);
            file.write(reinterpret_cast<char*>(&header), sizeof(header));
            file.write(reinterpret_cast<char*>(&image), sizeof(image));
            for (UINT y = 0; y < td.Height; ++y)
                file.write(static_cast<const char*>(mapped.pData) + y * mapped.RowPitch, td.Width * 4);
            context->Unmap(staging.Get(), 0);
            if (!file) return 7;
            std::cout << "DXGI client capture " << td.Width << "x" << td.Height << " pid " << t.pid << "\n";
            return 0;
        }
    }
    std::cerr << "DXGI no output contains target client rect=" << crop.left << "," << crop.top << "," << crop.right
              << "," << crop.bottom << " pid=" << t.pid << "\n";
    return 88;
}
