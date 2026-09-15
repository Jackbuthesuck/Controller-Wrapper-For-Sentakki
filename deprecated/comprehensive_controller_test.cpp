#include <windows.h>
#include <XInput.h>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "XInput.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

// DirectInput includes
#include <dinput.h>

int main() {
    std::cout << "Comprehensive Controller Detection Test" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    // Test XInput controllers
    std::cout << "\n1. XInput Controllers (Xbox controllers):" << std::endl;
    bool xinputFound = false;
    for (int i = 0; i < 4; i++) {
        XINPUT_STATE state;
        DWORD result = XInputGetState(i, &state);
        
        std::cout << "  Controller " << (i + 1) << ": ";
        if (result == ERROR_SUCCESS) {
            std::cout << "CONNECTED" << std::endl;
            xinputFound = true;
        } else if (result == ERROR_DEVICE_NOT_CONNECTED) {
            std::cout << "NOT CONNECTED" << std::endl;
        } else {
            std::cout << "ERROR: " << result << std::endl;
        }
    }
    
    // Test DirectInput controllers
    std::cout << "\n2. DirectInput Controllers (Generic controllers):" << std::endl;
    LPDIRECTINPUT8 di = nullptr;
    HRESULT hr = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&di, nullptr);
    
    if (SUCCEEDED(hr)) {
        std::cout << "  DirectInput initialized successfully" << std::endl;
        
        // Enumerate joysticks
        hr = di->EnumDevices(DI8DEVCLASS_GAMECTRL, [](LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef) -> BOOL {
            std::cout << "  Found controller: " << lpddi->tszProductName << std::endl;
            std::cout << "    Instance GUID: " << lpddi->guidInstance.Data1 << std::endl;
            return DIENUM_CONTINUE;
        }, nullptr, DIEDFL_ATTACHEDONLY);
        
        if (FAILED(hr)) {
            std::cout << "  Failed to enumerate devices: " << hr << std::endl;
        }
        
        di->Release();
    } else {
        std::cout << "  Failed to initialize DirectInput: " << hr << std::endl;
    }
    
    // Check Windows Game Controller settings
    std::cout << "\n3. Windows Game Controller Status:" << std::endl;
    std::cout << "  Check Windows Settings > Gaming > Xbox Networking" << std::endl;
    std::cout << "  Check Device Manager for any controller devices" << std::endl;
    
    // Troubleshooting steps
    std::cout << "\n4. Troubleshooting Steps:" << std::endl;
    std::cout << "  a) Make sure your controller is connected via USB or Bluetooth" << std::endl;
    std::cout << "  b) For Xbox controllers, try pressing the Xbox button" << std::endl;
    std::cout << "  c) Check Device Manager for any yellow warning icons" << std::endl;
    std::cout << "  d) Try running as Administrator" << std::endl;
    std::cout << "  e) Restart your computer" << std::endl;
    std::cout << "  f) Update controller drivers" << std::endl;
    
    if (!xinputFound) {
        std::cout << "\n5. Alternative Solutions:" << std::endl;
        std::cout << "  - Try a different USB port" << std::endl;
        std::cout << "  - Use Xbox Accessories app to update controller firmware" << std::endl;
        std::cout << "  - Check if controller works in other games" << std::endl;
        std::cout << "  - Try a different controller if available" << std::endl;
    }
    
    std::cout << "\nPress any key to exit..." << std::endl;
    std::cin.get();
    
    return 0;
}
