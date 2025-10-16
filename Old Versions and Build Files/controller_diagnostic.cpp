#include <windows.h>
#include <dinput.h>
#include <iostream>
#include <string>
#include <cmath>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class ControllerDiagnostic {
private:
    LPDIRECTINPUT8 di;
    LPDIRECTINPUTDEVICE8 joystick;
    HWND hwnd;
    HWND editControl;
    
    static constexpr double PI = 3.14159265359;

public:
    ControllerDiagnostic() : di(nullptr), joystick(nullptr), hwnd(nullptr), editControl(nullptr) {
        createGUI();
        initializeDirectInput();
    }

    ~ControllerDiagnostic() {
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
        wc.lpszClassName = L"ControllerDiagnostic";
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

        RegisterClassExW(&wc);

        // Create main window
        hwnd = CreateWindowExW(
            WS_EX_TOPMOST,
            L"ControllerDiagnostic",
            L"Controller Diagnostic - Press F7 to exit",
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
        editControl = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
            10, 10, 480, 550,
            hwnd, nullptr, GetModuleHandle(nullptr), nullptr
        );

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        ControllerDiagnostic* pThis = nullptr;

        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pThis = (ControllerDiagnostic*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        } else {
            pThis = (ControllerDiagnostic*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
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
            ControllerDiagnostic* pThis = (ControllerDiagnostic*)pvRef;
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

    double calculateAngle(double x, double y) {
        if (x == 0 && y == 0) {
            return -1; // No input
        }

        // Calculate angle in degrees
        double angle = std::atan2(y, x) * 180.0 / PI;

        // Adjust angle based on quadrant and make clockwise
        if (angle < 0) {
            angle += 360;
        }
        angle = 360 - angle; // Make clockwise

        // Apply 90 degree offset
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

    void updateDebugInfo(const DIJOYSTATE& state) {
        // Get joystick values (normalized to -1 to 1)
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

        std::string info = "=== CONTROLLER DIAGNOSTIC ===\n";
        info += "Press F7 to exit\n\n";
        
        info += "RAW VALUES:\n";
        info += "X: " + std::to_string(state.lX) + " (norm: " + std::to_string(joyX) + ")\n";
        info += "Y: " + std::to_string(state.lY) + " (norm: " + std::to_string(joyY) + ")\n";
        info += "Z: " + std::to_string(state.lZ) + " (norm: " + std::to_string(joyZ) + ")\n";
        info += "R: " + std::to_string(state.lRz) + " (norm: " + std::to_string(joyR) + ")\n\n";
        
        info += "ANGLES:\n";
        info += "Left Angle: " + std::to_string(lAngle) + "\n";
        info += "Right Angle: " + std::to_string(rAngle) + "\n\n";
        
        info += "DIRECTIONS (0-7):\n";
        info += "Left Direction: " + std::to_string(lDirection) + "\n";
        info += "Right Direction: " + std::to_string(rDirection) + "\n\n";
        
        info += "BUTTONS (0=not pressed, 1=pressed):\n";
        for (int i = 0; i < 32; i++) {
            if (state.rgbButtons[i] & 0x80) {
                info += "Button " + std::to_string(i) + ": PRESSED\n";
            }
        }
        
        info += "\nPOV HAT:\n";
        info += "POV: " + std::to_string(state.rgdwPOV[0]) + "\n";
        
        info += "\n=== TEST INSTRUCTIONS ===\n";
        info += "1. Move left stick in all 8 directions\n";
        info += "2. Move right stick in all 8 directions\n";
        info += "3. Press all buttons to see which numbers they are\n";
        info += "4. Note which buttons you want to use for LB/RB\n";

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
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT) {
                break;
            }

            // Process controller input
            DIJOYSTATE state;
            HRESULT hr = joystick->GetDeviceState(sizeof(DIJOYSTATE), &state);
            
            if (SUCCEEDED(hr)) {
                updateDebugInfo(state);
            } else {
                // Try to reacquire the device
                if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
                    joystick->Acquire();
                }
            }

            // Small delay
            Sleep(50);
        }
    }
};

int main() {
    std::cout << "Controller Diagnostic Tool" << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << "This will show you exactly what your controller sends" << std::endl;
    std::cout << "Press F7 to exit" << std::endl;

    try {
        ControllerDiagnostic app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
