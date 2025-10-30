#define WIN32_LEAN_AND_MEAN
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

// C++/WinRT includes for UWP InputInjector (Touch mode)
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Input.Preview.Injection.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "windowsapp.lib")  // For UWP InputInjector

using namespace winrt;
using namespace Windows::UI::Input::Preview::Injection;

// ============================================
// ENUMS AND STRUCTURES
// ============================================

enum class ControllerType {
    XInput,       // Xbox controllers (Xbox 360, One, Series)
    DirectInput   // Generic DirectInput controllers
};

enum class InputMode {
    Touch,      // Multi-touch input using UWP InputInjector (for Sentakki)
    Mouse,      // Mouse cursor control + left click
    Keyboard    // Number keys 1-8 based on stick direction
};

struct ControllerInfo {
    ControllerType type;
    std::string name;
    DWORD index;  // Used for XInput controllers
    GUID guid;    // Used for DirectInput controllers
};

// ============================================
// MAIN CONTROLLER CLASS
// ============================================

class SimpleController {
private:
    // ========== GUI Components ==========
    LPDIRECTINPUT8 di;
    LPDIRECTINPUTDEVICE8 joystick;
    HWND hwnd;              // Main window (hidden)
    HWND overlayHwnd;       // Full-screen transparent overlay
    std::string debugText;  // Debug info displayed on overlay
    bool showDebugInfo;     // Toggle debug panel visibility
    
    // ========== Controller State ==========
    bool hasXInputController;
    DWORD xInputControllerIndex;
    XINPUT_STATE xInputState;
    
    // ========== Overlay Visualization ==========
    double overlayLeftX, overlayLeftY;      // Left stick (-1.0 to 1.0)
    double overlayRightX, overlayRightY;    // Right stick (-1.0 to 1.0)
    double overlayLeftAngle, overlayRightAngle;  // Angles in degrees
    int overlayLeftAlpha, overlayRightAlpha;     // Fade alpha (0-255)
    int overlayPosX, overlayPosY;           // Overlay screen position
    int overlayStickRadius;                 // Circle radius in pixels
    int updateIntervalMs;                   // Dynamic based on refresh rate
    
    // Locked pointer visualization
    double overlayLeftLockedX, overlayLeftLockedY;    // Left locked position
    double overlayRightLockedX, overlayRightLockedY;  // Right locked position
    int overlayLeftLockedAlpha, overlayRightLockedAlpha; // Locked pointer alpha
    
    // ========== Input Mode State ==========
    InputMode currentMode;  // Current operating mode (Touch/Mouse/Keyboard)
    
    // Touch mode state (UWP InputInjector)
    bool leftTouchActive;
    bool rightTouchActive;
    InputInjector inputInjector;
    bool inputInjectorInitialized;
    
    // Touch mode section tracking
    int currentLHeldDirection;  // Direction held when left bumper is pressed
    int currentRHeldDirection;  // Direction held when right bumper is pressed
    double currentLHeldX;       // X position when left direction was captured
    double currentLHeldY;       // Y position when left direction was captured
    double currentRHeldX;       // X position when right direction was captured
    double currentRHeldY;       // Y position when right direction was captured
    
    // Touch mode pointer locking (trigger-based)
    bool leftPointerLocked;     // Whether left touch is locked to a direction
    bool rightPointerLocked;    // Whether right touch is locked to a direction
    int leftLockedDirection;    // Direction the left touch is locked to
    int rightLockedDirection;   // Direction the right touch is locked to
    bool prevLeftTrigger;       // Previous left trigger state
    bool prevRightTrigger;      // Previous right trigger state
    
    // Mouse mode state
    bool mouseButtonPressed;
    bool alternateFrame;  // For dual-stick alternating mode
    
    // Keyboard mode state (number keys 1-8)
    std::string currentLeftKey;
    std::string currentRightKey;
    
    // Shared button tracking
    bool prevLeftBumper;
    bool prevRightBumper;
    
    // ========== Constants ==========
    static constexpr double PI = 3.14159265359;
    static constexpr int WINDOW_WIDTH = 480;
    static constexpr int WINDOW_HEIGHT = 640;
    static constexpr int SELECTION_SLEEP_MS = 4;  // For controller selection menu
    static constexpr double STICK_MAX_VALUE = 32767.0;
    static constexpr double STICK_NORMALIZE_FACTOR = 32767.5;
    static constexpr int DIRECTION_SECTORS = 8;  // 8 directional keys
    static constexpr double DEGREES_PER_SECTOR = 45.0;  // 360° / 8 = 45°
    static constexpr int OVERLAY_STICK_INDICATOR_RADIUS = 16;  // Pixel radius for stick indicators
    static constexpr int OVERLAY_LOCKED_INDICATOR_RADIUS = 14;  // Pixel radius for locked indicators (smaller)

public:
    // ========== Constructor & Initialization ==========
    
    SimpleController(InputMode mode = InputMode::Touch) : di(nullptr), joystick(nullptr), hwnd(nullptr), overlayHwnd(nullptr),
                        hasXInputController(false), xInputControllerIndex(0),
                        overlayLeftX(0.0), overlayLeftY(0.0), overlayRightX(0.0), overlayRightY(0.0),
                        overlayLeftAngle(-1.0), overlayRightAngle(-1.0), overlayStickRadius(150),
                        overlayLeftAlpha(0), overlayRightAlpha(0), updateIntervalMs(16),
                        overlayLeftLockedX(0.0), overlayLeftLockedY(0.0), overlayRightLockedX(0.0), overlayRightLockedY(0.0),
                        overlayLeftLockedAlpha(0), overlayRightLockedAlpha(0),
                        currentMode(mode), leftTouchActive(false), rightTouchActive(false), 
                        prevLeftBumper(false), prevRightBumper(false),
                        overlayPosX(0), overlayPosY(0), inputInjector(nullptr), inputInjectorInitialized(false),
                        currentLHeldDirection(-1), currentRHeldDirection(-1),
                        leftPointerLocked(false), rightPointerLocked(false),
                        leftLockedDirection(-1), rightLockedDirection(-1),
                        prevLeftTrigger(false), prevRightTrigger(false),
                        mouseButtonPressed(false), alternateFrame(false), currentLeftKey(""), currentRightKey(""),
                        debugText(""), showDebugInfo(true) {
        // Don't initialize controllers or create GUI in constructor
        // This will be done in the main loop
        
        // Initialize WinRT
        init_apartment();
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
    // ========== GUI Creation ==========
    
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
        // Use height as reference since width is now full-screen
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
        
        // Initialize touch injection
        initializeTouchInjection();
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
        
        // Draw direction indicators first (so they appear behind the stick indicators)
        // Calculate current directions from angles
        int leftDirection = getDirection(overlayLeftAngle);
        int rightDirection = getDirection(overlayRightAngle);
        
        // Draw direction indicators with overlap handling (only when sticks are actually moved with deadzone)
        // Use the same distance calculation as the overlay alpha system for consistency
        double leftDistance = std::sqrt(overlayLeftX * overlayLeftX + overlayLeftY * overlayLeftY);
        double rightDistance = std::sqrt(overlayRightX * overlayRightX + overlayRightY * overlayRightY);
        bool leftStickMoved = (leftDistance > 0.1); // Use distance-based deadzone like overlay
        bool rightStickMoved = (rightDistance > 0.1);
        
        if (leftStickMoved && rightStickMoved && leftDirection == rightDirection && leftDirection >= 0) {
            // Both sticks point to same direction - draw yellow blended arc
            int maxAlpha = (overlayLeftAlpha > overlayRightAlpha) ? overlayLeftAlpha : overlayRightAlpha;
            // Use active thickness if either bumper is pressed, otherwise use stick thickness
            int thickness = (leftTouchActive || rightTouchActive) ? -1 : (1 + (maxAlpha * 5 / 255));
            drawDirectionIndicator(hdc, centerX, centerY, leftDirection, RGB(255, 255, 0), maxAlpha, thickness);
        } else {
            // Different directions or only one moved - draw each separately
            if (leftStickMoved && leftDirection >= 0) {
                // Use active thickness if bumper is pressed, otherwise use stick thickness
                int thickness = leftTouchActive ? -1 : (1 + (overlayLeftAlpha * 5 / 255));
                drawDirectionIndicator(hdc, centerX, centerY, leftDirection, RGB(100, 150, 255), overlayLeftAlpha, thickness);
            }
            if (rightStickMoved && rightDirection >= 0) {
                // Use active thickness if bumper is pressed, otherwise use stick thickness
                int thickness = rightTouchActive ? -1 : (1 + (overlayRightAlpha * 5 / 255));
                drawDirectionIndicator(hdc, centerX, centerY, rightDirection, RGB(255, 100, 150), overlayRightAlpha, thickness);
            }
        }
        
        // Draw both sticks at the same position (they will overlap)
        // Draw left stick (blue)
        drawStick(hdc, centerX, centerY, overlayLeftX, overlayLeftY, RGB(100, 150, 255), overlayLeftAlpha);
        
        // Draw right stick (pink)
        drawStick(hdc, centerX, centerY, overlayRightX, overlayRightY, RGB(255, 100, 150), overlayRightAlpha);
        
        // Draw locked pointers (solid circles, bigger, only when triggers are active)
        // Left locked pointer (darker blue) - draw below raw pointer
        drawLockedPointer(hdc, centerX, centerY, overlayLeftLockedX, overlayLeftLockedY, RGB(50, 100, 200), overlayLeftLockedAlpha);
        
        // Right locked pointer (darker pink) - draw below raw pointer
        drawLockedPointer(hdc, centerX, centerY, overlayRightLockedX, overlayRightLockedY, RGB(200, 50, 100), overlayRightLockedAlpha);
        
        // Draw debug text on the middle left (if enabled)
        if (showDebugInfo && !debugText.empty()) {
            drawDebugText(hdc, rect);
        }
    }


    void drawDirectionIndicator(HDC hdc, int centerX, int centerY, int direction, COLORREF color, int alpha, int thickness = -1) {
        if (direction < 0 || alpha == 0) return; // No input detected or invisible
        
        // Skip if too faint to avoid artifacts
        if (alpha < 10) return;
        
        // Draw direction indicators: current direction full opaque, adjacent directions half opaque
        for (int dir = 0; dir < DIRECTION_SECTORS; dir++) {
            int currentAlpha;
            if (dir == direction) {
                currentAlpha = alpha; // Full opacity for current direction
            } else if (dir == (direction - 1 + DIRECTION_SECTORS) % DIRECTION_SECTORS || 
                       dir == (direction + 1) % DIRECTION_SECTORS) {
                currentAlpha = 1; // Almost invisible for adjacent directions
            } else {
                continue; // Skip non-adjacent directions
            }
            
            // Calculate angle for this direction
            // Direction 0 should be Up-Right (22.5°), not directly up (0°)
            double dirAngle = (dir * DEGREES_PER_SECTOR) + 22.5;
            if (dirAngle >= 360.0) {
                dirAngle -= 360.0;
            }
            
            // Draw an arc for this direction (45 degrees span)
            double arcSpan = DEGREES_PER_SECTOR; // 45 degrees per direction
            double arcStartAngle = dirAngle - arcSpan / 2.0;
            double arcEndAngle = dirAngle + arcSpan / 2.0;
            
            // Convert to radians (standard math: 0° = right, counter-clockwise)
            double startRad = (90 - arcStartAngle) * PI / 180.0;
            double endRad = (90 - arcEndAngle) * PI / 180.0;
            
            // Calculate arc endpoints on the boundary circle
            int startX = centerX + (int)(cos(startRad) * overlayStickRadius);
            int startY = centerY - (int)(sin(startRad) * overlayStickRadius);
            int endX = centerX + (int)(cos(endRad) * overlayStickRadius);
            int endY = centerY - (int)(sin(endRad) * overlayStickRadius);
            
            // Modulate pen width based on alpha for fade effect
            int penWidth;
            if (dir == direction) {
                // Main direction - use full thickness
                if (thickness == -1) {
                    // Default active thickness (when bumper pressed)
                    penWidth = 2 + (currentAlpha * 8 / 255); // 2-10 pixels
                } else {
                    // Use specified thickness (for inactive state)
                    penWidth = thickness;
                }
            } else {
                // Adjacent directions - always small/thin
                penWidth = 1; // Always thin for adjacent directions
            }
            
            // Draw the arc along the main circle boundary
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

    void drawLockedPointer(HDC hdc, int centerX, int centerY, double stickX, double stickY, COLORREF color, int alpha) {
        if (alpha == 0) return; // Don't draw if invisible
        
        // Draw locked pointer - solid filled circle (smaller than regular pointer)
        int indicatorX = centerX + (int)(stickX * (overlayStickRadius - OVERLAY_STICK_INDICATOR_RADIUS));
        int indicatorY = centerY - (int)(stickY * (overlayStickRadius - OVERLAY_STICK_INDICATOR_RADIUS)); // Inverted Y
        
        // Create solid brush for filled circle
        HBRUSH solidBrush = CreateSolidBrush(color);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, solidBrush);
        HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
        
        Ellipse(hdc,
                indicatorX - OVERLAY_LOCKED_INDICATOR_RADIUS,
                indicatorY - OVERLAY_LOCKED_INDICATOR_RADIUS,
                indicatorX + OVERLAY_LOCKED_INDICATOR_RADIUS,
                indicatorY + OVERLAY_LOCKED_INDICATOR_RADIUS);
        
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(solidBrush);
    }

    void drawDebugText(HDC hdc, RECT rect) {
        // Position at bottom-left, 120px from bottom
        int textX = 30;
        int lineHeight = 24;
        
        // Calculate text height to position from bottom
        int lineCount = 0;
        std::string tempText = debugText;
        size_t pos = 0;
        while ((pos = tempText.find("\r\n")) != std::string::npos) {
            lineCount++;
            tempText.erase(0, pos + 2);
        }
        if (!tempText.empty()) lineCount++;
        
        int textHeight = lineCount * lineHeight;
        int textY = rect.bottom - textHeight - 120; // 120px from bottom
        
        if (textY < 30) textY = 30; // Don't go too high
        
        // Create large, bold font for easy reading
        HFONT hFont = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, "Consolas");
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
        
        // Set up text rendering - no background, just clean text
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255)); // White text
        
        // Render text lines
        std::string text = debugText;
        size_t textPos = 0;
        int currentY = textY;
        
        while ((textPos = text.find("\r\n")) != std::string::npos) {
            std::string line = text.substr(0, textPos);
            TextOutA(hdc, textX, currentY, line.c_str(), line.length());
            currentY += lineHeight;
            text.erase(0, textPos + 2);
        }
        if (!text.empty()) {
            TextOutA(hdc, textX, currentY, text.c_str(), text.length());
        }
        
        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
    }

    // ========== Touch Mode (UWP InputInjector) ==========
    
    void initializeTouchInjection() {
        if (!inputInjectorInitialized) {
            try {
                // Create UWP InputInjector - this should work WITHOUT touch hardware!
                inputInjector = InputInjector::TryCreate();
                
                if (inputInjector) {
                    // Initialize for touch input with NO visualization (prevents on-screen keyboard)
                    inputInjector.InitializeTouchInjection(InjectedInputVisualizationMode::None);
                    inputInjectorInitialized = true;
                    std::cout << "UWP InputInjector initialized successfully!" << std::endl;
                    std::cout << "Touch injection enabled (no on-screen keyboard)" << std::endl;
                } else {
                    std::cout << "Failed to create InputInjector - system may not support UWP input injection" << std::endl;
                }
            } catch (hresult_error const& ex) {
                std::wcout << L"Exception creating InputInjector: " << ex.message().c_str() << std::endl;
                std::cout << "Error code: 0x" << std::hex << ex.code() << std::dec << std::endl;
            }
        }
    }

    void getTouchCoordinates(double stickX, double stickY, LONG& touchX, LONG& touchY) {
        // Get the overlay window size
        RECT overlayRect;
        GetWindowRect(overlayHwnd, &overlayRect);
        int overlayWidth = overlayRect.right - overlayRect.left;
        int overlayHeight = overlayRect.bottom - overlayRect.top;
        
        // Calculate center of overlay
        int centerX = overlayPosX + overlayWidth / 2;
        int centerY = overlayPosY + overlayHeight / 2;
        
        // Map stick position (-1.0 to 1.0) to screen coordinates
        touchX = centerX + (int)(stickX * overlayStickRadius);
        touchY = centerY - (int)(stickY * overlayStickRadius); // Invert Y
        
        // Clamp to screen bounds to ensure valid coordinates
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        if (touchX < 0) touchX = 0;
        if (touchX >= screenWidth) touchX = screenWidth - 1;
        if (touchY < 0) touchY = 0;
        if (touchY >= screenHeight) touchY = screenHeight - 1;
    }

    void pixelToHimetric(LONG pixelX, LONG pixelY, LONG& himetricX, LONG& himetricY) {
        // Convert pixel coordinates to HIMETRIC (hundredths of a millimeter)
        HDC screenDC = GetDC(nullptr);
        int dpiX = GetDeviceCaps(screenDC, LOGPIXELSX);
        int dpiY = GetDeviceCaps(screenDC, LOGPIXELSY);
        ReleaseDC(nullptr, screenDC);
        
        // HIMETRIC calculation: (pixels * 2540) / DPI
        himetricX = (pixelX * 2540) / dpiX;
        himetricY = (pixelY * 2540) / dpiY;
    }

    InjectedInputTouchInfo createTouchInfo(int touchId, double stickX, double stickY, bool isDown, bool isUp) {
        InjectedInputTouchInfo touchInfo{};
        
        // Get screen coordinates (in pixels)
        LONG touchX, touchY;
        getTouchCoordinates(stickX, stickY, touchX, touchY);
        
        // Create pixel location structure
        InjectedInputPoint pixelLocation{};
        pixelLocation.PositionX = touchX;
        pixelLocation.PositionY = touchY;
        
        // Create pointer info structure
        InjectedInputPointerInfo pointerInfo{};
        pointerInfo.PointerId = touchId;
        pointerInfo.PixelLocation = pixelLocation;
        
        // Set pointer options/flags (direct assignment for struct fields)
        if (isDown) {
            pointerInfo.PointerOptions =
                InjectedInputPointerOptions::PointerDown |
                InjectedInputPointerOptions::InContact |
                InjectedInputPointerOptions::InRange |
                InjectedInputPointerOptions::New;
        } else if (isUp) {
            pointerInfo.PointerOptions = InjectedInputPointerOptions::PointerUp;
        } else {
            pointerInfo.PointerOptions =
                InjectedInputPointerOptions::Update |
                InjectedInputPointerOptions::InContact |
                InjectedInputPointerOptions::InRange;
        }
        
        // InjectedInputTouchInfo uses setter methods (not direct assignment)
        touchInfo.PointerInfo(pointerInfo);
        
        // Set touch parameters (using setter methods)
        touchInfo.Pressure(1.0);  // Full pressure
        touchInfo.TouchParameters(
            InjectedInputTouchParameters::Pressure |
            InjectedInputTouchParameters::Contact
        );
        
        // Set contact area (30x30 pixels - finger-sized)
        InjectedInputRectangle contactArea{};
        contactArea.Left = 15;
        contactArea.Top = 15;
        contactArea.Bottom = 15;
        contactArea.Right = 15;
        touchInfo.Contact(contactArea);
        
        return touchInfo;
    }

    void sendMultipleTouches(const std::vector<InjectedInputTouchInfo>& touches) {
        if (!inputInjectorInitialized || !inputInjector || touches.empty()) return;
        
        try {
            inputInjector.InjectTouchInput(touches);
        } catch (hresult_error const& ex) {
            static int errorCount = 0;
            if (errorCount < 3) {
                std::wcout << L"Touch injection failed: " << ex.message().c_str() << std::endl;
                std::cout << "Error code: 0x" << std::hex << ex.code() << std::dec << std::endl;
                errorCount++;
                if (errorCount >= 3) {
                    std::cout << "(Further errors suppressed)" << std::endl;
                }
            }
        }
    }

    void sendTouch(int touchId, double stickX, double stickY, bool isDown, bool isUp) {
        if (!inputInjectorInitialized || !inputInjector) return;
        
        // Use the helper function to create touch info
        InjectedInputTouchInfo touchInfo = createTouchInfo(touchId, stickX, stickY, isDown, isUp);
        
        try {
            // Get coordinates for debug output
            LONG touchX, touchY;
            getTouchCoordinates(stickX, stickY, touchX, touchY);
            
            // Create collection and inject
            std::vector<InjectedInputTouchInfo> touchData;
            touchData.push_back(touchInfo);
            
            inputInjector.InjectTouchInput(touchData);
            
            static bool firstSuccess = true;
            if (firstSuccess && isDown) {
                std::cout << "UWP Touch injection working! Touch " << touchId << " at (" << touchX << ", " << touchY << ")" << std::endl;
                std::cout << "Multi-touch enabled with 2 independent touch points!" << std::endl;
                std::cout << "\nIMPORTANT: Position the overlay circle over your game window!" << std::endl;
                std::cout << "Touches are sent to the center of the overlay circle." << std::endl;
                firstSuccess = false;
            }
            
            // Debug: Print every 10th touch to show it's working
            static int touchCount = 0;
            if (isDown || isUp) {
                touchCount++;
                if (touchCount % 5 == 0) {
                    std::cout << "Touch " << touchId << (isDown ? " DOWN" : (isUp ? " UP" : " MOVE")) 
                             << " at screen (" << touchX << ", " << touchY << ")" << std::endl;
                }
            }
            
        } catch (hresult_error const& ex) {
            static int errorCount = 0;
            if (errorCount < 3) {
                std::wcout << L"Touch injection failed: " << ex.message().c_str() << std::endl;
                std::cout << "Error code: 0x" << std::hex << ex.code() << std::dec << std::endl;
                errorCount++;
                if (errorCount >= 3) {
                    std::cout << "(Further errors suppressed)" << std::endl;
                }
            }
        }
    }

    void sendBothTouchesIfActive(double leftX, double leftY, double rightX, double rightY,
                                 double leftLockedX, double leftLockedY, bool leftLocked,
                                 double rightLockedX, double rightLockedY, bool rightLocked) {
        // Only send both touches if both are active
        if (!leftTouchActive || !rightTouchActive) return;
        if (!inputInjectorInitialized || !inputInjector) return;
        
        try {
            std::vector<InjectedInputTouchInfo> touchData;
            
            // Add left touch
            double leftSendX = leftLocked ? leftLockedX : leftX;
            double leftSendY = leftLocked ? leftLockedY : leftY;
            touchData.push_back(createTouchInfo(0, leftSendX, leftSendY, false, false));
            
            // Add right touch
            double rightSendX = rightLocked ? rightLockedX : rightX;
            double rightSendY = rightLocked ? rightLockedY : rightY;
            touchData.push_back(createTouchInfo(1, rightSendX, rightSendY, false, false));
            
            // Send both touches together
            sendMultipleTouches(touchData);
        } catch (hresult_error const& ex) {
            static int errorCount = 0;
            if (errorCount < 3) {
                std::wcout << L"Multi-touch injection failed: " << ex.message().c_str() << std::endl;
                std::cout << "Error code: 0x" << std::hex << ex.code() << std::dec << std::endl;
                errorCount++;
                if (errorCount >= 3) {
                    std::cout << "(Further errors suppressed)" << std::endl;
                }
            }
        }
    }

    void handleTouchControl(bool leftBumper, bool rightBumper, bool leftTrigger, bool rightTrigger, double leftX, double leftY, double rightX, double rightY) {
        // Calculate angles and directions (like in keyboard mode) - for future use
        double lAngle = calculateAngle(leftX, leftY);
        double rAngle = calculateAngle(rightX, rightY);
        int lDirection = getDirection(lAngle);
        int rDirection = getDirection(rAngle);
        
        // Detect trigger press edges for pointer locking
        bool leftTriggerPressed = leftTrigger && !prevLeftTrigger;
        bool rightTriggerPressed = rightTrigger && !prevRightTrigger;
        bool leftTriggerReleased = !leftTrigger && prevLeftTrigger;
        bool rightTriggerReleased = !rightTrigger && prevRightTrigger;
        
        // Detect bumper press edges for touch input
        bool leftPressed = leftBumper && !prevLeftBumper;
        bool rightPressed = rightBumper && !prevRightBumper;
        bool leftReleased = !leftBumper && prevLeftBumper;
        bool rightReleased = !rightBumper && prevRightBumper;
        
        // Handle stick click-based pointer locking and touch events
        // Left stick click (L3) controls left touch locking
        if (leftTriggerPressed) {
            currentLHeldDirection = lDirection; // Capture current direction when stick clicked
            currentLHeldX = leftX; // Capture actual stick position
            currentLHeldY = leftY;
            leftTouchActive = true; // Start touch event
            sendTouch(0, leftX, leftY, true, false); // Touch down
        }
        
        if (leftTriggerReleased) {
            leftPointerLocked = false; // Release lock when stick released
            leftLockedDirection = -1;
            currentLHeldDirection = -1;
            if (leftTouchActive) {
                sendTouch(0, leftX, leftY, false, true); // Touch up
                leftTouchActive = false;
            }
        }
        
        // Right stick click (R3) controls right touch locking
        if (rightTriggerPressed) {
            currentRHeldDirection = rDirection; // Capture current direction when stick clicked
            currentRHeldX = rightX; // Capture actual stick position
            currentRHeldY = rightY;
            rightTouchActive = true; // Start touch event
            sendTouch(1, rightX, rightY, true, false); // Touch down
        }
        
        if (rightTriggerReleased) {
            rightPointerLocked = false; // Release lock when stick released
            rightLockedDirection = -1;
            currentRHeldDirection = -1;
            if (rightTouchActive) {
                sendTouch(1, rightX, rightY, false, true); // Touch up
                rightTouchActive = false;
            }
        }
        
        // Handle left bumper - touch ID 0
        if (leftPressed && !leftTouchActive) {
            leftTouchActive = true;
            sendTouch(0, leftX, leftY, true, false); // Touch down - only send once on press
        } else if (leftTouchActive && leftBumper) {
            // Check for pointer locking (trigger-based)
            if (leftTrigger && currentLHeldDirection >= 0) {
                int newLockedDirection;
                if (checkPointerLock(currentLHeldDirection, lDirection, lAngle, newLockedDirection)) {
                    if (!leftPointerLocked || leftLockedDirection != newLockedDirection) {
                        leftPointerLocked = true;
                        leftLockedDirection = newLockedDirection;
                    }
                    // Use path projection for positioning - project raw stick onto path from held to adjacent
                    double lockedX, lockedY;
                    
                    // Calculate the path vector from held direction to the chosen adjacent direction
                    double heldAngle = (currentLHeldDirection * DEGREES_PER_SECTOR) + 22.5;
                    if (heldAngle >= 360.0) heldAngle -= 360.0;
                    
                    double endAngle = (leftLockedDirection * DEGREES_PER_SECTOR) + 22.5;
                    if (endAngle >= 360.0) endAngle -= 360.0;
                    
                    // Calculate path direction vector (from held to adjacent)
                    double pathX = std::sin(endAngle * PI / 180.0) - std::sin(heldAngle * PI / 180.0);
                    double pathY = std::cos(endAngle * PI / 180.0) - std::cos(heldAngle * PI / 180.0);
                    
                    // Normalize the path vector
                    double pathLength = std::sqrt(pathX * pathX + pathY * pathY);
                    if (pathLength > 0) {
                        pathX /= pathLength;
                        pathY /= pathLength;
                    }
                    
                    // Calculate locked position along the path segment (from held to adjacent)
                    // Use center of direction arc instead of captured position
                    double heldX, heldY;
                    getDirectionArcCenter(currentLHeldDirection, heldX, heldY);
                    double rawDx = leftX - heldX;
                    double rawDy = leftY - heldY;
                    double t = rawDx * pathX + rawDy * pathY; // projection length along unit path
                    // Clamp t to [0, pathLength] to stay within the segment
                    if (t < 0.0) t = 0.0;
                    if (t > pathLength) t = pathLength;
                    lockedX = heldX + t * pathX;
                    lockedY = heldY + t * pathY;
                    sendTouch(0, lockedX, lockedY, false, false); // Touch move/update with locked position
                } else {
                    // No locking - use current position
                    if (leftPointerLocked) {
                        leftPointerLocked = false;
                        leftLockedDirection = -1;
                    }
                    sendTouch(0, leftX, leftY, false, false); // Touch move/update
                }
            } else {
                // No trigger pressed - use current position
                if (leftPointerLocked) {
                    leftPointerLocked = false;
                    leftLockedDirection = -1;
                    std::cout << "Left pointer UNLOCKED" << std::endl;
                }
                sendTouch(0, leftX, leftY, false, false); // Touch move/update
            }
        }
        
        if (leftReleased && leftTouchActive) {
            sendTouch(0, leftX, leftY, false, true); // Touch up
            leftTouchActive = false;
            std::cout << "Left bumper released: Sending Touch 0 UP" << std::endl;
        }
        
        // Handle left trigger movement while held
        if (leftTouchActive && leftTrigger) {
            // Check for pointer locking (trigger-based)
            if (currentLHeldDirection >= 0) {
                int newLockedDirection;
                if (checkPointerLock(currentLHeldDirection, lDirection, lAngle, newLockedDirection)) {
                    if (!leftPointerLocked || leftLockedDirection != newLockedDirection) {
                        leftPointerLocked = true;
                        leftLockedDirection = newLockedDirection;
                    }
                    // Use path projection for positioning - project raw stick onto path from held to adjacent
                    double lockedX, lockedY;
                    
                    // Calculate the path vector from held direction to the chosen adjacent direction
                    double heldAngle = (currentLHeldDirection * DEGREES_PER_SECTOR) + 22.5;
                    if (heldAngle >= 360.0) heldAngle -= 360.0;
                    
                    double endAngle = (leftLockedDirection * DEGREES_PER_SECTOR) + 22.5;
                    if (endAngle >= 360.0) endAngle -= 360.0;
                    
                    // Calculate path direction vector (from held to adjacent)
                    double pathX = std::sin(endAngle * PI / 180.0) - std::sin(heldAngle * PI / 180.0);
                    double pathY = std::cos(endAngle * PI / 180.0) - std::cos(heldAngle * PI / 180.0);
                    
                    // Normalize the path vector
                    double pathLength = std::sqrt(pathX * pathX + pathY * pathY);
                    if (pathLength > 0) {
                        pathX /= pathLength;
                        pathY /= pathLength;
                    }
                    
                    // Calculate locked position along the path segment (from held to adjacent)
                    // Use center of direction arc instead of captured position
                    double heldX, heldY;
                    getDirectionArcCenter(currentLHeldDirection, heldX, heldY);
                    double rawDx = leftX - heldX;
                    double rawDy = leftY - heldY;
                    double t = rawDx * pathX + rawDy * pathY; // projection length along unit path
                    // Clamp t to [0, pathLength] to stay within the segment
                    if (t < 0.0) t = 0.0;
                    if (t > pathLength) t = pathLength;
                    lockedX = heldX + t * pathX;
                    lockedY = heldY + t * pathY;
                    sendTouch(0, lockedX, lockedY, false, false); // Touch move/update with locked position
                } else {
                    // No locking - use current position
                    if (leftPointerLocked) {
                        leftPointerLocked = false;
                        leftLockedDirection = -1;
                    }
                    sendTouch(0, leftX, leftY, false, false); // Touch move/update
                }
            } else {
                // No trigger pressed - use current position
                if (leftPointerLocked) {
                    leftPointerLocked = false;
                    leftLockedDirection = -1;
                    std::cout << "Left pointer UNLOCKED" << std::endl;
                }
                sendTouch(0, leftX, leftY, false, false); // Touch move/update
            }
        }
        
        if (leftReleased && leftTouchActive) {
            sendTouch(0, leftX, leftY, false, true); // Touch up
            leftTouchActive = false;
            std::cout << "Left bumper released: Sending Touch 0 UP" << std::endl;
        }
        
        // Handle left trigger movement while held
        if (leftTouchActive && leftTrigger) {
            // Check for pointer locking (trigger-based)
            if (currentLHeldDirection >= 0) {
                int newLockedDirection;
                if (checkPointerLock(currentLHeldDirection, lDirection, lAngle, newLockedDirection)) {
                    if (!leftPointerLocked || leftLockedDirection != newLockedDirection) {
                        leftPointerLocked = true;
                        leftLockedDirection = newLockedDirection;
                    }
                    // Use path projection for positioning - project raw stick onto path from held to adjacent
                    double lockedX, lockedY;
                    
                    // Calculate the path vector from held direction to the chosen adjacent direction
                    double heldAngle = (currentLHeldDirection * DEGREES_PER_SECTOR) + 22.5;
                    if (heldAngle >= 360.0) heldAngle -= 360.0;
                    
                    double endAngle = (leftLockedDirection * DEGREES_PER_SECTOR) + 22.5;
                    if (endAngle >= 360.0) endAngle -= 360.0;
                    
                    // Calculate path direction vector (from held to adjacent)
                    double pathX = std::sin(endAngle * PI / 180.0) - std::sin(heldAngle * PI / 180.0);
                    double pathY = std::cos(endAngle * PI / 180.0) - std::cos(heldAngle * PI / 180.0);
                    
                    // Normalize the path vector
                    double pathLength = std::sqrt(pathX * pathX + pathY * pathY);
                    if (pathLength > 0) {
                        pathX /= pathLength;
                        pathY /= pathLength;
                    }
                    
                    // Calculate locked position along the path segment (from held to adjacent)
                    // Use center of direction arc instead of captured position
                    double heldX, heldY;
                    getDirectionArcCenter(currentLHeldDirection, heldX, heldY);
                    double rawDx = leftX - heldX;
                    double rawDy = leftY - heldY;
                    double t = rawDx * pathX + rawDy * pathY; // projection length along unit path
                    // Clamp t to [0, pathLength] to stay within the segment
                    if (t < 0.0) t = 0.0;
                    if (t > pathLength) t = pathLength;
                    lockedX = heldX + t * pathX;
                    lockedY = heldY + t * pathY;
                    sendTouch(0, lockedX, lockedY, false, false); // Touch move/update with locked position
                } else {
                    // No locking - use current position
                    if (leftPointerLocked) {
                        leftPointerLocked = false;
                        leftLockedDirection = -1;
                    }
                    sendTouch(0, leftX, leftY, false, false); // Touch move/update
                }
            } else {
                // No trigger pressed - use current position
                if (leftPointerLocked) {
                    leftPointerLocked = false;
                    leftLockedDirection = -1;
                    std::cout << "Left pointer UNLOCKED" << std::endl;
                }
                sendTouch(0, leftX, leftY, false, false); // Touch move/update
            }
        }
        
        // Handle right bumper - touch ID 1
        if (rightPressed && !rightTouchActive) {
            rightTouchActive = true;
            sendTouch(1, rightX, rightY, true, false); // Touch down - only send once on press
            std::cout << "Right bumper pressed: Sending Touch 1 DOWN" << std::endl;
        } else if (rightTouchActive && rightBumper) {
            // Check for pointer locking (trigger-based)
            if (rightTrigger && currentRHeldDirection >= 0) {
                int newLockedDirection;
                if (checkPointerLock(currentRHeldDirection, rDirection, rAngle, newLockedDirection)) {
                    if (!rightPointerLocked || rightLockedDirection != newLockedDirection) {
                        rightPointerLocked = true;
                        rightLockedDirection = newLockedDirection;
                    }
                    // Use path projection for positioning - project raw stick onto path from held to adjacent
                    double lockedX, lockedY;
                    
                    // Calculate the path vector from held direction to the chosen adjacent direction
                    double heldAngle = (currentRHeldDirection * DEGREES_PER_SECTOR) + 22.5;
                    if (heldAngle >= 360.0) heldAngle -= 360.0;
                    
                    double endAngle = (rightLockedDirection * DEGREES_PER_SECTOR) + 22.5;
                    if (endAngle >= 360.0) endAngle -= 360.0;
                    
                    // Calculate path direction vector (from held to adjacent)
                    double pathX = std::sin(endAngle * PI / 180.0) - std::sin(heldAngle * PI / 180.0);
                    double pathY = std::cos(endAngle * PI / 180.0) - std::cos(heldAngle * PI / 180.0);
                    
                    // Normalize the path vector
                    double pathLength = std::sqrt(pathX * pathX + pathY * pathY);
                    if (pathLength > 0) {
                        pathX /= pathLength;
                        pathY /= pathLength;
                    }
                    
                    // Calculate locked position along the path segment (from held to adjacent)
                    // Use center of direction arc instead of captured position
                    double heldX, heldY;
                    getDirectionArcCenter(currentRHeldDirection, heldX, heldY);
                    double rawDx = rightX - heldX;
                    double rawDy = rightY - heldY;
                    double t = rawDx * pathX + rawDy * pathY; // projection length along unit path
                    // Clamp t to [0, pathLength] to stay within the segment
                    if (t < 0.0) t = 0.0;
                    if (t > pathLength) t = pathLength;
                    lockedX = heldX + t * pathX;
                    lockedY = heldY + t * pathY;
                    sendTouch(1, lockedX, lockedY, false, false); // Touch move/update with locked position
                } else {
                    // No locking - use current position
                    if (rightPointerLocked) {
                        rightPointerLocked = false;
                        rightLockedDirection = -1;
                    }
                    sendTouch(1, rightX, rightY, false, false); // Touch move/update
                }
            } else {
                // No trigger pressed - use current position
                if (rightPointerLocked) {
                    rightPointerLocked = false;
                    rightLockedDirection = -1;
                    std::cout << "Right pointer UNLOCKED" << std::endl;
                }
                sendTouch(1, rightX, rightY, false, false); // Touch move/update
            }
        }
        
        if (rightReleased && rightTouchActive) {
            sendTouch(1, rightX, rightY, false, true); // Touch up
            rightTouchActive = false;
            std::cout << "Right bumper released: Sending Touch 1 UP" << std::endl;
        }
        
        // Handle right trigger movement while held
        if (rightTouchActive && rightTrigger) {
            // Check for pointer locking (trigger-based)
            if (currentRHeldDirection >= 0) {
                int newLockedDirection;
                if (checkPointerLock(currentRHeldDirection, rDirection, rAngle, newLockedDirection)) {
                    if (!rightPointerLocked || rightLockedDirection != newLockedDirection) {
                        rightPointerLocked = true;
                        rightLockedDirection = newLockedDirection;
                    }
                    // Use path projection for positioning - project raw stick onto path from held to adjacent
                    double lockedX, lockedY;
                    
                    // Calculate the path vector from held direction to the chosen adjacent direction
                    double heldAngle = (currentRHeldDirection * DEGREES_PER_SECTOR) + 22.5;
                    if (heldAngle >= 360.0) heldAngle -= 360.0;
                    
                    double endAngle = (rightLockedDirection * DEGREES_PER_SECTOR) + 22.5;
                    if (endAngle >= 360.0) endAngle -= 360.0;
                    
                    // Calculate path direction vector (from held to adjacent)
                    double pathX = std::sin(endAngle * PI / 180.0) - std::sin(heldAngle * PI / 180.0);
                    double pathY = std::cos(endAngle * PI / 180.0) - std::cos(heldAngle * PI / 180.0);
                    
                    // Normalize the path vector
                    double pathLength = std::sqrt(pathX * pathX + pathY * pathY);
                    if (pathLength > 0) {
                        pathX /= pathLength;
                        pathY /= pathLength;
                    }
                    
                    // Calculate locked position along the path segment (from held to adjacent)
                    // Use center of direction arc instead of captured position
                    double heldX, heldY;
                    getDirectionArcCenter(currentRHeldDirection, heldX, heldY);
                    double rawDx = rightX - heldX;
                    double rawDy = rightY - heldY;
                    double t = rawDx * pathX + rawDy * pathY; // projection length along unit path
                    // Clamp t to [0, pathLength] to stay within the segment
                    if (t < 0.0) t = 0.0;
                    if (t > pathLength) t = pathLength;
                    lockedX = heldX + t * pathX;
                    lockedY = heldY + t * pathY;
                    sendTouch(1, lockedX, lockedY, false, false); // Touch move/update with locked position
                } else {
                    // No locking - use current position
                    if (rightPointerLocked) {
                        rightPointerLocked = false;
                        rightLockedDirection = -1;
                    }
                    sendTouch(1, rightX, rightY, false, false); // Touch move/update
                }
            } else {
                // No trigger pressed - use current position
                if (rightPointerLocked) {
                    rightPointerLocked = false;
                    rightLockedDirection = -1;
                    std::cout << "Right pointer UNLOCKED" << std::endl;
                }
                sendTouch(1, rightX, rightY, false, false); // Touch move/update
            }
        }
        
        if (rightReleased && rightTouchActive) {
            sendTouch(1, rightX, rightY, false, true); // Touch up
            rightTouchActive = false;
            std::cout << "Right bumper released: Sending Touch 1 UP" << std::endl;
        }
        
        // Handle right trigger movement while held
        if (rightTouchActive && rightTrigger) {
            // Check for pointer locking (trigger-based)
            if (currentRHeldDirection >= 0) {
                int newLockedDirection;
                if (checkPointerLock(currentRHeldDirection, rDirection, rAngle, newLockedDirection)) {
                    if (!rightPointerLocked || rightLockedDirection != newLockedDirection) {
                        rightPointerLocked = true;
                        rightLockedDirection = newLockedDirection;
                    }
                    // Use path projection for positioning - project raw stick onto path from held to adjacent
                    double lockedX, lockedY;
                    
                    // Calculate the path vector from held direction to the chosen adjacent direction
                    double heldAngle = (currentRHeldDirection * DEGREES_PER_SECTOR) + 22.5;
                    if (heldAngle >= 360.0) heldAngle -= 360.0;
                    
                    double endAngle = (rightLockedDirection * DEGREES_PER_SECTOR) + 22.5;
                    if (endAngle >= 360.0) endAngle -= 360.0;
                    
                    // Calculate path direction vector (from held to adjacent)
                    double pathX = std::sin(endAngle * PI / 180.0) - std::sin(heldAngle * PI / 180.0);
                    double pathY = std::cos(endAngle * PI / 180.0) - std::cos(heldAngle * PI / 180.0);
                    
                    // Normalize the path vector
                    double pathLength = std::sqrt(pathX * pathX + pathY * pathY);
                    if (pathLength > 0) {
                        pathX /= pathLength;
                        pathY /= pathLength;
                    }
                    
                    // Calculate locked position along the path segment (from held to adjacent)
                    // Use center of direction arc instead of captured position
                    double heldX, heldY;
                    getDirectionArcCenter(currentRHeldDirection, heldX, heldY);
                    double rawDx = rightX - heldX;
                    double rawDy = rightY - heldY;
                    double t = rawDx * pathX + rawDy * pathY; // projection length along unit path
                    // Clamp t to [0, pathLength] to stay within the segment
                    if (t < 0.0) t = 0.0;
                    if (t > pathLength) t = pathLength;
                    lockedX = heldX + t * pathX;
                    lockedY = heldY + t * pathY;
                    sendTouch(1, lockedX, lockedY, false, false); // Touch move/update with locked position
                } else {
                    // No locking - use current position
                    if (rightPointerLocked) {
                        rightPointerLocked = false;
                        rightLockedDirection = -1;
                    }
                    sendTouch(1, rightX, rightY, false, false); // Touch move/update
                }
            } else {
                // No trigger pressed - use current position
                if (rightPointerLocked) {
                    rightPointerLocked = false;
                    rightLockedDirection = -1;
                    std::cout << "Right pointer UNLOCKED" << std::endl;
                }
                // Skip individual send if both touches are active (will send both together at end)
                if (!leftTouchActive) {
                    sendTouch(1, rightX, rightY, false, false); // Touch move/update
                }
            }
        }
        
        // Send both touches together if both are active (for true multi-touch support)
        // This ensures both touches can be held independently
        if (leftTouchActive && rightTouchActive) {
            // Calculate positions for both touches, taking locks into account
            double leftSendX = leftX, leftSendY = leftY;
            double rightSendX = rightX, rightSendY = rightY;
            
            // Check if left touch is locked
            if (leftPointerLocked && currentLHeldDirection >= 0) {
                // Calculate locked position
                double heldAngle = (currentLHeldDirection * DEGREES_PER_SECTOR) + 22.5;
                if (heldAngle >= 360.0) heldAngle -= 360.0;
                double endAngle = (leftLockedDirection * DEGREES_PER_SECTOR) + 22.5;
                if (endAngle >= 360.0) endAngle -= 360.0;
                
                double pathX = std::sin(endAngle * PI / 180.0) - std::sin(heldAngle * PI / 180.0);
                double pathY = std::cos(endAngle * PI / 180.0) - std::cos(heldAngle * PI / 180.0);
                double pathLength = std::sqrt(pathX * pathX + pathY * pathY);
                if (pathLength > 0) {
                    pathX /= pathLength;
                    pathY /= pathLength;
                }
                
                // Use center of direction arc instead of captured position
                double heldX, heldY;
                getDirectionArcCenter(currentLHeldDirection, heldX, heldY);
                double rawDx = leftX - heldX;
                double rawDy = leftY - heldY;
                double t = rawDx * pathX + rawDy * pathY;
                if (t < 0.0) t = 0.0;
                if (t > pathLength) t = pathLength;
                leftSendX = heldX + t * pathX;
                leftSendY = heldY + t * pathY;
            }
            
            // Check if right touch is locked
            if (rightPointerLocked && currentRHeldDirection >= 0) {
                // Calculate locked position
                double heldAngle = (currentRHeldDirection * DEGREES_PER_SECTOR) + 22.5;
                if (heldAngle >= 360.0) heldAngle -= 360.0;
                double endAngle = (rightLockedDirection * DEGREES_PER_SECTOR) + 22.5;
                if (endAngle >= 360.0) endAngle -= 360.0;
                
                double pathX = std::sin(endAngle * PI / 180.0) - std::sin(heldAngle * PI / 180.0);
                double pathY = std::cos(endAngle * PI / 180.0) - std::cos(heldAngle * PI / 180.0);
                double pathLength = std::sqrt(pathX * pathX + pathY * pathY);
                if (pathLength > 0) {
                    pathX /= pathLength;
                    pathY /= pathLength;
                }
                
                // Use center of direction arc instead of captured position
                double heldX, heldY;
                getDirectionArcCenter(currentRHeldDirection, heldX, heldY);
                double rawDx = rightX - heldX;
                double rawDy = rightY - heldY;
                double t = rawDx * pathX + rawDy * pathY;
                if (t < 0.0) t = 0.0;
                if (t > pathLength) t = pathLength;
                rightSendX = heldX + t * pathX;
                rightSendY = heldY + t * pathY;
            }
            
            // Send both touches together
            sendBothTouchesIfActive(leftSendX, leftSendY, rightSendX, rightSendY,
                                    leftSendX, leftSendY, leftPointerLocked,
                                    rightSendX, rightSendY, rightPointerLocked);
        }
        
        // Update previous button states
        prevLeftBumper = leftBumper;
        prevRightBumper = rightBumper;
        prevLeftTrigger = leftTrigger;
        prevRightTrigger = rightTrigger;
    }

    // ========== Mouse Mode (SendInput) ==========
    
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

    // ========== Keyboard Mode (SendInput) ==========
    
    int getKeyCode(const std::string& key) {
        if (key == "1") return '1';
        if (key == "2") return '2';
        if (key == "3") return '3';
        if (key == "4") return '4';
        if (key == "5") return '5';
        if (key == "6") return '6';
        if (key == "7") return '7';
        if (key == "8") return '8';
        return 0;
    }

    void simulateKeyPress(int keyCode, bool isDown) {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = keyCode;
        input.ki.dwFlags = isDown ? 0 : KEYEVENTF_KEYUP;
        input.ki.wScan = MapVirtualKey(keyCode, MAPVK_VK_TO_VSC);
        input.ki.dwExtraInfo = GetMessageExtraInfo();
        SendInput(1, &input, sizeof(INPUT));
    }

    void sendKeyPress(const std::string& key, bool isDown) {
        int keyCode = getKeyCode(key);
        if (keyCode == 0) return;
        simulateKeyPress(keyCode, isDown);
    }

    // ========== Cleanup (Release All Inputs) ==========
    
    void cleanup() {
        // Release all keyboard keys (for Keyboard mode)
        if (!currentLeftKey.empty()) {
            sendKeyPress(currentLeftKey, false);
            currentLeftKey = "";
        }
        if (!currentRightKey.empty()) {
            sendKeyPress(currentRightKey, false);
            currentRightKey = "";
        }
        
        // Release all touch points
        if (leftTouchActive) {
            sendTouch(0, 0, 0, false, true); // Send touch up
            leftTouchActive = false;
        }
        if (rightTouchActive) {
            sendTouch(1, 0, 0, false, true); // Send touch up
            rightTouchActive = false;
        }
        
        // Release mouse button
        if (mouseButtonPressed) {
            sendMouseButton(false);
            mouseButtonPressed = false;
        }
    }

    void handleKeyboardControl(bool leftBumper, bool rightBumper, double leftX, double leftY, double rightX, double rightY) {
        // Calculate angles and directions
        double lAngle = calculateAngle(leftX, leftY);
        double rAngle = calculateAngle(rightX, rightY);
        int lDirection = getDirection(lAngle);
        int rDirection = getDirection(rAngle);
        
        // Handle LEFT bumper + left stick
        if (leftBumper && lDirection != -1) {
            std::string newKey = std::to_string(lDirection + 1);
            
            // Check for conflict with right stick key
            if (newKey == currentRightKey && !currentRightKey.empty()) {
                // Conflict - release our key if we have one
                if (!currentLeftKey.empty()) {
                    sendKeyPress(currentLeftKey, false);
                    currentLeftKey = "";
                }
                return;
            }
            
            // Handle key change
            if (currentLeftKey != newKey) {
                if (!currentLeftKey.empty()) {
                    sendKeyPress(currentLeftKey, false); // Release old
                }
                currentLeftKey = newKey;
                sendKeyPress(currentLeftKey, true); // Press new
            }
        } else if (!currentLeftKey.empty()) {
            // Bumper released or no direction - release key
            sendKeyPress(currentLeftKey, false);
            currentLeftKey = "";
        }
        
        // Handle RIGHT bumper + right stick
        if (rightBumper && rDirection != -1) {
            std::string newKey = std::to_string(rDirection + 1);
            
            // Check for conflict with left stick key
            if (newKey == currentLeftKey && !currentLeftKey.empty()) {
                // Conflict - release our key if we have one
                if (!currentRightKey.empty()) {
                    sendKeyPress(currentRightKey, false);
                    currentRightKey = "";
                }
                return;
            }
            
            // Handle key change
            if (currentRightKey != newKey) {
                if (!currentRightKey.empty()) {
                    sendKeyPress(currentRightKey, false); // Release old
                }
                currentRightKey = newKey;
                sendKeyPress(currentRightKey, true); // Press new
            }
        } else if (!currentRightKey.empty()) {
            // Bumper released or no direction - release key
            sendKeyPress(currentRightKey, false);
            currentRightKey = "";
        }
    }

    // ========== Overlay Rendering ==========
    
    void updateOverlay(double leftX, double leftY, double rightX, double rightY, double leftAngle, double rightAngle) {
        overlayLeftX = leftX;
        overlayLeftY = leftY;
        overlayRightX = rightX;
        overlayRightY = rightY;
        overlayLeftAngle = leftAngle;
        overlayRightAngle = rightAngle;
        
        // Calculate distance from center for left stick
        double leftDistance = std::sqrt(leftX * leftX + leftY * leftY);
        // Left stick alpha: full opacity when bumper OR trigger pressed, fade when neither pressed
        if (leftTouchActive || leftPointerLocked) {
            overlayLeftAlpha = 255; // Full opacity when bumper or trigger is pressed
        } else {
            // Map 0.0-0.5 distance to 0-255 alpha (max opacity at 50% from center)
            if (leftDistance >= 0.5) {
                overlayLeftAlpha = 255;
            } else {
                overlayLeftAlpha = (int)((leftDistance / 0.5) * 255.0);
            }
        }
        
        // Calculate distance from center for right stick
        double rightDistance = std::sqrt(rightX * rightX + rightY * rightY);
        // Right stick alpha: full opacity when bumper OR trigger pressed, fade when neither pressed
        if (rightTouchActive || rightPointerLocked) {
            overlayRightAlpha = 255; // Full opacity when bumper or trigger is pressed
        } else {
            // Map 0.0-0.5 distance to 0-255 alpha (max opacity at 50% from center)
            if (rightDistance >= 0.5) {
                overlayRightAlpha = 255;
            } else {
                overlayRightAlpha = (int)((rightDistance / 0.5) * 255.0);
            }
        }
        
        // Calculate active touch pointer positions
        if (leftTouchActive) {
            if (leftPointerLocked && currentLHeldDirection >= 0) {
                // Use path projection for visual representation - project raw stick onto path from held to adjacent
                // Calculate the path vector from held direction to the chosen adjacent direction
                double heldAngle = (currentLHeldDirection * DEGREES_PER_SECTOR) + 22.5;
                if (heldAngle >= 360.0) heldAngle -= 360.0;
                
                double endAngle = (leftLockedDirection * DEGREES_PER_SECTOR) + 22.5;
                if (endAngle >= 360.0) endAngle -= 360.0;
                
                // Calculate path direction vector (from held to adjacent)
                double pathX = std::sin(endAngle * PI / 180.0) - std::sin(heldAngle * PI / 180.0);
                double pathY = std::cos(endAngle * PI / 180.0) - std::cos(heldAngle * PI / 180.0);
                
                // Normalize the path vector
                double pathLength = std::sqrt(pathX * pathX + pathY * pathY);
                if (pathLength > 0) {
                    pathX /= pathLength;
                    pathY /= pathLength;
                }
                
                // Calculate locked position along the path segment (from held to adjacent)
                // Use center of direction arc instead of captured position
                double heldX, heldY;
                getDirectionArcCenter(currentLHeldDirection, heldX, heldY);
                double rawDx = leftX - heldX;
                double rawDy = leftY - heldY;
                double t = rawDx * pathX + rawDy * pathY; // projection length along unit path
                // Clamp t to [0, pathLength] to stay within the segment
                if (t < 0.0) t = 0.0;
                if (t > pathLength) t = pathLength;
                overlayLeftLockedX = heldX + t * pathX;
                overlayLeftLockedY = heldY + t * pathY;
            } else {
                // Bumper pressed or trigger without locking - show current stick position
                overlayLeftLockedX = leftX;
                overlayLeftLockedY = leftY;
            }
            overlayLeftLockedAlpha = 255; // Full opacity for active touch pointer
        } else {
            overlayLeftLockedAlpha = 0; // Hide when not active
        }
        
        if (rightTouchActive) {
            if (rightPointerLocked && currentRHeldDirection >= 0) {
                // Use path projection for visual representation - project raw stick onto path from held to adjacent
                // Calculate the path vector from held direction to the chosen adjacent direction
                double heldAngle = (currentRHeldDirection * DEGREES_PER_SECTOR) + 22.5;
                if (heldAngle >= 360.0) heldAngle -= 360.0;
                
                double endAngle = (rightLockedDirection * DEGREES_PER_SECTOR) + 22.5;
                if (endAngle >= 360.0) endAngle -= 360.0;
                
                // Calculate path direction vector (from held to adjacent)
                double pathX = std::sin(endAngle * PI / 180.0) - std::sin(heldAngle * PI / 180.0);
                double pathY = std::cos(endAngle * PI / 180.0) - std::cos(heldAngle * PI / 180.0);
                
                // Normalize the path vector
                double pathLength = std::sqrt(pathX * pathX + pathY * pathY);
                if (pathLength > 0) {
                    pathX /= pathLength;
                    pathY /= pathLength;
                }
                
                // Calculate locked position along the path segment (from held to adjacent)
                // Use center of direction arc instead of captured position
                double heldX, heldY;
                getDirectionArcCenter(currentRHeldDirection, heldX, heldY);
                double rawDx = rightX - heldX;
                double rawDy = rightY - heldY;
                double t = rawDx * pathX + rawDy * pathY; // projection length along unit path
                // Clamp t to [0, pathLength] to stay within the segment
                if (t < 0.0) t = 0.0;
                if (t > pathLength) t = pathLength;
                overlayRightLockedX = heldX + t * pathX;
                overlayRightLockedY = heldY + t * pathY;
            } else {
                // Bumper pressed or trigger without locking - show current stick position
                overlayRightLockedX = rightX;
                overlayRightLockedY = rightY;
            }
            overlayRightLockedAlpha = 255; // Full opacity for active touch pointer
        } else {
            overlayRightLockedAlpha = 0; // Hide when not active
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
                    // Don't call PostQuitMessage here - we handle it explicitly
                    // Otherwise restart causes double WM_QUIT
                    return 0;
            }
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    // ========== Controller Initialization ==========
    
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

        // Set data format - use DIJOYSTATE2 for extended axes (includes sliders for DS4 touchpad)
        hr = joystick->SetDataFormat(&c_dfDIJoystick2);
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

        // Set data format - use DIJOYSTATE2 for extended axes (includes sliders for DS4 touchpad)
        hr = joystick->SetDataFormat(&c_dfDIJoystick2);
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

    // ========== Utility Functions ==========
    
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

    /**
     * Get the opposite direction (180° apart)
     * @param direction Direction sector (0-7)
     * @return Opposite direction sector (0-7)
     */
    int getOppositeDirection(int direction) {
        if (direction < 0 || direction >= DIRECTION_SECTORS) return -1;
        return (direction + 4) % DIRECTION_SECTORS;
    }

    /**
     * Calculate angle difference between two directions
     * @param dir1 First direction (0-7)
     * @param dir2 Second direction (0-7)
     * @return Angle difference in degrees (0-180)
     */
    double getDirectionAngleDifference(int dir1, int dir2) {
        if (dir1 < 0 || dir2 < 0) return 180.0;
        
        double angle1 = dir1 * DEGREES_PER_SECTOR;
        double angle2 = dir2 * DEGREES_PER_SECTOR;
        
        double diff = std::abs(angle1 - angle2);
        if (diff > 180.0) {
            diff = 360.0 - diff;
        }
        return diff;
    }

    /**
     * Get adjacent directions for a given direction
     * @param direction Base direction (0-7)
     * @param leftAdjacent Reference to store left adjacent direction
     * @param rightAdjacent Reference to store right adjacent direction
     */
    void getAdjacentDirections(int direction, int& leftAdjacent, int& rightAdjacent) {
        if (direction < 0 || direction >= DIRECTION_SECTORS) {
            leftAdjacent = -1;
            rightAdjacent = -1;
            return;
        }
        
        leftAdjacent = (direction - 1 + DIRECTION_SECTORS) % DIRECTION_SECTORS;
        rightAdjacent = (direction + 1) % DIRECTION_SECTORS;
    }

    /**
     * Get the center position of a direction arc
     * @param direction The direction (0-7)
     * @param centerX Reference to store the X coordinate (-1.0 to 1.0)
     * @param centerY Reference to store the Y coordinate (-1.0 to 1.0)
     */
    void getDirectionArcCenter(int direction, double& centerX, double& centerY) {
        if (direction < 0 || direction >= DIRECTION_SECTORS) {
            centerX = 0.0;
            centerY = 0.0;
            return;
        }
        
        // Calculate the center angle of this direction
        double centerAngle = (direction * DEGREES_PER_SECTOR) + 22.5;
        if (centerAngle >= 360.0) centerAngle -= 360.0;
        
        // Convert to radians and calculate position at full radius (1.0)
        double angleRad = centerAngle * PI / 180.0;
        centerX = std::sin(angleRad);  // X = sin(angle)
        centerY = std::cos(angleRad);  // Y = cos(angle)
    }

    /**
     * Check if pointer should be locked based on stick movement
     * @param heldDirection The direction when bumper was first pressed
     * @param currentDirection Current stick direction
     * @param currentAngle Current stick angle
     * @param lockedDirection Reference to store the direction to lock to
     * @return true if pointer should be locked
     */
    bool checkPointerLock(int heldDirection, int currentDirection, double currentAngle, int& lockedDirection) {
        if (heldDirection < 0 || currentDirection < 0) return false;
        
        // Path locking: lock to the straight line path between held direction and its adjacent opposites
        // The path starts from the held direction and goes to the adjacent directions of its opposite
        // Direction 0 -> opposite 4 -> adjacents 3,5 (path: 0->3 or 0->5)
        // Direction 1 -> opposite 5 -> adjacents 4,6 (path: 1->4 or 1->6)
        // Direction 2 -> opposite 6 -> adjacents 5,7 (path: 2->5 or 2->7)
        // Direction 3 -> opposite 7 -> adjacents 6,0 (path: 3->6 or 3->0)
        // Direction 4 -> opposite 0 -> adjacents 7,1 (path: 4->7 or 4->1)
        // Direction 5 -> opposite 1 -> adjacents 0,2 (path: 5->0 or 5->2)
        // Direction 6 -> opposite 2 -> adjacents 1,3 (path: 6->1 or 6->3)
        // Direction 7 -> opposite 3 -> adjacents 2,4 (path: 7->2 or 7->4)
        
        int oppositeDirection = (heldDirection + 4) % 8;
        int leftAdjacent = (oppositeDirection - 1 + 8) % 8;
        int rightAdjacent = (oppositeDirection + 1) % 8;
        
        // Calculate angles for the held direction and the two adjacent directions
        double heldAngle = (heldDirection * DEGREES_PER_SECTOR) + 22.5;
        if (heldAngle >= 360.0) heldAngle -= 360.0;
        
        double leftAngle = (leftAdjacent * DEGREES_PER_SECTOR) + 22.5;
        if (leftAngle >= 360.0) leftAngle -= 360.0;
        
        double rightAngle = (rightAdjacent * DEGREES_PER_SECTOR) + 22.5;
        if (rightAngle >= 360.0) rightAngle -= 360.0;
        
        // Determine which adjacent direction the current angle is closer to
        double angleDiffToLeft = std::abs(currentAngle - leftAngle);
        if (angleDiffToLeft > 180.0) angleDiffToLeft = 360.0 - angleDiffToLeft;
        
        double angleDiffToRight = std::abs(currentAngle - rightAngle);
        if (angleDiffToRight > 180.0) angleDiffToRight = 360.0 - angleDiffToRight;
        
        // Choose the closer adjacent direction as the end point
        // The path will be from heldDirection to the chosen adjacent direction
        if (angleDiffToLeft < angleDiffToRight) {
            lockedDirection = leftAdjacent;
        } else {
            lockedDirection = rightAdjacent;
        }
        
        return true;
    }

    // ========== Debug Info Display ==========
    
    void updateDebugInfo(double lAngle, double rAngle, int lDirection, int rDirection, const DIJOYSTATE2* diState = nullptr) {
        // Build debug info string based on current mode
        // This is displayed on the overlay (bottom-left, large text)
        
        std::string info = "CONTROLLER INPUT MAPPER\r\n";
        info += std::string(hasXInputController ? "XInput" : "DirectInput") + " | ";
        
        // Show current mode
        switch (currentMode) {
            case InputMode::Touch:
                info += "Touch Mode\r\n";
                break;
            case InputMode::Mouse:
                info += "Mouse Mode\r\n";
                break;
            case InputMode::Keyboard:
                info += "Keyboard Mode\r\n";
                break;
        }
        info += "\r\n";
        
        // Mode-specific status
        if (currentMode == InputMode::Touch) {
            info += "TOUCH STATUS:\r\n";
            info += "  Touch 0 (LB + L Stick): " + std::string(leftTouchActive ? "ACTIVE" : "---") + "\r\n";
            if (leftTouchActive) {
                // Get screen coordinates
                LONG touchX, touchY;
                getTouchCoordinates(overlayLeftX, overlayLeftY, touchX, touchY);
                info += "    Screen: (" + std::to_string(touchX) + ", " + std::to_string(touchY) + ")\r\n";
                // Show held direction and lock status
                if (currentLHeldDirection >= 0) {
                    info += "    Held Dir: " + std::to_string(currentLHeldDirection) + "\r\n";
                }
                if (leftPointerLocked) {
                    info += "    LOCKED to: " + std::to_string(leftLockedDirection) + "\r\n";
                }
            }
            info += "  Touch 1 (RB + R Stick): " + std::string(rightTouchActive ? "ACTIVE" : "---") + "\r\n";
            if (rightTouchActive) {
                LONG touchX, touchY;
                getTouchCoordinates(overlayRightX, overlayRightY, touchX, touchY);
                info += "    Screen: (" + std::to_string(touchX) + ", " + std::to_string(touchY) + ")\r\n";
                // Show held direction and lock status
                if (currentRHeldDirection >= 0) {
                    info += "    Held Dir: " + std::to_string(currentRHeldDirection) + "\r\n";
                }
                if (rightPointerLocked) {
                    info += "    LOCKED to: " + std::to_string(rightLockedDirection) + "\r\n";
                }
            }
            info += "\r\n";
            
            // Stick positions
            info += "STICK POSITIONS:\r\n";
            char buf[64];
            sprintf_s(buf, "  Left:  X=%.2f Y=%.2f\r\n", overlayLeftX, overlayLeftY);
            info += buf;
            sprintf_s(buf, "  Right: X=%.2f Y=%.2f\r\n", overlayRightX, overlayRightY);
            info += buf;
            info += "\r\n";
            
            // Current angles and directions
            info += "CURRENT DIRECTIONS:\r\n";
            if (lAngle >= 0) {
                sprintf_s(buf, "  Left:  %.1f° (Dir %d)\r\n", lAngle, lDirection);
                info += buf;
            } else {
                info += "  Left:  ---\r\n";
            }
            if (rAngle >= 0) {
                sprintf_s(buf, "  Right: %.1f° (Dir %d)\r\n", rAngle, rDirection);
                info += buf;
            } else {
                info += "  Right: ---\r\n";
            }
            info += "\r\n";
            

            // Pointer locking status
            info += "POINTER LOCKING:\r\n";
            info += "  Left Lock: " + std::string(leftPointerLocked ? "ACTIVE" : "---") + "\r\n";
            if (leftPointerLocked) {
                info += "    Locked to: " + std::to_string(leftLockedDirection) + "\r\n";
            }
            if (currentLHeldDirection >= 0) {
                info += "    Captured: " + std::to_string(currentLHeldDirection) + "\r\n";
            }
            info += "  Right Lock: " + std::string(rightPointerLocked ? "ACTIVE" : "---") + "\r\n";
            if (rightPointerLocked) {
                info += "    Locked to: " + std::to_string(rightLockedDirection) + "\r\n";
            }
            if (currentRHeldDirection >= 0) {
                info += "    Captured: " + std::to_string(currentRHeldDirection) + "\r\n";
            }
            info += "\r\n";
        } else if (currentMode == InputMode::Mouse) {
            info += "MOUSE CONTROL:\r\n";
            bool leftPressed = prevLeftBumper;
            bool rightPressed = prevRightBumper;
            bool bothPressed = leftPressed && rightPressed;
            
            // Get current mouse position
            POINT mousePos;
            GetCursorPos(&mousePos);
            info += "  Cursor: (" + std::to_string(mousePos.x) + ", " + std::to_string(mousePos.y) + ")\r\n";
            
            if (bothPressed) {
                info += "  Mode: Alternating sticks\r\n";
                info += "  Current: " + std::string(alternateFrame ? "Left" : "Right") + "\r\n";
            } else if (leftPressed) {
                info += "  Mode: Left stick\r\n";
            } else if (rightPressed) {
                info += "  Mode: Right stick\r\n";
            } else {
                info += "  Mode: Inactive\r\n";
            }
            info += "\r\n";
            
            // Stick positions
            info += "STICK POSITIONS:\r\n";
            char buf[64];
            sprintf_s(buf, "  Left:  X=%.2f Y=%.2f\r\n", overlayLeftX, overlayLeftY);
            info += buf;
            sprintf_s(buf, "  Right: X=%.2f Y=%.2f\r\n", overlayRightX, overlayRightY);
            info += buf;
            info += "\r\n";
        } else if (currentMode == InputMode::Keyboard) {
            info += "KEYBOARD (1-8):\r\n";
            info += "  L Stick Key: " + (currentLeftKey.empty() ? std::string("---") : currentLeftKey) + "\r\n";
            info += "  R Stick Key: " + (currentRightKey.empty() ? std::string("---") : currentRightKey) + "\r\n";
            info += "\r\n";
            
            // Show stick details for keyboard mode
            info += "STICK POSITIONS:\r\n";
            char buf[64];
            sprintf_s(buf, "  Left:  X=%.2f Y=%.2f\r\n", overlayLeftX, overlayLeftY);
            info += buf;
            sprintf_s(buf, "  Right: X=%.2f Y=%.2f\r\n", overlayRightX, overlayRightY);
            info += buf;
            info += "\r\n";
            
            info += "ANGLES:\r\n";
            if (lAngle >= 0) {
                sprintf_s(buf, "  Left:  %.1f°\r\n", lAngle);
                info += buf;
            } else {
                info += "  Left:  ---\r\n";
            }
            if (rAngle >= 0) {
                sprintf_s(buf, "  Right: %.1f°\r\n", rAngle);
                info += buf;
            } else {
                info += "  Right: ---\r\n";
            }
            info += "\r\n";
            
            info += "DIRECTIONS:\r\n";
            info += "  Left:  " + (lDirection >= 0 ? std::to_string(lDirection + 1) : std::string("---")) + "\r\n";
            info += "  Right: " + (rDirection >= 0 ? std::to_string(rDirection + 1) : std::string("---")) + "\r\n";
            info += "\r\n";
        }
        
        info += "Ctrl+Shift+` = Toggle | Ctrl+Alt+Shift+` = Restart\r\n";

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
    
public:
    // ========== Main Application Loop ==========
    
    /**
     * Main application loop - processes controller input and updates GUI
     * Handles both XInput and DirectInput controllers based on selected mode
     * 
     * Modes:
     *   - Touch: LB/RB + sticks = Touch points 0/1
     *   - Mouse: Bumpers + sticks = Cursor control + LMB
     *   - Keyboard: LB/RB + sticks = Number keys 1-8
     * 
     * Keyboard shortcuts:
     *   - Ctrl+Shift+` = Toggle debug UI
     *   - Ctrl+Alt+Shift+` = Restart (return to mode selection)
     */
    void run() {
        if (!hwnd || (!joystick && !hasXInputController)) {
            std::cerr << "Not initialized!" << std::endl;
            return;
        }
        
        // Keep console visible throughout the process
        
        // Clear keyboard state completely to prevent restart flag persistence
        for (int i = 0; i < 10; i++) {
            GetAsyncKeyState(VK_CONTROL);
            GetAsyncKeyState(VK_SHIFT);
            GetAsyncKeyState(VK_MENU);
            GetAsyncKeyState(VK_OEM_3);
            Sleep(20);
        }
        
        // Wait for physical keys to be released
        int waitCount = 0;
        while (waitCount < 100) { // Max 5 seconds
            bool anyKeyHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) || 
                              (GetAsyncKeyState(VK_SHIFT) & 0x8000) || 
                              (GetAsyncKeyState(VK_MENU) & 0x8000) || 
                              (GetAsyncKeyState(VK_OEM_3) & 0x8000);
            
            if (!anyKeyHeld) break;
            
            Sleep(50);
            waitCount++;
        }
        
        // Additional delay for safety
        Sleep(200);
        
        // Clear ALL leftover messages from previous instance
        MSG clearMsg;
        while (PeekMessage(&clearMsg, nullptr, 0, 0, PM_REMOVE)) {
            // Silently discard all leftover messages
        }

        // Check current keyboard state and initialize prev states to match
        bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        bool backtickDown = (GetAsyncKeyState(VK_OEM_3) & 0x8000) != 0;
        
        bool togglePressed = ctrlDown && shiftDown && !altDown && backtickDown;
        bool restartPressed = ctrlDown && shiftDown && altDown && backtickDown;
        
        // Initialize prev states to CURRENT state to prevent first-frame trigger
        bool prevTogglePressed = togglePressed;
        bool prevRestartPressed = restartPressed;

        MSG msg = {};
        while (true) {
            // Process messages (non-blocking)
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (msg.message == WM_QUIT) {
                    cleanup(); // Release all inputs before exiting
                    return; // Return immediately from run() method
                }
            }

            // Check keyboard shortcuts
            ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            backtickDown = (GetAsyncKeyState(VK_OEM_3) & 0x8000) != 0;
            
            togglePressed = ctrlDown && shiftDown && !altDown && backtickDown;
            restartPressed = ctrlDown && shiftDown && altDown && backtickDown;
            
            // Toggle debug on key press (not hold)
            if (togglePressed && !prevTogglePressed) {
                showDebugInfo = !showDebugInfo;
                std::cout << "Debug info " << (showDebugInfo ? "enabled" : "disabled") << std::endl;
                if (overlayHwnd) {
                    RedrawWindow(overlayHwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOFRAME);
                }
            }
            
            // Restart program on key press (not hold)
            if (restartPressed && !prevRestartPressed) {
                std::cout << "Restarting..." << std::endl;
                cleanup(); // Release all inputs before restarting
                PostQuitMessage(0);
                return;
            }
            
            prevTogglePressed = togglePressed;
            prevRestartPressed = restartPressed;

            // Process controller (always, regardless of focus)
            bool controllerSuccess = false;
            bool leftButtonPressed = false;
            bool rightButtonPressed = false;
            bool leftTriggerPressed = false;
            bool rightTriggerPressed = false;
            double joyX = 0, joyY = 0, joyZ = 0, joyR = 0;
            
            if (hasXInputController) {
                // Process XInput controller (Xbox controllers)
                DWORD result = XInputGetState(xInputControllerIndex, &xInputState);
                if (result == ERROR_SUCCESS) {
                    controllerSuccess = true;
                    
                    // XInput button mapping: Left Shoulder = LB, Right Shoulder = RB
                    leftButtonPressed = (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
                    rightButtonPressed = (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
                    
                    // XInput stick click mapping: L3 = Left stick click, R3 = Right stick click
                    leftTriggerPressed = (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
                    rightTriggerPressed = (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
                    
                    // XInput stick values: -32768 to 32767, normalize to -1.0 to 1.0
                    // For XInput, we need to match DirectInput coordinate system
                    joyX = xInputState.Gamepad.sThumbLX / STICK_MAX_VALUE;   // Left stick X
                    joyY = xInputState.Gamepad.sThumbLY / STICK_MAX_VALUE;   // Left stick Y (not inverted for XInput)
                    joyZ = xInputState.Gamepad.sThumbRX / STICK_MAX_VALUE;   // Right stick X
                    joyR = xInputState.Gamepad.sThumbRY / STICK_MAX_VALUE;   // Right stick Y (not inverted for XInput)
                }
            } else if (joystick) {
                // Process DirectInput controller - use DIJOYSTATE2 for extended axes
            DIJOYSTATE2 state;
            HRESULT hr = joystick->GetDeviceState(sizeof(DIJOYSTATE2), &state);
            
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
                    
                    // DirectInput stick click mapping - use L3 and R3 buttons
                    // Left stick click: L3 button (button 10)
                    leftTriggerPressed = (state.rgbButtons[10] & 0x80) != 0; // L3 button
                    // Right stick click: R3 button (button 11)
                    rightTriggerPressed = (state.rgbButtons[11] & 0x80) != 0; // R3 button
                    
                    
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

                // Handle input based on current mode
                switch (currentMode) {
                    case InputMode::Touch:
                        handleTouchControl(leftButtonPressed, rightButtonPressed, leftTriggerPressed, rightTriggerPressed, joyX, joyY, joyZ, joyR);
                        break;
                    case InputMode::Mouse:
                        handleMouseControl(leftButtonPressed, rightButtonPressed, joyX, joyY, joyZ, joyR);
                        break;
                    case InputMode::Keyboard:
                        handleKeyboardControl(leftButtonPressed, rightButtonPressed, joyX, joyY, joyZ, joyR);
                        break;
                }
                
                // Update overlay with stick positions and angles
                updateOverlay(joyX, joyY, joyZ, joyR, lAngle, rAngle);

                // Update debug info (only if visible for efficiency)
                if (showDebugInfo) {
                    if (hasXInputController) {
                        updateDebugInfo(lAngle, rAngle, lDirection, rDirection);
                    } else if (joystick) {
                        // For DirectInput, we need to get the state again for debug info
                        DIJOYSTATE2 debugState;
                        HRESULT debugHr = joystick->GetDeviceState(sizeof(DIJOYSTATE2), &debugState);
                        if (SUCCEEDED(debugHr)) {
                            updateDebugInfo(lAngle, rAngle, lDirection, rDirection, &debugState);
                        } else {
                            updateDebugInfo(lAngle, rAngle, lDirection, rDirection);
                        }
                    } else {
                        updateDebugInfo(lAngle, rAngle, lDirection, rDirection);
                    }
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

// ============================================
// MAIN ENTRY POINT
// ============================================

int main() {
    // Allocate console for debug output
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
    
    // Main loop: Show mode selection → Run app → On restart, loop back
    while (true) {
        // ========== Mode Selection Menu ==========
        std::cout << "========================================" << std::endl;
        std::cout << "    CONTROLLER INPUT MAPPER" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;
        std::cout << "Choose input mode:" << std::endl;
        std::cout << std::endl;
        std::cout << "  [1] Touch Mode (Multi-touch for Sentakki)" << std::endl;
        std::cout << "      - 2 independent touch points" << std::endl;
        std::cout << "      - LB + Left Stick = Touch 0" << std::endl;
        std::cout << "      - RB + Right Stick = Touch 1" << std::endl;
        std::cout << std::endl;
        std::cout << "  [2] Mouse Mode (Cursor + Click)" << std::endl;
        std::cout << "      - Right stick = mouse cursor" << std::endl;
        std::cout << "      - Bumpers = left click" << std::endl;
        std::cout << std::endl;
        std::cout << "  [3] Keyboard Mode (Number keys 1-8)" << std::endl;
        std::cout << "      - Both sticks = directional keys 1-8" << std::endl;
        std::cout << "      - Perfect for lane-based rhythm games" << std::endl;
        std::cout << std::endl;
        std::cout << "Select mode (1-3): ";
        
        InputMode selectedMode = InputMode::Touch; // Default
        char choice = _getch();
        std::cout << choice << std::endl << std::endl;
        
        switch (choice) {
            case '1':
                selectedMode = InputMode::Touch;
                std::cout << "Starting in TOUCH mode..." << std::endl;
                break;
            case '2':
                selectedMode = InputMode::Mouse;
                std::cout << "Starting in MOUSE mode..." << std::endl;
                break;
            case '3':
                selectedMode = InputMode::Keyboard;
                std::cout << "Starting in KEYBOARD mode..." << std::endl;
                break;
            default:
                std::cout << "Invalid choice. Please select 1, 2, or 3." << std::endl;
                std::cout << std::endl;
                continue; // Go back to mode selection
        }
        
        std::cout << "Close the program by closing the console" << std::endl;
        std::cout << "Ctrl+Shift+` = Toggle debug info | Ctrl+Alt+Shift+` = Restart" << std::endl;
        std::cout << std::endl;

    try {
        SimpleController app(selectedMode);
            if (!app.initialize()) {
                std::cerr << "[ERROR] Failed to initialize application!" << std::endl;
                continue;
            }
            app.run();
            
            // If we get here, the window was closed (restart requested)
            // Loop back to mode selection menu
            
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
            std::cerr << "Press any key to continue or Ctrl+C to exit..." << std::endl;
            _getch();
            std::cout << std::endl;
        }
        
        // Loop back to show mode selection menu again
    }

    FreeConsole();
    return 0;
}
