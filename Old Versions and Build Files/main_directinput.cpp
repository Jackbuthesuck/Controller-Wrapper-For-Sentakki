#include <windows.h>
#include <dinput.h>
#include <iostream>
#include <string>
#include <cmath>
#include <thread>
#include <chrono>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class ControllerToMaimai {
private:
    LPDIRECTINPUT8 di;
    LPDIRECTINPUTDEVICE8 joystick;
    HWND hwnd;
    HWND editControl;
    
    std::string lCurrentKey;
    std::string rCurrentKey;
    bool lHaveKey;
    bool rHaveKey;
    
    // Constants
    static constexpr double PI = 3.14159265359;
    static constexpr int SLEEP_MS = 16; // ~60 FPS
    static constexpr int GUI_WIDTH = 300;
    static constexpr int GUI_HEIGHT = 300;

public:
    ControllerToMaimai() : di(nullptr), joystick(nullptr), hwnd(nullptr), editControl(nullptr),
                          lCurrentKey("null"), rCurrentKey("null"), lHaveKey(false), rHaveKey(false) {
        createGUI();
        initializeDirectInput();
    }

    ~ControllerToMaimai() {
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
    void initializeDirectInput() {
        HRESULT hr = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&di, nullptr);
        if (FAILED(hr)) {
            std::cerr << "Failed to initialize DirectInput: " << hr << std::endl;
            return;
        }

        // Find and create joystick device
        hr = di->EnumDevices(DI8DEVCLASS_GAMECTRL, [](LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef) -> BOOL {
            ControllerToMaimai* pThis = (ControllerToMaimai*)pvRef;
            HRESULT hr = pThis->di->CreateDevice(lpddi->guidInstance, &pThis->joystick, nullptr);
            if (SUCCEEDED(hr)) {
                std::cout << "Found controller: " << lpddi->tszProductName << std::endl;
                return DIENUM_STOP; // Stop after first controller
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

        // Set cooperative level
        hr = joystick->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
        if (FAILED(hr)) {
            std::cerr << "Failed to set cooperative level: " << hr << std::endl;
            return;
        }

        // Acquire the device
        hr = joystick->Acquire();
        if (FAILED(hr)) {
            std::cerr << "Failed to acquire device: " << hr << std::endl;
        }
    }

    void createGUI() {
        // Register window class
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"ControllerToMaimai";
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

        RegisterClassExW(&wc);

        // Create main window
        hwnd = CreateWindowExW(
            WS_EX_TOPMOST,
            L"ControllerToMaimai",
            L"Controller to Maimai In Osu! (DirectInput)",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            GUI_WIDTH + 20, GUI_HEIGHT + 100,
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
            10, 10, GUI_WIDTH, GUI_HEIGHT,
            hwnd, nullptr, GetModuleHandle(nullptr), nullptr
        );

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        ControllerToMaimai* pThis = nullptr;

        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pThis = (ControllerToMaimai*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        } else {
            pThis = (ControllerToMaimai*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
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

    void simulateKeyPress(int keyCode, bool isDown) {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = keyCode;
        input.ki.dwFlags = isDown ? 0 : KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
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

        simulateKeyPress(keyCode, isDown);
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

    double calculateAngle(double x, double y) {
        if (x == 0 && y == 0) {
            return -1; // No input
        }

        // Calculate angle in degrees using the same logic as the AHK script
        double angle = std::atan2(y, x) * 180.0 / PI;

        // Adjust angle based on quadrant (same as AHK script)
        if (x < 0) {
            angle += 180;
        } else if (y < 0) {
            angle += 360;
        }

        // Make clockwise by subtracting from 360 degrees (same as AHK script)
        angle = 360 - angle;

        // Apply 90 degree offset (same as AHK script)
        angle += 90;
        if (angle >= 360) {
            angle -= 360;
        }

        return angle;
    }

    int getDirection(double angle) {
        if (angle == -1) return -1;
        int direction = static_cast<int>(std::floor(angle / 45.0));
        if (direction >= 8) {
            direction = 0;
        }
        return direction;
    }

    void updateDebugInfo(double lAngle, double rAngle, int lDirection, int rDirection, 
                        const DIJOYSTATE& state) {
        static DWORD lastUpdate = 0;
        DWORD currentTime = GetTickCount();
        
        // Only update UI every 100ms to prevent freezing
        if (currentTime - lastUpdate < 100) {
            return;
        }
        lastUpdate = currentTime;
        
        std::string info = "=== CONTROLLER TO MAIMAI ===\n";
        info += "Press F7 to exit\n\n";
        
        info += "ANGLES:\n";
        info += "Left Angle: " + std::to_string(lAngle) + "\n";
        info += "Right Angle: " + std::to_string(rAngle) + "\n\n";
        
        info += "DIRECTIONS (0-7):\n";
        info += "Left Direction: " + std::to_string(lDirection) + "\n";
        info += "Right Direction: " + std::to_string(rDirection) + "\n\n";
        
        info += "ACTIVE KEYS:\n";
        info += "Left Key: " + lCurrentKey + " (Active: " + (lHaveKey ? "true" : "false") + ")\n";
        info += "Right Key: " + rCurrentKey + " (Active: " + (rHaveKey ? "true" : "false") + ")\n\n";
        
        info += "CONTROLLER STATE:\n";
        info += "Left Stick (X,Y): " + std::to_string(state.lX) + ", " + std::to_string(state.lY) + "\n";
        info += "Right Stick (Z,R): " + std::to_string(state.lZ) + ", " + std::to_string(state.lRz) + "\n";
        info += "LB (Button 4): " + std::to_string((state.rgbButtons[4] & 0x80) ? 1 : 0) + "\n";
        info += "RB (Button 5): " + std::to_string((state.rgbButtons[5] & 0x80) ? 1 : 0) + "\n\n";
        
        info += "DIRECTION MAPPING:\n";
        info += "0=Top, 1=Top-Right, 2=Right, 3=Bottom-Right\n";
        info += "4=Bottom, 5=Bottom-Left, 6=Left, 7=Top-Left\n";

        std::wstring winfo(info.begin(), info.end());
        SetWindowTextW(editControl, winfo.c_str());
    }

public:
    void run() {
        if (!hwnd) {
            std::cerr << "GUI not initialized!" << std::endl;
            return;
        }

        if (!joystick) {
            std::cerr << "Controller not initialized!" << std::endl;
            return;
        }

        MSG msg = {};
        while (true) {
            // Use PeekMessage to avoid blocking
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);

                if (msg.message == WM_QUIT) {
                    break;
                }
            }

            // Process controller input
            processControllerInput();

            // Small delay to prevent excessive CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
        }
    }

private:
    void processControllerInput() {
        if (!joystick) return;

        DIJOYSTATE state;
        HRESULT hr = joystick->GetDeviceState(sizeof(DIJOYSTATE), &state);
        
        if (FAILED(hr)) {
            // Try to reacquire the device
            if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
                joystick->Unacquire();
                Sleep(10); // Small delay before reacquiring
                joystick->Acquire();
            }
            return;
        }

        // Check buttons - LB is button 4, RB is button 5
        // Use safer button detection to prevent hanging
        bool leftButtonPressed = false;
        bool rightButtonPressed = false;
        
        if (state.rgbButtons[4] & 0x80) {
            leftButtonPressed = true;
        }
        if (state.rgbButtons[5] & 0x80) {
            rightButtonPressed = true;
        }

        // Get joystick values (normalized to -1 to 1)
        // DirectInput values are typically -32768 to 32767
        // Left stick: X and Y axes
        // Right stick: Z and R axes
        double joyX = state.lX / 32767.0;
        double joyY = state.lY / 32767.0;
        double joyZ = state.lZ / 32767.0;
        double joyR = state.lRz / 32767.0;

        // Calculate angles
        double lAngle = calculateAngle(joyX, joyY);
        double rAngle = calculateAngle(joyZ, joyR);

        // Get directions
        int lDirection = getDirection(lAngle);
        int rDirection = getDirection(rAngle);

        // Handle button presses
        if (leftButtonPressed) {
            if (lDirection != -1) {
                leftRadialMenuHandler(lDirection);
            }
        } else {
            if (lHaveKey) {
                simulateKeyPress(lCurrentKey, false);
                lHaveKey = false;
                lCurrentKey = "null";
            }
        }

        if (rightButtonPressed) {
            if (rDirection != -1) {
                rightRadialMenuHandler(rDirection);
            }
        } else {
            if (rHaveKey) {
                simulateKeyPress(rCurrentKey, false);
                rHaveKey = false;
                rCurrentKey = "null";
            }
        }

        // Update debug info
        updateDebugInfo(lAngle, rAngle, lDirection, rDirection, state);
    }
};

int main() {
    std::cout << "Controller to Maimai for Sentakki - DirectInput Version" << std::endl;
    std::cout << "Press F7 to exit" << std::endl;

    try {
        ControllerToMaimai app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
