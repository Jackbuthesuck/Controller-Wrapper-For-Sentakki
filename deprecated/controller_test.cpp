#include <windows.h>
#include <XInput.h>
#include <iostream>
#include <string>

#pragma comment(lib, "XInput.lib")

int main() {
    std::cout << "Controller Detection Test" << std::endl;
    std::cout << "=========================" << std::endl;
    
    // Test all 4 XInput controller slots
    for (int i = 0; i < 4; i++) {
        XINPUT_STATE state;
        DWORD result = XInputGetState(i, &state);
        
        std::cout << "Controller " << (i + 1) << ": ";
        
        if (result == ERROR_SUCCESS) {
            std::cout << "CONNECTED" << std::endl;
            std::cout << "  Buttons: " << state.Gamepad.wButtons << std::endl;
            std::cout << "  Left Stick: (" << state.Gamepad.sThumbLX << ", " << state.Gamepad.sThumbLY << ")" << std::endl;
            std::cout << "  Right Stick: (" << state.Gamepad.sThumbRX << ", " << state.Gamepad.sThumbRY << ")" << std::endl;
            std::cout << "  Triggers: L=" << (int)state.Gamepad.bLeftTrigger << " R=" << (int)state.Gamepad.bRightTrigger << std::endl;
        } else if (result == ERROR_DEVICE_NOT_CONNECTED) {
            std::cout << "NOT CONNECTED" << std::endl;
        } else {
            std::cout << "ERROR: " << result << std::endl;
        }
        std::cout << std::endl;
    }
    
    // Test if XInput is available
    std::cout << "XInput Version: 1.4" << std::endl;
    
    // Check if we can get capabilities
    for (int i = 0; i < 4; i++) {
        XINPUT_CAPABILITIES caps;
        DWORD result = XInputGetCapabilities(i, XINPUT_FLAG_GAMEPAD, &caps);
        
        std::cout << "Controller " << (i + 1) << " capabilities: ";
        if (result == ERROR_SUCCESS) {
            std::cout << "Available" << std::endl;
            std::cout << "  Type: " << caps.Type << std::endl;
            std::cout << "  SubType: " << caps.SubType << std::endl;
        } else {
            std::cout << "Not available (Error: " << result << ")" << std::endl;
        }
    }
    
    std::cout << std::endl << "Press any key to exit..." << std::endl;
    std::cin.get();
    
    return 0;
}
