#include "graphics_check.hpp"
#include <cstdio>
int main() {
    DXGI_ADAPTER_DESC1 adapter{};
    adapter.VendorId=0x1414;adapter.DeviceId=0x008c;
    if(!jfg::frontend::software_adapter(adapter))return 1; // Actual Hyper-V descriptor, missing software flag.
    adapter.Flags=DXGI_ADAPTER_FLAG_SOFTWARE;
    if(!jfg::frontend::software_adapter(adapter))return 2;
    adapter.VendorId=0x10de;adapter.DeviceId=0x2b85;adapter.Flags=0;
    if(jfg::frontend::software_adapter(adapter))return 3; // Actual host RTX 5090.
    adapter.VendorId=0x8086;adapter.DeviceId=0x9a49;adapter.DedicatedVideoMemory=0;
    if(jfg::frontend::software_adapter(adapter))return 4; // Shared-memory integrated GPU is valid.
    adapter.VendorId=0x1002;adapter.DeviceId=0x164e;
    if(jfg::frontend::software_adapter(adapter))return 5;
    adapter.Flags=DXGI_ADAPTER_FLAG_REMOTE;
    if(!jfg::frontend::software_adapter(adapter))return 6;
    std::puts("6 graphics adapter checks passed");return 0;
}
