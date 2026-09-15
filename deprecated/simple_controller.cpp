#include <windows.h>
#include <dinput.h>
#include <iostream>
#include <string>
#include <cmath>
#include <thread>
#include <chrono>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class SimpleController {
private:
    LPDIRECTINPUT8 di;
    LPDIRECTINPUTDEVICE8 joystick;
    HWND hwnd;
    HWND editControl;
    
    std::string lCurrentKey;
    std::string rCurrentKey;
    bool lHaveKey;
    bool rHaveKey;
    
    static constexpr double PI = 3.14159265359;

public:
    SimpleController() : di(nullptr), joystick(nullptr), hwnd(nullptr), editControl(nullptr),
                        lCurrentKey("null"), rCurrentKey("null"), lHaveKey(false), rHaveKey(false) {
        createGUI();
        initializeDirectInput();
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
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"SimpleController";
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

        RegisterClassExW(&wc);

        // Create main window (always on top, no focus stealing)
        hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            L"SimpleController",
            L"Simple Controller to Maimai - Press F7 to exit",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            700, 600,
            nullptr, nullptr, GetModuleHandle(nullptr), this
        );

        if (!hwnd) {
            std::cerr << "Failed to create window!" << std::endl;
            return;
        }

        // Create edit control for debug info
        editControl = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
            10, 10, 670, 550,
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
                case WM_KEYDOWN:
                    if (wParam == VK_F7) {
                        PostQuitMessage(0);
                        return 0;
                    }
                    break;
            }
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    void initializeDirectInput() {
        HRESULT hr = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&di, nullptr);
        if (FAILED(hr)) {
            std::cerr << "Failed to initialize DirectInput: " << hr << std::endl;
            return;
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
            return;
        }

        // Set data format
        hr = joystick->SetDataFormat(&c_dfDIJoystick);
        if (FAILED(hr)) {
            std::cerr << "Failed to set data format: " << hr << std::endl;
            return;
        }

        // Set cooperative level (background mode)
        hr = joystick->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
        if (FAILED(hr)) {
            std::cerr << "Failed to set cooperative level (background): " << hr << std::endl;
            // Try foreground as fallback
            hr = joystick->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
            if (FAILED(hr)) {
                std::cerr << "Failed to set cooperative level (foreground): " << hr << std::endl;
                return;
            }
        }

        // Acquire the device
        hr = joystick->Acquire();
        if (FAILED(hr)) {
            std::cerr << "Failed to acquire device: " << hr << std::endl;
            std::cerr << "This might be because another application is using the controller." << std::endl;
            std::cerr << "Try closing other controller applications and restart this program." << std::endl;
        } else {
            std::cout << "Controller acquired successfully!" << std::endl;
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

    void updateDebugInfo(double lAngle, double rAngle, int lDirection, int rDirection, 
                        const DIJOYSTATE& state) {
        static DWORD lastUpdate = 0;
        DWORD currentTime = GetTickCount();
        
        // Only update every 200ms
        if (currentTime - lastUpdate < 200) {
            return;
        }
        lastUpdate = currentTime;
        
        std::string info = "=== SIMPLE CONTROLLER ===\r\n";
        info += "Press F7 to exit (Or just close the window) \r\n\r\n";
        
        info += "RAW VALUES:\r\n";
        info += "X: " + std::to_string(state.lX) + "\r\n";
        info += "Y: " + std::to_string(state.lY) + "\r\n";
        info += "Z: " + std::to_string(state.lZ) + "\r\n";
        info += "R: " + std::to_string(state.lRz) + "\r\n\r\n";
        
        info += "NORMALIZED:\r\n";
        info += "X: " + std::to_string((state.lX / 32767.5) - 1.0) + "\r\n";
        info += "Y: " + std::to_string((state.lY / 32767.5) - 1.0) + "\r\n";
        info += "Z: " + std::to_string((state.lZ / 32767.5) - 1.0) + "\r\n";
        info += "R: " + std::to_string(1.0 - (state.lRz / 32767.5)) + "\r\n\r\n";
        
        info += "ANGLES:\r\n";
        info += "Left: " + std::to_string(lAngle) + "\r\n";
        info += "Right: " + std::to_string(rAngle) + "\r\n\r\n";
        
        info += "DIRECTIONS:\r\n";
        info += "Left: " + std::to_string(lDirection) + "\r\n";
        info += "Right: " + std::to_string(rDirection) + "\r\n\r\n";
        
        info += "BUTTONS:\r\n";
        info += "Button 4: " + std::to_string((state.rgbButtons[4] & 0x80) ? 1 : 0) + "\r\n";
        info += "Button 5: " + std::to_string((state.rgbButtons[5] & 0x80) ? 1 : 0) + "\r\n";
        info += "All Buttons: ";
        for (int i = 0; i < 8; i++) {
            if (state.rgbButtons[i] & 0x80) {
                info += std::to_string(i) + " ";
            }
        }
        info += "\r\n\r\n";
        
        info += "ACTIVE KEYS:\r\n";
        info += "Left: " + lCurrentKey + " (" + (lHaveKey ? "ON" : "OFF") + ")\r\n";
        info += "Right: " + rCurrentKey + " (" + (rHaveKey ? "ON" : "OFF") + ")\r\n\r\n";
        
        info += "DIRECTION MAPPING:\r\n";
        info += "0=Top, 1=Top-Right, 2=Right, 3=Bottom-Right\r\n";
        info += "4=Bottom, 5=Bottom-Left, 6=Left, 7=Top-Left\r\n";

        std::wstring winfo(info.begin(), info.end());
        SetWindowTextW(editControl, winfo.c_str());
    }

public:
    void run() {
        if (!hwnd || !joystick) {
            std::cerr << "Not initialized!" << std::endl;
            return;
        }

        MSG msg = {};
        while (true) {
            // Process messages (non-blocking)
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (msg.message == WM_QUIT) {
                    break;
                }
            }

            // Process controller (always, regardless of focus)
            DIJOYSTATE state;
            HRESULT hr = joystick->GetDeviceState(sizeof(DIJOYSTATE), &state);
            
            // If device lost, try to reacquire
            if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
                joystick->Unacquire();
                joystick->Acquire();
                continue;
            }
            
            if (SUCCEEDED(hr)) {
                // Check buttons
                bool leftButtonPressed = (state.rgbButtons[4] & 0x80) != 0;
                bool rightButtonPressed = (state.rgbButtons[5] & 0x80) != 0;
                
                // Debug: Check all buttons
                std::string buttonDebug = "Buttons: ";
                for (int i = 0; i < 8; i++) {
                    if (state.rgbButtons[i] & 0x80) {
                        buttonDebug += std::to_string(i) + " ";
                    }
                }

                // Get joystick values with correct mapping
                // X, Z, Y, R: 0 to 65535 (full 16-bit)
                double joyX = (state.lX / 32767.5) - 1.0;  // Convert 0-32767 to -1 to 1
                double joyY = 1.0 - (state.lY / 32767.5);   // Convert 0-65535 to 1 to -1 (invert Y like right stick)
                double joyZ = (state.lZ / 32767.5) - 1.0;   // Convert 0-65535 to -1 to 1
                double joyR = 1.0 - (state.lRz / 32767.5); // Convert 0-65535 to 1 to -1 (invert R)

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
                updateDebugInfo(lAngle, rAngle, lDirection, rDirection, state);
            } else {
                // Try to reacquire
                if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
                    joystick->Unacquire();
                    Sleep(10);
                    joystick->Acquire();
                }
            }

            Sleep(16); // ~60 FPS
        }
    }
};

int main() {
    // Allocate console for debug output
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
    
    std::cout << "Simple Controller to Maimai" << std::endl;
    std::cout << "Press F7 to exit" << std::endl;
    std::cout << "Debug: Check this console for key simulation errors" << std::endl;

    try {
        SimpleController app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
