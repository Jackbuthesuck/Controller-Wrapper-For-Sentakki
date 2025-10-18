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
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "msimg32.lib")

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
    HWND overlayHwnd;
    std::string debugText; // Store debug info for overlay display
    bool showDebugInfo; // Toggle debug info visibility
    
    // XInput support
    bool hasXInputController;
    DWORD xInputControllerIndex;
    XINPUT_STATE xInputState;
    
    // Alternating frame counter for dual-stick mode
    bool alternateFrame;
    
    // Overlay stick positions (-1.0 to 1.0)
    double overlayLeftX;
    double overlayLeftY;
    double overlayRightX;
    double overlayRightY;
    
    // Overlay stick angles (in degrees)
    double overlayLeftAngle;
    double overlayRightAngle;
    
    // Alpha values for fading (0-255)
    int overlayLeftAlpha;
    int overlayRightAlpha;
    
    // Mouse control state
    enum class MouseControlState {
        None,
        Left,
        Right
    };
    MouseControlState mouseControlActive;
    bool prevLeftBumper;
    bool prevRightBumper;
    int overlayPosX;  // Store overlay position for mouse mapping
    int overlayPosY;
    
    // Constants
    static constexpr double PI = 3.14159265359;
    static constexpr int WINDOW_WIDTH = 480;
    static constexpr int WINDOW_HEIGHT = 640;
    static constexpr int EDIT_PADDING = 2;
    static constexpr int UPDATE_INTERVAL_MS = 200;
    static constexpr int MAIN_LOOP_SLEEP_MS = 16;  // 60 FPS (1000ms / 60 = ~16ms)
    static constexpr int SELECTION_SLEEP_MS = 4;
    static constexpr double STICK_MAX_VALUE = 32767.0;
    static constexpr double STICK_NORMALIZE_FACTOR = 32767.5;
    static constexpr int DIRECTION_SECTORS = 8;
    static constexpr double DEGREES_PER_SECTOR = 45.0;
    int overlayStickRadius;
    static constexpr int OVERLAY_STICK_INDICATOR_RADIUS = 15;
    int updateIntervalMs; // Dynamic update interval based on screen refresh rate

public:
    SimpleController() : di(nullptr), joystick(nullptr), hwnd(nullptr), overlayHwnd(nullptr),
                        hasXInputController(false), xInputControllerIndex(0),
                        alternateFrame(false),
                        overlayLeftX(0.0), overlayLeftY(0.0), overlayRightX(0.0), overlayRightY(0.0),
                        overlayLeftAngle(-1.0), overlayRightAngle(-1.0), overlayStickRadius(150),
                        overlayLeftAlpha(0), overlayRightAlpha(0), updateIntervalMs(16),
                        mouseControlActive(MouseControlState::None), prevLeftBumper(false), prevRightBumper(false),
                        overlayPosX(0), overlayPosY(0), debugText(""), showDebugInfo(true) {
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
        if (overlayHwnd) {
            DestroyWindow(overlayHwnd);
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
            WINDOW_WIDTH, WINDOW_HEIGHT,
            nullptr, nullptr, GetModuleHandle(nullptr), this
        );

        if (!hwnd) {
            logError("Failed to create window!");
            return;
        }

        // Hide the main window - we only use overlay now
        ShowWindow(hwnd, SW_HIDE);
        
        // Create the overlay window
        createOverlay();
    }

    void createOverlay() {
        // Register overlay window class
        WNDCLASSEXA overlayWc = {};
        overlayWc.cbSize = sizeof(WNDCLASSEXA);
        overlayWc.style = CS_OWNDC; // Own DC for better performance, remove redraw flags
        overlayWc.lpfnWndProc = OverlayWindowProc;
        overlayWc.hInstance = GetModuleHandle(nullptr);
        overlayWc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        overlayWc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH); // Transparent background
        overlayWc.lpszClassName = "StickOverlay";

        RegisterClassExA(&overlayWc);

        // Get screen dimensions
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        
        // Calculate overlay size - full screen width, 90% height
        int overlayHeight = (int)(screenHeight * 0.9);
        int overlayWidth = screenWidth; // Full screen width for debug text visibility
        
        // Calculate stick radius to fill most of the overlay (leave some padding)
        overlayStickRadius = (int)(overlayHeight * 0.45); // 45% of height (90% of half = radius)
        
        // Get screen refresh rate
        HDC screenDC = GetDC(nullptr);
        int refreshRate = GetDeviceCaps(screenDC, VREFRESH);
        ReleaseDC(nullptr, screenDC);
        
        // Calculate update interval based on refresh rate
        if (refreshRate > 1) {
            updateIntervalMs = 1000 / refreshRate;
            std::cout << "Detected screen refresh rate: " << refreshRate << "Hz" << std::endl;
            std::cout << "Setting update interval to: " << updateIntervalMs << "ms" << std::endl;
        } else {
            // Fallback to 60Hz if detection fails
            updateIntervalMs = 16;
            std::cout << "Could not detect refresh rate, defaulting to 60Hz (16ms)" << std::endl;
        }
        
        // Center the overlay on screen (vertically only, full width horizontally)
        int posX = 0; // Start at left edge of screen
        int posY = (screenHeight - overlayHeight) / 2;
        
        // Store overlay position
        overlayPosX = posX;
        overlayPosY = posY;
        
        // Create transparent overlay window (always on top, click-through, no activation)
        overlayHwnd = CreateWindowExA(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
            "StickOverlay",
            "Stick Position Overlay",
            WS_POPUP,
            posX, posY,
            overlayWidth, overlayHeight,
            nullptr, nullptr, GetModuleHandle(nullptr), this
        );

        if (!overlayHwnd) {
            logError("Failed to create overlay window!");
            return;
        }

        // Set transparency - make black (RGB(0,0,0)) completely transparent
        // This way only the drawn graphics are visible, not the background
        SetLayeredWindowAttributes(overlayHwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

        ShowWindow(overlayHwnd, SW_SHOW);
        UpdateWindow(overlayHwnd);
    }

    static LRESULT CALLBACK OverlayWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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
                case WM_PAINT: {
                    PAINTSTRUCT ps;
                    HDC hdc = BeginPaint(hwnd, &ps);
                    pThis->drawOverlay(hdc);
                    EndPaint(hwnd, &ps);
                    return 0;
                }
                case WM_ERASEBKGND:
                    return 1; // We handle background ourselves
            }
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    void drawOverlay(HDC hdc) {
        RECT rect;
        GetClientRect(overlayHwnd, &rect);
        
        // Clear the window - make it completely transparent
        // Fill with black which is our transparency key
        HBRUSH clearBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rect, clearBrush);
        DeleteObject(clearBrush);
        
        // Calculate center position - both sticks in the same location
        int centerX = rect.right / 2;
        int centerY = rect.bottom / 2;
        
        // Draw boundary circle with fade based on max alpha
        int maxAlpha = (overlayLeftAlpha > overlayRightAlpha) ? overlayLeftAlpha : overlayRightAlpha;
        
        if (maxAlpha > 10) {
            // Modulate pen width based on alpha for fade effect
            int boundaryWidth = 1 + (maxAlpha * 3 / 255); // 1-4 pixels
            
            // Draw outer circle (boundary) - only once for both sticks, full gray color
            HPEN boundaryPen = CreatePen(PS_SOLID, boundaryWidth, RGB(200, 200, 200));
            HPEN oldPen = (HPEN)SelectObject(hdc, boundaryPen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            
            Ellipse(hdc, 
                    centerX - overlayStickRadius, 
                    centerY - overlayStickRadius,
                    centerX + overlayStickRadius, 
                    centerY + overlayStickRadius);
            
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(boundaryPen);
        }
        
        // Draw angle indicators first (so they appear behind the stick indicators)
        drawAngleIndicator(hdc, centerX, centerY, overlayLeftAngle, RGB(100, 150, 255), overlayLeftAlpha);
        drawAngleIndicator(hdc, centerX, centerY, overlayRightAngle, RGB(255, 100, 150), overlayRightAlpha);
        
        // Draw both sticks at the same position (they will overlap)
        // Draw left stick (blue)
        drawStick(hdc, centerX, centerY, overlayLeftX, overlayLeftY, RGB(100, 150, 255), overlayLeftAlpha);
        
        // Draw right stick (pink)
        drawStick(hdc, centerX, centerY, overlayRightX, overlayRightY, RGB(255, 100, 150), overlayRightAlpha);
        
        // Draw debug text on the middle left (if enabled)
        if (showDebugInfo && !debugText.empty()) {
            drawDebugText(hdc, rect);
        }
    }

    void drawDebugText(HDC hdc, RECT rect) {
        // Overlay now spans full screen width starting at x=0
        // So textX=20 means 20 pixels from left edge of screen
        int textX = 20;
        int textY = rect.bottom / 2 - 250; // Middle of overlay window (vertically centered on screen)
        
        // Ensure text is visible (not off-screen)
        if (textY < 20) textY = 20;
        
        // Set up text rendering
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        
        // Create font for debug text
        HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, "Consolas");
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
        
        // Split debug text into lines and render
        std::string text = debugText;
        size_t pos = 0;
        int lineHeight = 18;
        int currentY = textY;
        
        while ((pos = text.find("\r\n")) != std::string::npos) {
            std::string line = text.substr(0, pos);
            TextOutA(hdc, textX, currentY, line.c_str(), line.length());
            currentY += lineHeight;
            text.erase(0, pos + 2); // Remove "\r\n"
        }
        // Draw remaining text
        if (!text.empty()) {
            TextOutA(hdc, textX, currentY, text.c_str(), text.length());
        }
        
        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
    }

    void drawAngleIndicator(HDC hdc, int centerX, int centerY, double angle, COLORREF color, int alpha) {
        if (angle < 0 || alpha == 0) return; // No input detected or invisible
        
        // Skip if too faint to avoid artifacts
        if (alpha < 10) return;
        
        // Draw an arc along the main boundary circle
        double arcSpan = 45.0; // Arc spans 45 degrees (22.5 degrees on each side)
        
        // Calculate start and end angles for the arc
        double arcStartAngle = angle - arcSpan / 2.0;
        double arcEndAngle = angle + arcSpan / 2.0;
        
        // Convert to radians (standard math: 0° = right, counter-clockwise)
        double startRad = (90 - arcStartAngle) * PI / 180.0;
        double endRad = (90 - arcEndAngle) * PI / 180.0;
        
        // Calculate arc endpoints on the boundary circle
        int startX = centerX + (int)(cos(startRad) * overlayStickRadius);
        int startY = centerY - (int)(sin(startRad) * overlayStickRadius);
        int endX = centerX + (int)(cos(endRad) * overlayStickRadius);
        int endY = centerY - (int)(sin(endRad) * overlayStickRadius);
        
        // Modulate pen width based on alpha for fade effect (thinner when faint)
        int penWidth = 2 + (alpha * 8 / 255); // 2-10 pixels
        
        // Draw the arc along the main circle boundary - use full bright color for vibrancy
        HPEN arcPen = CreatePen(PS_SOLID, penWidth, color);
        HPEN oldPen = (HPEN)SelectObject(hdc, arcPen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        
        // Use Arc function to draw along the boundary circle
        Arc(hdc,
            centerX - overlayStickRadius, centerY - overlayStickRadius,
            centerX + overlayStickRadius, centerY + overlayStickRadius,
            endX, endY, startX, startY);
        
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(arcPen);
    }

    void drawStick(HDC hdc, int centerX, int centerY, double stickX, double stickY, COLORREF color, int alpha) {
        // Boundary circle is now drawn in drawOverlay() to avoid duplication
        
        if (alpha == 0) return; // Don't draw position indicator if invisible
        
        // Skip if too faint to avoid artifacts
        if (alpha < 10) return;
        
        // Modulate pen width based on alpha for fade effect (thinner when faint)
        int penWidth = 1 + (alpha * 5 / 255); // 1-6 pixels
        
        // Draw stick position indicator - hollow circle with colored edge
        int indicatorX = centerX + (int)(stickX * (overlayStickRadius - OVERLAY_STICK_INDICATOR_RADIUS));
        int indicatorY = centerY - (int)(stickY * (overlayStickRadius - OVERLAY_STICK_INDICATOR_RADIUS)); // Inverted Y
        
        // Create colored pen for the edge with modulated width - use full bright color
        HPEN indicatorPen = CreatePen(PS_SOLID, penWidth, color);
        HPEN oldPen = (HPEN)SelectObject(hdc, indicatorPen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        
        Ellipse(hdc,
                indicatorX - OVERLAY_STICK_INDICATOR_RADIUS,
                indicatorY - OVERLAY_STICK_INDICATOR_RADIUS,
                indicatorX + OVERLAY_STICK_INDICATOR_RADIUS,
                indicatorY + OVERLAY_STICK_INDICATOR_RADIUS);
        
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(indicatorPen);
    }

    void moveMouseToCenter() {
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        SetCursorPos(screenWidth / 2, screenHeight / 2);
    }

    void moveMouseToStickPosition(double stickX, double stickY) {
        RECT overlayRect;
        GetWindowRect(overlayHwnd, &overlayRect);
        int overlayWidth = overlayRect.right - overlayRect.left;
        int overlayHeight = overlayRect.bottom - overlayRect.top;
        
        int centerX = overlayPosX + overlayWidth / 2;
        int centerY = overlayPosY + overlayHeight / 2;
        
        int mouseX = centerX + (int)(stickX * overlayStickRadius);
        int mouseY = centerY - (int)(stickY * overlayStickRadius);
        
        SetCursorPos(mouseX, mouseY);
    }

    void sendMouseButton(bool down) {
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));
    }

    void handleMouseControl(bool leftBumper, bool rightBumper, double leftX, double leftY, double rightX, double rightY) {
        bool anyBumperPressed = leftBumper || rightBumper;
        bool bothPressed = leftBumper && rightBumper;
        bool prevAnyPressed = prevLeftBumper || prevRightBumper;
        
        // Press LMB when any bumper is first pressed
        if (anyBumperPressed && !prevAnyPressed) {
            sendMouseButton(true);
            std::cout << "Bumper pressed: LMB activated" << std::endl;
        }
        
        // Release LMB when all bumpers are released
        if (!anyBumperPressed && prevAnyPressed) {
            sendMouseButton(false);
            moveMouseToCenter();
            std::cout << "All bumpers released: LMB released, mouse to center" << std::endl;
        }
        
        // Handle mouse positioning
        if (bothPressed) {
            // Both bumpers pressed - alternate between sticks every frame
            alternateFrame = !alternateFrame;
            if (alternateFrame) {
                moveMouseToStickPosition(leftX, leftY);
            } else {
                moveMouseToStickPosition(rightX, rightY);
            }
        } else if (leftBumper) {
            // Only left bumper - follow left stick
            moveMouseToStickPosition(leftX, leftY);
        } else if (rightBumper) {
            // Only right bumper - follow right stick
            moveMouseToStickPosition(rightX, rightY);
        }
        
        prevLeftBumper = leftBumper;
        prevRightBumper = rightBumper;
    }

    void updateOverlay(double leftX, double leftY, double rightX, double rightY, double leftAngle, double rightAngle) {
        overlayLeftX = leftX;
        overlayLeftY = leftY;
        overlayRightX = rightX;
        overlayRightY = rightY;
        overlayLeftAngle = leftAngle;
        overlayRightAngle = rightAngle;
        
        // Calculate distance from center for left stick
        double leftDistance = std::sqrt(leftX * leftX + leftY * leftY);
        // Map 0.0-0.5 distance to 0-255 alpha (max opacity at 50% from center)
        if (leftDistance >= 0.5) {
            overlayLeftAlpha = 255;
        } else {
            overlayLeftAlpha = (int)((leftDistance / 0.5) * 255.0);
        }
        
        // Calculate distance from center for right stick
        double rightDistance = std::sqrt(rightX * rightX + rightY * rightY);
        // Map 0.0-0.5 distance to 0-255 alpha (max opacity at 50% from center)
        if (rightDistance >= 0.5) {
            overlayRightAlpha = 255;
        } else {
            overlayRightAlpha = (int)((rightDistance / 0.5) * 255.0);
        }
        
        if (overlayHwnd) {
            // Use RedrawWindow for smoother updates without flicker
            RedrawWindow(overlayHwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOFRAME);
        }
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
                    logError("Failed to initialize selected controller!");
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
            Sleep(SELECTION_SLEEP_MS);
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
            logError("Failed to initialize DirectInput: " + std::to_string(hr));
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
            logError("Failed to set data format: " + std::to_string(hr));
            return false;
        }

        // Set cooperative level (background mode)
        hr = joystick->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
        if (FAILED(hr)) {
            logError("Failed to set cooperative level (background): " + std::to_string(hr));
            // Try foreground as fallback
            hr = joystick->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
            if (FAILED(hr)) {
                logError("Failed to set cooperative level (foreground): " + std::to_string(hr));
                return false;
            }
        }

        // Acquire the device
        hr = joystick->Acquire();
        if (FAILED(hr)) {
            logError("Failed to acquire device: " + std::to_string(hr));
            std::cerr << "This might be because another application is using the controller." << std::endl;
            std::cerr << "Try closing other controller applications and restart this program." << std::endl;
            return false;
        } else {
            logInfo("Controller acquired successfully!");
            return true;
        }
    }

    /**
     * Calculate angle from stick coordinates with 0° = top, clockwise
     * @param x X coordinate (-1.0 to 1.0)
     * @param y Y coordinate (-1.0 to 1.0)
     * @return Angle in degrees (0-360) or -1 if no input
     */
    double calculateAngle(double x, double y) {
        if (x == 0 && y == 0) {
            return -1; // No input detected
        }
        
        // Calculate angle with atan2 (0° = right, 90° = up, 180° = left, 270° = down)
        double angle = std::atan2(y, x) * 180.0 / PI;
        
        // Convert to 0-360 range (atan2 returns -180 to 180)
        if (angle < 0) {
            angle += 360;
        }
        
        // Rotate so 0° = top (90° becomes 0°) - this aligns with game expectations
        angle = angle - 90;
        if (angle < 0) {
            angle += 360;
        }
        
        // Make clockwise (game expects clockwise rotation from top)
        angle = 360 - angle;
        
        return angle;
    }

    /**
     * Convert angle to direction sector (0-7)
     * @param angle Angle in degrees (0-360) or -1 for no input
     * @return Direction sector (0-7) or -1 for no input
     * 
     * Direction mapping:
     * 0 = Up-Right, 1 = Right-Up, 2 = Right-Down, 3 = Down-Right
     * 4 = Down-Left, 5 = Left-Down, 6 = Left-Up, 7 = Up-Left
     */
    int getDirection(double angle) {
        if (angle == -1) return -1; // No input detected
        
        // Map 0-360 to 0-7 directions exactly like AHK script
        // Use Floor(angle / 45) without any offset for precise mapping
        int direction = static_cast<int>(std::floor(angle / DEGREES_PER_SECTOR));
        if (direction >= DIRECTION_SECTORS) {
            direction = 0;  // Reset to 0 if out of bounds (same as AHK script)
        }
        return direction;
    }

    void updateDebugInfo(double lAngle, double rAngle, int lDirection, int rDirection, const DIJOYSTATE* diState = nullptr) {
        // Update debug info every frame (no throttling) to match overlay refresh rate
        
        std::string info = "=== CONTROLLER TO MOUSE ===\r\n";
        info += "Version: Mouse + LMB Control\r\n";
        info += "Mode: Alternating dual-stick support\r\n";
        info += "Controller Type: " + std::string(hasXInputController ? "XInput" : "DirectInput") + "\r\n\r\n";
        
        if (hasXInputController) {
            info += getXInputDebugInfo();
        } else if (diState) {
            info += getDirectInputDebugInfo(diState);
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
        
        info += getButtonDebugInfo(diState);
        
        info += "MOUSE CONTROL:\r\n";
        bool leftPressed = prevLeftBumper;
        bool rightPressed = prevRightBumper;
        bool bothPressed = leftPressed && rightPressed;
        
        if (bothPressed) {
            info += "Mode: Alternating between both sticks\r\n";
            info += "Current frame: " + std::string(alternateFrame ? "Left stick" : "Right stick") + "\r\n";
        } else if (leftPressed) {
            info += "Mode: Left stick only\r\n";
        } else if (rightPressed) {
            info += "Mode: Right stick only\r\n";
        } else {
            info += "Mode: None (LMB released)\r\n";
        }
        info += "\r\n";
        
        info += "SHORTCUTS:\r\n";
        info += "Ctrl+Shift+` = Toggle debug info\r\n";
        info += "Ctrl+Alt+Shift+` = Restart program\r\n";

        // Store debug info for overlay rendering
        debugText = info;
    }

private:
    void logError(const std::string& message) {
        std::cerr << "[ERROR] " << message << std::endl;
    }
    
    void logInfo(const std::string& message) {
        std::cout << "[INFO] " << message << std::endl;
    }
    
    std::string getXInputDebugInfo() {
        std::string info = "XINPUT VALUES:\r\n";
        info += "Left Stick X: " + std::to_string(xInputState.Gamepad.sThumbLX) + "\r\n";
        info += "Left Stick Y: " + std::to_string(xInputState.Gamepad.sThumbLY) + "\r\n";
        info += "Right Stick X: " + std::to_string(xInputState.Gamepad.sThumbRX) + "\r\n";
        info += "Right Stick Y: " + std::to_string(xInputState.Gamepad.sThumbRY) + "\r\n\r\n";
        
        info += "NORMALIZED:\r\n";
        info += "Left X: " + std::to_string(xInputState.Gamepad.sThumbLX / STICK_MAX_VALUE) + "\r\n";
        info += "Left Y: " + std::to_string(xInputState.Gamepad.sThumbLY / STICK_MAX_VALUE) + "\r\n";
        info += "Right X: " + std::to_string(xInputState.Gamepad.sThumbRX / STICK_MAX_VALUE) + "\r\n";
        info += "Right Y: " + std::to_string(xInputState.Gamepad.sThumbRY / STICK_MAX_VALUE) + "\r\n\r\n";
        
        return info;
    }
    
    std::string getDirectInputDebugInfo(const DIJOYSTATE* diState) {
        std::string info = "DIRECTINPUT VALUES:\r\n";
        info += "X: " + std::to_string(diState->lX) + "\r\n";
        info += "Y: " + std::to_string(diState->lY) + "\r\n";
        info += "Z: " + std::to_string(diState->lZ) + "\r\n";
        info += "R: " + std::to_string(diState->lRz) + "\r\n\r\n";
        
        info += "NORMALIZED:\r\n";
        info += "X: " + std::to_string((diState->lX / STICK_NORMALIZE_FACTOR) - 1.0) + "\r\n";
        info += "Y: " + std::to_string(1.0 - (diState->lY / STICK_NORMALIZE_FACTOR)) + "\r\n";
        info += "Z: " + std::to_string((diState->lZ / STICK_NORMALIZE_FACTOR) - 1.0) + "\r\n";
        info += "R: " + std::to_string(1.0 - (diState->lRz / STICK_NORMALIZE_FACTOR)) + "\r\n\r\n";
        
        return info;
    }
    
    std::string getButtonDebugInfo(const DIJOYSTATE* diState) {
        std::string info = "BUTTONS:\r\n";
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
        return info;
    }

public:
    /**
     * Main application loop - processes controller input and updates GUI
     * Handles both XInput and DirectInput controllers
     * Updates debug information and manages key simulation
     */
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

            // Check keyboard shortcuts
            static bool prevTogglePressed = false;
            static bool prevRestartPressed = false;
            
            // Ctrl + Shift + ` = Toggle debug info
            bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            bool backtickDown = (GetAsyncKeyState(VK_OEM_3) & 0x8000) != 0; // ` key
            
            bool togglePressed = ctrlDown && shiftDown && !altDown && backtickDown;
            bool restartPressed = ctrlDown && shiftDown && altDown && backtickDown;
            
            // Toggle debug on key press (not hold)
            if (togglePressed && !prevTogglePressed) {
                showDebugInfo = !showDebugInfo;
                std::cout << "Debug info " << (showDebugInfo ? "enabled" : "disabled") << std::endl;
                if (overlayHwnd) {
                    RedrawWindow(overlayHwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOFRAME);
                }
            }
            
            // Restart program on key press
            if (restartPressed && !prevRestartPressed) {
                std::cout << "Restart requested via keyboard shortcut" << std::endl;
                PostQuitMessage(0);
                return;
            }
            
            prevTogglePressed = togglePressed;
            prevRestartPressed = restartPressed;

            // Process controller (always, regardless of focus)
            bool controllerSuccess = false;
            bool leftButtonPressed = false;
            bool rightButtonPressed = false;
            double joyX = 0, joyY = 0, joyZ = 0, joyR = 0;
            
            if (hasXInputController) {
                // Process XInput controller (Xbox controllers)
                DWORD result = XInputGetState(xInputControllerIndex, &xInputState);
                if (result == ERROR_SUCCESS) {
                    controllerSuccess = true;
                    
                    // XInput button mapping: Left Shoulder = LB, Right Shoulder = RB
                    leftButtonPressed = (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
                    rightButtonPressed = (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
                    
                    // XInput stick values: -32768 to 32767, normalize to -1.0 to 1.0
                    // For XInput, we need to match DirectInput coordinate system
                    joyX = xInputState.Gamepad.sThumbLX / STICK_MAX_VALUE;   // Left stick X
                    joyY = xInputState.Gamepad.sThumbLY / STICK_MAX_VALUE;   // Left stick Y (not inverted for XInput)
                    joyZ = xInputState.Gamepad.sThumbRX / STICK_MAX_VALUE;   // Right stick X
                    joyR = xInputState.Gamepad.sThumbRY / STICK_MAX_VALUE;   // Right stick Y (not inverted for XInput)
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

                // Get directions (for debug info)
                int lDirection = getDirection(lAngle);
                int rDirection = getDirection(rAngle);

                // Handle mouse control - first bumper controls mouse + LMB
                handleMouseControl(leftButtonPressed, rightButtonPressed, joyX, joyY, joyZ, joyR);

                // Update overlay with stick positions and angles
                updateOverlay(joyX, joyY, joyZ, joyR, lAngle, rAngle);

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

            Sleep(updateIntervalMs); // Match screen refresh rate
        }
        
        std::cout << "run() method ending..." << std::endl;
    }
};

int main() {
    // Allocate console for debug output
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
    
    std::cout << "=== CONTROLLER TO MOUSE ===" << std::endl;
    std::cout << "Version: Mouse + LMB Control" << std::endl;
    std::cout << "One bumper: Mouse follows that stick + holds LMB" << std::endl;
    std::cout << "Both bumpers: Mouse alternates between sticks every frame + holds LMB" << std::endl;
    std::cout << "Close the program by closing the console" << std::endl;
    std::cout << "Ctrl+Shift+` = Toggle debug info | Ctrl+Alt+Shift+` = Restart" << std::endl;
    std::cout << std::endl;

    while (true) {
    try {
        SimpleController app;
            if (!app.initialize()) {
                std::cerr << "[ERROR] Failed to initialize application!" << std::endl;
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
            
            std::cout << "=== CONTROLLER TO MOUSE ===" << std::endl;
            std::cout << "Version: Mouse + LMB Control" << std::endl;
            std::cout << "One bumper: Mouse follows that stick | Both: Alternates every frame" << std::endl;
            std::cout << "Ctrl+Shift+` = Toggle debug info | Ctrl+Alt+Shift+` = Restart" << std::endl;
            
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
