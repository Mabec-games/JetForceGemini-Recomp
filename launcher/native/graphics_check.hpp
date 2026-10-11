#pragma once
#include <windows.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace jfg::frontend {
enum class GraphicsAvailability { hardware, software_only, query_failed };

inline bool software_adapter(const DXGI_ADAPTER_DESC1 &adapter) noexcept {
    // Hyper-V can expose Basic Render Driver as adapter 0 with Flags == 0.
    // Checking only DXGI_ADAPTER_FLAG_SOFTWARE lets WARP reach RT64's compiler.
    return (adapter.Flags & (DXGI_ADAPTER_FLAG_SOFTWARE | DXGI_ADAPTER_FLAG_REMOTE)) != 0 ||
           (adapter.VendorId == 0x1414 && adapter.DeviceId == 0x008c);
}

inline GraphicsAvailability graphics_availability() noexcept {
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()))))
        return GraphicsAvailability::query_failed;
    for (UINT index = 0;; ++index) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        const HRESULT result = factory->EnumAdapters1(index, adapter.GetAddressOf());
        if (result == DXGI_ERROR_NOT_FOUND)
            return GraphicsAvailability::software_only;
        if (FAILED(result))
            return GraphicsAvailability::query_failed;
        DXGI_ADAPTER_DESC1 description{};
        if (FAILED(adapter->GetDesc1(&description)))
            return GraphicsAvailability::query_failed;
        if (!software_adapter(description))
            return GraphicsAvailability::hardware;
    }
}

inline const wchar_t *graphics_message(GraphicsAvailability result) noexcept {
    if (result == GraphicsAvailability::software_only)
        return L"The game needs a hardware graphics device. Windows is currently exposing only software graphics.\n\n"
               L"In a virtual machine, enable access to a compatible GPU or play on your PC instead. "
               L"You can still select your ROM and verify the game files here.";
    return L"Windows could not list the available graphics devices. Check your graphics driver and restart the launcher.";
}
} // namespace jfg::frontend
