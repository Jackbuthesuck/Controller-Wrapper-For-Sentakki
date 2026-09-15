#include <windows.h>
#include <XInput.h>
#include <iostream>
#include <string>
#include <cmath>
#include <thread>
#include <chrono>

#pragma comment(lib, "XInput.lib")

class ControllerToMaimai {
private:
    int controllerNumber;
    std::string lCurrentKey;
    std::string rCurrentKey;
    bool lHaveKey;
    bool rHaveKey;
    HWND hwnd;
    HWND editControl;
    
    // Constants
    static constexpr double PI = 3.14159265359;
    static constexpr int SLEEP_MS = 2;
    static constexpr int GUI_WIDTH = 300;
    static constexpr int GUI_HEIGHT = 300;

public:
    ControllerToMaimai() : controllerNumber(0), lCurrentKey("null"), rCurrentKey("null"), 
                          lHaveKey(false), rHaveKey(false), hwnd(nullptr), editControl(nullptr) {
        // Auto-detect controller
        detectController();
        createGUI();
    }

    ~ControllerToMaimai() {
        if (hwnd) {
            DestroyWindow(hwnd);
        }
    }

private:
    void detectController() {
        // Try to find a connected controller (1-4 for XInput)
        for (int i = 1; i <= 4; i++) {
            XINPUT_STATE state;
            if (XInputGetState(i - 1, &state) == ERROR_SUCCESS) {
                controllerNumber = i;
                std::cout << "Controller detected: #" << controllerNumber << std::endl;
                return;
            }
        }
        std::cout << "No controller detected!" << std::endl;
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
            L"Controller to Maimai In Osu!",
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

    void updateDebugInfo(double lAngle, double rAngle, int lDirection, int rDirection) {
        std::string info = "lAngle: \n" + std::to_string(lAngle) + 
                          "\nrAngle: \n" + std::to_string(rAngle) +
                          "\nlDirection: " + std::to_string(lDirection) +
                          "\nrDirection: " + std::to_string(rDirection) +
                          "\nlCurrentKey: " + lCurrentKey +
                          "\nrCurrentKey: " + rCurrentKey +
                          "\nlHaveKey: " + (lHaveKey ? "true" : "false") +
                          "\nrHaveKey: " + (rHaveKey ? "true" : "false");

        std::wstring winfo(info.begin(), info.end());
        SetWindowTextW(editControl, winfo.c_str());
    }

public:
    void run() {
        if (!hwnd) {
            std::cerr << "GUI not initialized!" << std::endl;
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
            processControllerInput();

            // Small delay to prevent excessive CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
        }
    }

private:
    void processControllerInput() {
        if (controllerNumber <= 0) return;

        XINPUT_STATE state;
        if (XInputGetState(controllerNumber - 1, &state) != ERROR_SUCCESS) {
            return;
        }

        // Check buttons 5 and 6 (LB and RB on Xbox controller)
        bool leftButtonPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
        bool rightButtonPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;

        // Get joystick values (normalized to -1 to 1)
        double joyX = (state.Gamepad.sThumbLX / 32767.0);
        double joyY = (state.Gamepad.sThumbLY / 32767.0);
        double joyZ = (state.Gamepad.sThumbRX / 32767.0);
        double joyR = (state.Gamepad.sThumbRY / 32767.0);

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
        updateDebugInfo(lAngle, rAngle, lDirection, rDirection);
    }
};

int main() {
    std::cout << "Controller to Maimai for Sentakki - C++ Version" << std::endl;
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
