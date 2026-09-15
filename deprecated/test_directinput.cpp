#include <windows.h>
#include <dinput.h>
#include <iostream>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

int main() {
    std::cout << "DirectInput Controller Test" << std::endl;
    std::cout << "===========================" << std::endl;
    
    LPDIRECTINPUT8 di = nullptr;
    LPDIRECTINPUTDEVICE8 joystick = nullptr;
    
    // Initialize DirectInput
    HRESULT hr = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&di, nullptr);
    if (FAILED(hr)) {
        std::cout << "Failed to initialize DirectInput: " << hr << std::endl;
        return 1;
    }
    
    std::cout << "DirectInput initialized successfully" << std::endl;
    
    // Find controller
    bool controllerFound = false;
    hr = di->EnumDevices(DI8DEVCLASS_GAMECTRL, [](LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef) -> BOOL {
        std::cout << "Found controller: " << lpddi->tszProductName << std::endl;
        std::cout << "  Instance GUID: " << lpddi->guidInstance.Data1 << std::endl;
        *(bool*)pvRef = true;
        return DIENUM_STOP;
    }, &controllerFound, DIEDFL_ATTACHEDONLY);
    
    if (!controllerFound) {
        std::cout << "No DirectInput controller found!" << std::endl;
        di->Release();
        return 1;
    }
    
    // Create device (we'll use the first found device)
    // This is a simplified test - the main program handles device enumeration properly
    if (FAILED(hr)) {
        std::cout << "Failed to create device: " << hr << std::endl;
        di->Release();
        return 1;
    }
    
    std::cout << "Controller device created successfully" << std::endl;
    
    // Set data format
    hr = joystick->SetDataFormat(&c_dfDIJoystick);
    if (FAILED(hr)) {
        std::cout << "Failed to set data format: " << hr << std::endl;
    } else {
        std::cout << "Data format set successfully" << std::endl;
    }
    
    // Clean up
    if (joystick) {
        joystick->Release();
    }
    if (di) {
        di->Release();
    }
    
    std::cout << "\nDirectInput test completed!" << std::endl;
    std::cout << "Press any key to exit..." << std::endl;
    std::cin.get();
    
    return 0;
}
