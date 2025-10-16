#include <windows.h>
#include <dinput.h>
#include <XInput.h>
#include <iostream>
#include <string>
#include <cmath>
#include <thread>
#include <chrono>
#include <vector>
#include <conio.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib")

enum class ControllerType {
    XInput,
    DirectInput
};

struct ControllerInfo {
    ControllerType type;
    std::string name;
    DWORD index;
    GUID guid;
};

class SimpleController {
private:
    LPDIRECTINPUT8 di;
    LPDIRECTINPUTDEVICE8 joystick;
    HWND hwnd;
    HWND editControl;
    
    // XInput support
    bool hasXInputController;
    DWORD xInputControllerIndex;
    XINPUT_STATE xInputState;
    
    std::string lCurrentKey;
    std::string rCurrentKey;
    bool lHaveKey;
    bool rHaveKey;
    
    static constexpr double PI = 3.14159265359;

public:
    SimpleController() : di(nullptr), joystick(nullptr), hwnd(nullptr), editControl(nullptr),
                        hasXInputController(false), xInputControllerIndex(0),
                        lCurrentKey("null"), rCurrentKey("null"), lHaveKey(false), rHaveKey(false) {
        // Don't initialize controllers or create GUI in constructor
        // This will be done in the main loop
    }
    
    bool initialize() {
        initializeControllers();
        createGUI();
        return (hwnd != nullptr);
    }

    ~SimpleController() {
        if (joystick) {
            joystick->Unacquire();
            joystick->Release();
        }
        if (di) {
            di->Release();
        }
        if (hwnd) {
            DestroyWindow(hwnd);
        }
    }

private:
    void createGUI() {
        // Register window class
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = "SimpleController";
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

        RegisterClassExA(&wc);

        // Create main window (always on top, no focus stealing)
        hwnd = CreateWindowExA(
            WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            "SimpleController",
            "Controller to Maimai",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            500, 600,
            nullptr, nullptr, GetModuleHandle(nullptr), this
        );

        if (!hwnd) {
            std::cerr << "Failed to create window!" << std::endl;
            return;
        }

        // Create edit control for debug info
        editControl = CreateWindowExA(
            WS_EX_CLIENTEDGE,
            "EDIT",
            "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
            10, 10, 470, 550,
            hwnd, nullptr, GetModuleHandle(nullptr), nullptr
        );

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        SimpleController* pThis = nullptr;

        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pThis = (SimpleController*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        } else {
            pThis = (SimpleController*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }

        if (pThis) {
            switch (uMsg) {
                case WM_DESTROY:
                    PostQuitMessage(0);
                    return 0;
                case WM_SIZE:
                    if (pThis->editControl) {
                        RECT clientRect;
                        GetClientRect(hwnd, &clientRect);
                        // Resize edit control to fill the client area with some padding
                        SetWindowPos(pThis->editControl, nullptr, 
                                    10, 10, 
                                    clientRect.right - 20, 
                                    clientRect.bottom - 20, 
                                    SWP_NOZORDER);
                    }
                    return 0;
            }
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    void initializeControllers() {
        // List all available controllers and let user choose
        std::vector<ControllerInfo> availableControllers = listAllControllers();
        
        if (availableControllers.empty()) {
            std::cerr << "No compatible controllers found!" << std::endl;
            std::cerr << "Please connect a controller and restart the application." << std::endl;
            std::cerr << "Press any key to exit..." << std::endl;
            _getch();
            exit(1);
        }
        
        // Display controller selection menu
        displayControllerMenu(availableControllers);
        
        // Get user selection
        int selectedIndex = getControllerSelection(availableControllers.size());
        
        if (selectedIndex >= 0 && selectedIndex < availableControllers.size()) {
            ControllerInfo selected = availableControllers[selectedIndex];
            if (selected.type == ControllerType::XInput) {
                hasXInputController = true;
                xInputControllerIndex = selected.index;
                std::cout << "Selected XInput controller: " << selected.name << std::endl;
            } else {
                if (initializeDirectInputWithDevice(selected.guid)) {
                    std::cout << "Selected DirectInput controller: " << selected.name << std::endl;
                } else {
                    std::cerr << "Failed to initialize selected controller!" << std::endl;
                    std::cerr << "Press any key to exit..." << std::endl;
                    _getch();
                    exit(1);
                }
            }
        } else {
            std::cerr << "Invalid selection!" << std::endl;
            std::cerr << "Press any key to exit..." << std::endl;
            _getch();
            exit(1);
        }
        
        std::cout << "Controller initialized successfully! Opening GUI..." << std::endl;
    }

    std::vector<ControllerInfo> listAllControllers() {
        std::vector<ControllerInfo> controllers;
        
        // List XInput controllers
        for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
            XINPUT_STATE state;
            DWORD result = XInputGetState(i, &state);
            if (result == ERROR_SUCCESS) {
                ControllerInfo info;
                info.type = ControllerType::XInput;
                info.name = "Xbox Controller " + std::to_string(i + 1);
                info.index = i;
                controllers.push_back(info);
            }
        }
        
        // List DirectInput controllers
        if (!di) {
            HRESULT hr = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&di, nullptr);
            if (FAILED(hr)) {
                return controllers;
            }
        }
        
        di->EnumDevices(DI8DEVCLASS_GAMECTRL, [](LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef) -> BOOL {
            std::vector<ControllerInfo>* controllers = (std::vector<ControllerInfo>*)pvRef;
            ControllerInfo info;
            info.type = ControllerType::DirectInput;
            info.name = lpddi->tszProductName;
            info.guid = lpddi->guidInstance;
            controllers->push_back(info);
            return DIENUM_CONTINUE;
        }, &controllers, DIEDFL_ATTACHEDONLY);
        
        return controllers;
    }

    void displayControllerMenu(const std::vector<ControllerInfo>& controllers) {
        std::cout << "\r\n=== CONTROLLER SELECTION ===" << std::endl;
        std::cout << "Available controllers:" << std::endl;
        
        for (size_t i = 0; i < controllers.size(); i++) {
            std::string typeStr = (controllers[i].type == ControllerType::XInput) ? "XInput" : "DirectInput";
            std::cout << "[" << (i + 1) << "] " << controllers[i].name << " (" << typeStr << ")" << std::endl;
        }
        
        std::cout << "\r\nPress the number key (1-" << controllers.size() << ") to select a controller:" << std::endl;
    }

    int getControllerSelection(int maxControllers) {
        while (true) {
            if (_kbhit()) {
                int key = _getch();
                if (key >= '1' && key <= '9') {
                    int selection = key - '1';
                    if (selection < maxControllers) {
                        return selection;
                    }
                }
            }
            Sleep(4);
        }
    }

    bool initializeDirectInputWithDevice(const GUID& deviceGuid) {
        if (!di) {
            HRESULT hr = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&di, nullptr);
            if (FAILED(hr)) {
                return false;
            }
        }

        HRESULT hr = di->CreateDevice(deviceGuid, &joystick, nullptr);
        if (FAILED(hr)) {
            return false;
        }

        // Set data format
        hr = joystick->SetDataFormat(&c_dfDIJoystick);
        if (FAILED(hr)) {
            return false;
        }

        // Set cooperative level
        hr = joystick->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
        if (FAILED(hr)) {
            hr = joystick->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
            if (FAILED(hr)) {
                return false;
            }
        }

        // Acquire the device
        hr = joystick->Acquire();
        return SUCCEEDED(hr);
    }

    bool initializeXInput() {
        // Check for XInput controllers (Xbox 360, Xbox One, Xbox Series)
        for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
            XINPUT_STATE state;
            DWORD result = XInputGetState(i, &state);
            if (result == ERROR_SUCCESS) {
                hasXInputController = true;
                xInputControllerIndex = i;
                std::cout << "Found XInput controller at index " << i << std::endl;
                return true;
            }
        }
        return false;
    }

    bool initializeDirectInput() {
        HRESULT hr = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&di, nullptr);
        if (FAILED(hr)) {
            std::cerr << "Failed to initialize DirectInput: " << hr << std::endl;
            return false;
        }

        // Find and create joystick device
        hr = di->EnumDevices(DI8DEVCLASS_GAMECTRL, [](LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef) -> BOOL {
            SimpleController* pThis = (SimpleController*)pvRef;
            HRESULT hr = pThis->di->CreateDevice(lpddi->guidInstance, &pThis->joystick, nullptr);
            if (SUCCEEDED(hr)) {
                std::cout << "Found controller: " << lpddi->tszProductName << std::endl;
                return DIENUM_STOP;
            }
            return DIENUM_CONTINUE;
        }, this, DIEDFL_ATTACHEDONLY);

        if (!joystick) {
            std::cerr << "No DirectInput controller found!" << std::endl;
            return false;
        }

        // Set data format
        hr = joystick->SetDataFormat(&c_dfDIJoystick);
        if (FAILED(hr)) {
            std::cerr << "Failed to set data format: " << hr << std::endl;
            return false;
        }

        // Set cooperative level (background mode)
        hr = joystick->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
        if (FAILED(hr)) {
            std::cerr << "Failed to set cooperative level (background): " << hr << std::endl;
            // Try foreground as fallback
            hr = joystick->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
            if (FAILED(hr)) {
                std::cerr << "Failed to set cooperative level (foreground): " << hr << std::endl;
                return false;
            }
        }

        // Acquire the device
        hr = joystick->Acquire();
        if (FAILED(hr)) {
            std::cerr << "Failed to acquire device: " << hr << std::endl;
            std::cerr << "This might be because another application is using the controller." << std::endl;
            std::cerr << "Try closing other controller applications and restart this program." << std::endl;
            return false;
        } else {
            std::cout << "Controller acquired successfully!" << std::endl;
            return true;
        }
    }

    void simulateKeyPress(int keyCode, bool isDown) {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = keyCode;
        input.ki.dwFlags = isDown ? 0 : KEYEVENTF_KEYUP;
        input.ki.dwExtraInfo = GetMessageExtraInfo();
        
        // Add scan code for better game compatibility
        input.ki.wScan = MapVirtualKey(keyCode, MAPVK_VK_TO_VSC);
        
        UINT result = SendInput(1, &input, sizeof(INPUT));
        
        // Debug output
        if (result == 0) {
            std::cout << "SendInput failed for key " << keyCode << " (down=" << isDown << ")" << std::endl;
        } else {
            std::cout << "Key " << keyCode << " " << (isDown ? "DOWN" : "UP") << " sent successfully" << std::endl;
        }
        
        // Small delay to ensure key is processed
        if (isDown) {
            Sleep(1);
        }
    }

    void simulateKeyPress(const std::string& key, bool isDown) {
        if (key == "null") return;
        
        int keyCode = 0;
        if (key == "1") keyCode = '1';
        else if (key == "2") keyCode = '2';
        else if (key == "3") keyCode = '3';
        else if (key == "4") keyCode = '4';
        else if (key == "5") keyCode = '5';
        else if (key == "6") keyCode = '6';
        else if (key == "7") keyCode = '7';
        else if (key == "8") keyCode = '8';
        else return;

        std::cout << "Simulating key: " << key << " (down=" << isDown << ")" << std::endl;
        
        // Try SendInput first
        simulateKeyPress(keyCode, isDown);
        
        // Also try keybd_event as fallback for better game compatibility
        if (isDown) {
            keybd_event(keyCode, 0, 0, 0);
        } else {
            keybd_event(keyCode, 0, KEYEVENTF_KEYUP, 0);
        }
    }

    void leftRadialMenuHandler(int direction) {
        if (lHaveKey) {
            simulateKeyPress(lCurrentKey, true);
        } else {
            lHaveKey = true;
            lCurrentKey = std::to_string(direction + 1);
            simulateKeyPress(lCurrentKey, true);
        }
    }

    void rightRadialMenuHandler(int direction) {
        if (rHaveKey) {
            simulateKeyPress(rCurrentKey, true);
        } else {
            rHaveKey = true;
            rCurrentKey = std::to_string(direction + 1);
            simulateKeyPress(rCurrentKey, true);
        }
    }

    // Angle calculation with 0° = top, clockwise
    double calculateAngle(double x, double y) {
        if (x == 0 && y == 0) {
            return -1;
        }
        
        // Calculate angle with atan2 (0° = right, 90° = up, 180° = left, 270° = down)
        double angle = std::atan2(y, x) * 180.0 / PI;
        
        // Convert to 0-360 range
        if (angle < 0) {
            angle += 360;
        }
        
        // Rotate so 0° = top (90° becomes 0°)
        angle = angle - 90;
        if (angle < 0) {
            angle += 360;
        }
        
        // Make clockwise
        angle = 360 - angle;
        
        return angle;
    }

    int getDirection(double angle) {
        if (angle == -1) return -1;
        
        // Map 0-360 to 0-7 directions exactly like AHK script
        // Use Floor(angle / 45) without any offset
        int direction = static_cast<int>(std::floor(angle / 45.0));
        if (direction >= 8) {
            direction = 0;  // Reset to 0 if out of bounds (same as AHK script)
        }
        return direction;
    }

    void updateDebugInfo(double lAngle, double rAngle, int lDirection, int rDirection, const DIJOYSTATE* diState = nullptr) {
        static DWORD lastUpdate = 0;
        DWORD currentTime = GetTickCount();
        
        // Only update every 200ms
        if (currentTime - lastUpdate < 200) {
            return;
        }
        lastUpdate = currentTime;
        
        std::string info = "=== SIMPLE CONTROLLER ===\r\n";
        info += "Controller Type: " + std::string(hasXInputController ? "XInput" : "DirectInput") + "\r\n\r\n";
        
        if (hasXInputController) {
            info += "XINPUT VALUES:\r\n";
            info += "Left Stick X: " + std::to_string(xInputState.Gamepad.sThumbLX) + "\r\n";
            info += "Left Stick Y: " + std::to_string(xInputState.Gamepad.sThumbLY) + "\r\n";
            info += "Right Stick X: " + std::to_string(xInputState.Gamepad.sThumbRX) + "\r\n";
            info += "Right Stick Y: " + std::to_string(xInputState.Gamepad.sThumbRY) + "\r\n\r\n";
            
            info += "NORMALIZED:\r\n";
            info += "Left X: " + std::to_string(xInputState.Gamepad.sThumbLX / 32767.0) + "\r\n";
            info += "Left Y: " + std::to_string(xInputState.Gamepad.sThumbLY / 32767.0) + "\r\n";
            info += "Right X: " + std::to_string(xInputState.Gamepad.sThumbRX / 32767.0) + "\r\n";
            info += "Right Y: " + std::to_string(xInputState.Gamepad.sThumbRY / 32767.0) + "\r\n\r\n";
        } else if (diState) {
            info += "DIRECTINPUT VALUES:\r\n";
            info += "X: " + std::to_string(diState->lX) + "\r\n";
            info += "Y: " + std::to_string(diState->lY) + "\r\n";
            info += "Z: " + std::to_string(diState->lZ) + "\r\n";
            info += "R: " + std::to_string(diState->lRz) + "\r\n\r\n";
            
            info += "NORMALIZED:\r\n";
            info += "X: " + std::to_string((diState->lX / 32767.5) - 1.0) + "\r\n";
            info += "Y: " + std::to_string(1.0 - (diState->lY / 32767.5)) + "\r\n";
            info += "Z: " + std::to_string((diState->lZ / 32767.5) - 1.0) + "\r\n";
            info += "R: " + std::to_string(1.0 - (diState->lRz / 32767.5)) + "\r\n\r\n";
        } else {
            info += "DIRECTINPUT VALUES:\r\n";
            info += "X: " + std::to_string(0) + "\r\n";
            info += "Y: " + std::to_string(0) + "\r\n";
            info += "Z: " + std::to_string(0) + "\r\n";
            info += "R: " + std::to_string(0) + "\r\n\r\n";
            
            info += "NORMALIZED:\r\n";
            info += "X: " + std::to_string(0) + "\r\n";
            info += "Y: " + std::to_string(0) + "\r\n";
            info += "Z: " + std::to_string(0) + "\r\n";
            info += "R: " + std::to_string(0) + "\r\n\r\n";
        }
        
        info += "ANGLES:\r\n";
        info += "Left: " + std::to_string(lAngle) + "\r\n";
        info += "Right: " + std::to_string(rAngle) + "\r\n\r\n";
        
        info += "DIRECTIONS:\r\n";
        info += "Left: " + std::to_string(lDirection) + "\r\n";
        info += "Right: " + std::to_string(rDirection) + "\r\n\r\n";
        
        info += "BUTTONS:\r\n";
        if (hasXInputController) {
            info += "Left Shoulder: " + std::to_string((xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? 1 : 0) + "\r\n";
            info += "Right Shoulder: " + std::to_string((xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 1 : 0) + "\r\n";
            info += "All Buttons: ";
            if (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_A) info += "A ";
            if (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_B) info += "B ";
            if (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_X) info += "X ";
            if (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_Y) info += "Y ";
            if (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) info += "LB ";
            if (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) info += "RB ";
            info += "\r\n\r\n";
        } else if (diState) {
            info += "Button 4: " + std::to_string((diState->rgbButtons[4] & 0x80) ? 1 : 0) + "\r\n";
            info += "Button 5: " + std::to_string((diState->rgbButtons[5] & 0x80) ? 1 : 0) + "\r\n";
            info += "All Buttons: ";
            for (int i = 0; i < 8; i++) {
                if (diState->rgbButtons[i] & 0x80) {
                    info += std::to_string(i) + " ";
                }
            }
            info += "\r\n\r\n";
        } else {
            info += "Button 4: " + std::to_string(0) + "\r\n";
            info += "Button 5: " + std::to_string(0) + "\r\n";
            info += "All Buttons: ";
            info += "\r\n\r\n";
        }
        
        info += "ACTIVE KEYS:\r\n";
        info += "Left: " + lCurrentKey + " (" + (lHaveKey ? "ON" : "OFF") + ")\r\n";
        info += "Right: " + rCurrentKey + " (" + (rHaveKey ? "ON" : "OFF") + ")\r\n\r\n";
        
        info += "DIRECTION MAPPING:\r\n";
        info += "0 = Up-Right,1 = Right-Up, 2 = Right-Down, 3 = Down-Right\r\n";
        info += "4 = Down-Left, 5 = Left-Down, 6 = Left-Up, 7 = Up-Left\r\n";

        SetWindowTextA(editControl, info.c_str());
    }

public:
    void run() {
        if (!hwnd || (!joystick && !hasXInputController)) {
            std::cerr << "Not initialized!" << std::endl;
            return;
        }
        
        // Keep console visible throughout the process

        MSG msg = {};
        while (true) {
            // Process messages (non-blocking)
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (msg.message == WM_QUIT) {
                    std::cout << "WM_QUIT received - window closing..." << std::endl;
                    std::cout << "Breaking from run() method..." << std::endl;
                    return; // Return immediately from run() method
                }
            }

            // Process controller (always, regardless of focus)
            bool controllerSuccess = false;
            bool leftButtonPressed = false;
            bool rightButtonPressed = false;
            double joyX = 0, joyY = 0, joyZ = 0, joyR = 0;
            
            if (hasXInputController) {
                // Process XInput controller
                DWORD result = XInputGetState(xInputControllerIndex, &xInputState);
                if (result == ERROR_SUCCESS) {
                    controllerSuccess = true;
                    
                    // XInput button mapping: Left Shoulder = LB, Right Shoulder = RB
                    leftButtonPressed = (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
                    rightButtonPressed = (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
                    
                    // XInput stick values: -32768 to 32767
                    // For XInput, we need to match DirectInput coordinate system
                    joyX = xInputState.Gamepad.sThumbLX / 32767.0;   // Left stick X
                    joyY = xInputState.Gamepad.sThumbLY / 32767.0;   // Left stick Y (not inverted for XInput)
                    joyZ = xInputState.Gamepad.sThumbRX / 32767.0;   // Right stick X
                    joyR = xInputState.Gamepad.sThumbRY / 32767.0;   // Right stick Y (not inverted for XInput)
                }
            } else if (joystick) {
                // Process DirectInput controller
            DIJOYSTATE state;
            HRESULT hr = joystick->GetDeviceState(sizeof(DIJOYSTATE), &state);
            
            // If device lost, try to reacquire
            if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
                joystick->Unacquire();
                joystick->Acquire();
                continue;
            }
            
            if (SUCCEEDED(hr)) {
                    controllerSuccess = true;
                    
                    // DirectInput button mapping
                    leftButtonPressed = (state.rgbButtons[4] & 0x80) != 0;
                    rightButtonPressed = (state.rgbButtons[5] & 0x80) != 0;
                    
                    // DirectInput stick values
                    joyX = (state.lX / 32767.5) - 1.0;
                    joyY = 1.0 - (state.lY / 32767.5);
                    joyZ = (state.lZ / 32767.5) - 1.0;
                    joyR = 1.0 - (state.lRz / 32767.5);
                }
            }
            
            if (controllerSuccess) {
                // Debug: Check all buttons
                std::string buttonDebug = "Buttons: ";
                if (hasXInputController) {
                    // XInput button debug
                    if (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_A) buttonDebug += "A ";
                    if (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_B) buttonDebug += "B ";
                    if (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_X) buttonDebug += "X ";
                    if (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_Y) buttonDebug += "Y ";
                    if (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) buttonDebug += "LB ";
                    if (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) buttonDebug += "RB ";
                } else {
                    // DirectInput button debug - simplified for now
                    buttonDebug += "DirectInput buttons detected";
                }

                // Joystick values are now processed above in the controller detection section

                // Calculate angles
                double lAngle = calculateAngle(joyX, joyY);
                double rAngle = calculateAngle(joyZ, joyR);

                // Get directions
                int lDirection = getDirection(lAngle);
                int rDirection = getDirection(rAngle);

                // Handle button presses
                if (leftButtonPressed && lDirection != -1) {
                    leftRadialMenuHandler(lDirection);
                } else if (lHaveKey) {
                    simulateKeyPress(lCurrentKey, false);
                    lHaveKey = false;
                    lCurrentKey = "null";
                }

                if (rightButtonPressed && rDirection != -1) {
                    rightRadialMenuHandler(rDirection);
                } else if (rHaveKey) {
                    simulateKeyPress(rCurrentKey, false);
                    rHaveKey = false;
                    rCurrentKey = "null";
                }

                // Update debug info
                if (hasXInputController) {
                    updateDebugInfo(lAngle, rAngle, lDirection, rDirection);
                } else if (joystick) {
                    // For DirectInput, we need to get the state again for debug info
                    DIJOYSTATE debugState;
                    HRESULT debugHr = joystick->GetDeviceState(sizeof(DIJOYSTATE), &debugState);
                    if (SUCCEEDED(debugHr)) {
                        updateDebugInfo(lAngle, rAngle, lDirection, rDirection, &debugState);
                    } else {
                        updateDebugInfo(lAngle, rAngle, lDirection, rDirection);
                    }
                } else {
                    updateDebugInfo(lAngle, rAngle, lDirection, rDirection);
                }
            } else {
                // Try to reacquire DirectInput if needed
                if (joystick) {
                    joystick->Unacquire();
                    Sleep(10);
                    joystick->Acquire();
                }
            }

            Sleep(16); // ~60 FPS
        }
        
        std::cout << "run() method ending..." << std::endl;
    }
};

int main() {
    // Allocate console for debug output
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
    
    std::cout << "Simple Controller to Maimai" << std::endl;
    std::cout << "Close the program by closing the console" << std::endl;
    std::cout << "Closing the GUI will restart the program" << std::endl;

    while (true) {
    try {
        SimpleController app;
            if (!app.initialize()) {
                std::cerr << "Failed to initialize application!" << std::endl;
                continue;
            }
            app.run();
            
            // If we get here, the window was closed
            std::cout << std::endl;
            std::cout << "=== WINDOW CLOSED ===" << std::endl;
            std::cout << "Window closed. Returning to controller selection..." << std::endl;

            
            // Reinitialize console completely to fix text handling
            FreeConsole();
            AllocConsole();
            freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
            freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
            
            std::cout << "Simple Controller to Maimai" << std::endl;
            std::cout << "Debug: Check this console for key simulation errors" << std::endl;
            
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
            std::cerr << "Press any key to continue or Ctrl+C to exit..." << std::endl;
            _getch();
        }
        
        // Continue to next iteration to restart controller selection
        // This happens after both try and catch blocks
    }

    FreeConsole();
    return 0;
}
