// SPDX-License-Identifier: GPL-2.0-only
// Exact-window GPU capture using the installed Windows Graphics Capture API.
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <fstream>
#include <iostream>
#include <filesystem>
struct Target {
    DWORD pid;
    HWND window = nullptr;
};
BOOL CALLBACK find_target(HWND w, LPARAM raw) {
    auto& t = *reinterpret_cast<Target*>(raw);
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    RECT r{};
    GetClientRect(w, &r);
    if (pid == t.pid && IsWindowVisible(w) && !IsIconic(w) && r.right >= 640 && r.bottom >= 360) {
        t.window = w;
        return FALSE;
    }
    return TRUE;
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 60;
    wchar_t* end = nullptr;
    const unsigned long pid = wcstoul(argv[1], &end, 10);
    if (!pid || !end || *end) return 60;
    Target target{pid};
    EnumWindows(find_target, reinterpret_cast<LPARAM>(&target));
    if (!target.window) return 62;
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        using namespace winrt::Windows::Graphics;
        if (!Capture::GraphicsCaptureSession::IsSupported()) return 88;
        winrt::com_ptr<ID3D11Device> device;
        winrt::com_ptr<ID3D11DeviceContext> context;
        winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                               D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                                               device.put(), nullptr, context.put()));
        auto dxgi = device.as<IDXGIDevice>();
        winrt::com_ptr<IInspectable> inspectable;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), inspectable.put()));
        auto direct = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
        auto interop = winrt::get_activation_factory<Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
        Capture::GraphicsCaptureItem item{nullptr};
        winrt::check_hresult(interop->CreateForWindow(target.window, winrt::guid_of<Capture::GraphicsCaptureItem>(),
                                                      winrt::put_abi(item)));
        auto pool = Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
            direct, winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, item.Size());
        auto session = pool.CreateCaptureSession(item);
        session.StartCapture();
        Capture::Direct3D11CaptureFrame frame{nullptr};
        const auto deadline = GetTickCount64() + 5000;
        while (!frame && GetTickCount64() < deadline) {
            frame = pool.TryGetNextFrame();
            if (!frame) Sleep(20);
        }
        if (!frame) return 4;
        DWORD current = 0;
        GetWindowThreadProcessId(target.window, &current);
        if (current != target.pid || IsIconic(target.window)) return 62;
        auto access = frame.Surface().as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        winrt::com_ptr<ID3D11Texture2D> source;
        winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D), source.put_void()));
        D3D11_TEXTURE2D_DESC desc{};
        source->GetDesc(&desc);
        const auto size = frame.ContentSize();
        if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM || size.Width <= 0 || size.Height <= 0 ||
            static_cast<UINT>(size.Width) > desc.Width || static_cast<UINT>(size.Height) > desc.Height)
            return 88;
        desc.Width = static_cast<UINT>(size.Width);
        desc.Height = static_cast<UINT>(size.Height);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        winrt::com_ptr<ID3D11Texture2D> staging;
        winrt::check_hresult(device->CreateTexture2D(&desc, nullptr, staging.put()));
        D3D11_BOX box{0, 0, 0, desc.Width, desc.Height, 1};
        context->CopySubresourceRegion(staging.get(), 0, 0, 0, 0, source.get(), 0, &box);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        winrt::check_hresult(context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped));
        BITMAPFILEHEADER header{};
        BITMAPINFOHEADER info{};
        info.biSize = sizeof(info);
        info.biWidth = size.Width;
        info.biHeight = -size.Height;
        info.biPlanes = 1;
        info.biBitCount = 32;
        info.biCompression = BI_RGB;
        info.biSizeImage = desc.Width * desc.Height * 4;
        header.bfType = 0x4d42;
        header.bfOffBits = sizeof(header) + sizeof(info);
        header.bfSize = header.bfOffBits + info.biSizeImage;
        std::ofstream output(std::filesystem::path(argv[2]), std::ios::binary);
        output.write(reinterpret_cast<char*>(&header), sizeof(header));
        output.write(reinterpret_cast<char*>(&info), sizeof(info));
        for (UINT y = 0; y < desc.Height; ++y)
            output.write(static_cast<const char*>(mapped.pData) + y * mapped.RowPitch, desc.Width * 4);
        context->Unmap(staging.get(), 0);
        session.Close();
        pool.Close();
        if (!output) return 7;
        std::cout << "WGC whole-window capture " << size.Width << "x" << size.Height << " pid=" << pid << "\n";
        return 0;
    } catch (const winrt::hresult_error& error) {
        std::cerr << "WGC HRESULT=0x" << std::hex << static_cast<unsigned long>(error.code().value) << " pid=" << pid
                  << "\n";
        return 1;
    }
}
