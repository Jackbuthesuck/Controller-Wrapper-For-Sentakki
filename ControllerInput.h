#ifndef CONTROLLER_INPUT_H
#define CONTROLLER_INPUT_H

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

class ControllerMapper {
private:
    // ========== GUI Components ==========
    LPDIRECTINPUT8 di;
    LPDIRECTINPUTDEVICE8 joystick;
    HWND hwnd;              // Main window (hidden)
    HWND overlayHwnd;       // Full-screen transparent overlay
    std::string debugText;  // Debug info displayed on overlay
    POINT lastMousePos;      // Last mouse position for detecting movement
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
    
    // Monitor information (detected from console window)
    int monitorLeft, monitorTop;           // Monitor position in virtual screen
    int monitorWidth, monitorHeight;       // Monitor dimensions
    HMONITOR monitorHandle;                // Monitor handle
    
    // Primary monitor information (for touch coordinate conversion)
    int primaryMonitorLeft, primaryMonitorTop;  // Primary monitor position
    
    // Real-time monitor detection - cache monitor bounds for quick checking
    int monitorRight, monitorBottom;  // Cached monitor bounds for quick boundary checks
    
    // Locked pointer visualization
    double overlayLeftLockedX, overlayLeftLockedY;    // Left locked position
    double overlayRightLockedX, overlayRightLockedY;  // Right locked position
    int overlayLeftLockedAlpha, overlayRightLockedAlpha; // Locked pointer alpha
    
    // L3/R3 5-touch X pattern visualization
    double overlayL3CenterX, overlayL3CenterY;      // L3 center position
    double overlayR3CenterX, overlayR3CenterY;      // R3 center position
    int overlayL3Alpha, overlayR3Alpha;             // L3/R3 pattern alpha
    
    // Track previous overlay values to prevent unnecessary redraws
    double prevOverlayLeftX, prevOverlayLeftY, prevOverlayRightX, prevOverlayRightY;
    double prevOverlayLeftAngle, prevOverlayRightAngle;
    int prevOverlayLeftAlpha, prevOverlayRightAlpha;
    int prevOverlayLeftLockedAlpha, prevOverlayRightLockedAlpha;
    double prevOverlayLeftLockedX, prevOverlayLeftLockedY;
    double prevOverlayRightLockedX, prevOverlayRightLockedY;
    double prevOverlayL3CenterX, prevOverlayL3CenterY;
    double prevOverlayR3CenterX, prevOverlayR3CenterY;
    int prevOverlayL3Alpha, prevOverlayR3Alpha;
    
    // ========== Input Mode State ==========
    InputMode currentMode;  // Current operating mode (Touch/Mouse/Keyboard)
    
    // Touch mode state (UWP InputInjector)
    bool leftTouchActive;
    bool rightTouchActive;
    InputInjector inputInjector;
    bool inputInjectorInitialized;
    
    // Touch mode section tracking
    int currentLHeldDirection;  // Direction held when L1 is pressed
    int currentRHeldDirection;  // Direction held when R1 is pressed
    double currentLHeldX;       // X position when left direction was captured
    double currentLHeldY;       // Y position when left direction was captured
    double currentRHeldX;       // X position when right direction was captured
    double currentRHeldY;       // Y position when right direction was captured
    
    // Touch mode pointer locking (trigger-based: L2/R2)
    bool leftPointerLocked;     // Whether left touch is locked to a direction
    bool rightPointerLocked;    // Whether right touch is locked to a direction
    int leftLockedDirection;    // Direction the left touch is locked to
    int rightLockedDirection;   // Direction the right touch is locked to
    bool prevL2;  // Previous L2 (left trigger) button state
    bool prevR2;  // Previous R2 (right trigger) button state
    bool prevL3;  // Previous L3 (left stick press) button state
    bool prevR3;  // Previous R3 (right stick press) button state
    
    // L3/R3 5-touch X pattern state
    bool l3TouchActive;  // Whether L3 5-touch pattern is active
    bool r3TouchActive;  // Whether R3 5-touch pattern is active
    
    // All 20 touch tracking (0-19) for debug overlay
    bool touchActive[20];  // Whether each touch is active
    double touchX[20];     // X position of each touch (stick coordinates)
    double touchY[20];     // Y position of each touch (stick coordinates)
    
    // Mouse mode state
    bool mouseButtonPressed;
    bool alternateFrame;  // For dual-stick alternating mode
    
    // Keyboard mode state (number keys 1-8)
    std::string currentLeftKey;
    std::string currentRightKey;
    
    // Shared button tracking
    bool prevL1;  // Previous L1 (left shoulder) button state
    bool prevR1;  // Previous R1 (right shoulder) button state
    
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
    static constexpr int X_PATTERN_RADIUS_PIXELS = 125;  // Pixel radius for 5-touch X pattern (center to corner distance)

public:
    // ========== Constructor & Initialization ==========
    ControllerMapper(InputMode mode = InputMode::Touch);
    bool initialize();
    ~ControllerMapper();
    
    // ========== Main Application Loop ==========
    void run();
    
    // ========== Cleanup (Release All Inputs) ==========
    void cleanup();

private:
    // ========== GUI Creation ==========
    void createGUI();
    void createOverlay();
    void detectMonitorFromCursor(bool verbose = false);
    POINT checkMonitorChange();
    void updateRefreshRate();
    void updateOverlayPosition();
    
    // ========== Controller Initialization ==========
    void initializeControllers();
    std::vector<ControllerInfo> listAllControllers();
    void displayControllerMenu(const std::vector<ControllerInfo>& controllers);
    int getControllerSelection(int maxControllers);
    bool initializeDirectInputWithDevice(const GUID& deviceGuid);
    
    // ========== Overlay Rendering ==========
    void updateOverlay(double leftX, double leftY, double rightX, double rightY, double leftAngle, double rightAngle);
    void drawOverlay(HDC hdc);
    
private:
    // Helper functions for overlay updates
    int calculateAlpha(double distance, bool touchActive, bool pointerLocked);
    void updateTouchPointerPosition(bool touchActive, bool pointerLocked, int heldDirection, int lockedDirection, 
                                   double stickX, double stickY, double& lockedX, double& lockedY, int& alpha);
    void drawDirectionIndicator(HDC hdc, int centerX, int centerY, int direction, COLORREF color, int alpha, int thickness = -1);
    void drawStick(HDC hdc, int centerX, int centerY, double stickX, double stickY, COLORREF color, int alpha);
    void drawTouchPointIndicatorAtOverlayPos(HDC hdc, int overlayX, int overlayY, COLORREF color);
    void drawTouchPointIndicator(HDC hdc, LONG screenX, LONG screenY, COLORREF color);
    void drawLockedPointer(HDC hdc, int centerX, int centerY, double stickX, double stickY, COLORREF color, int alpha);
    void drawPalmTouchPattern(HDC hdc, int centerX, int centerY, double centerStickX, double centerStickY, COLORREF color, int alpha);
    void drawAllTouches(HDC hdc, int centerX, int centerY);
    void drawDebugText(HDC hdc, RECT rect);
    
    // Helper functions for overlay rendering
    void convertStickToOverlayCoords(double stickX, double stickY, int centerX, int centerY, int& overlayX, int& overlayY);
    void drawTouchCircleWithId(HDC hdc, int overlayX, int overlayY, int touchId, COLORREF color, int radius = OVERLAY_LOCKED_INDICATOR_RADIUS);
    
    // ========== Utility Functions ==========
    double calculateAngle(double x, double y);
    int getDirection(double angle);
    void calculateLockedPosition(int heldDirection, int lockedDirection, double currentX, double currentY, double& lockedX, double& lockedY);
    bool checkPointerLock(int heldDirection, int currentDirection, double currentAngle, int& lockedDirection);
    void getDirectionArcCenter(int direction, double& centerX, double& centerY);
    void getAdjacentDirections(int direction, int& leftAdjacent, int& rightAdjacent);
    void updateDebugInfo(double lAngle, double rAngle, int lDirection, int rDirection, DIJOYSTATE2* state = nullptr);
    void logError(const std::string& message);
    void logInfo(const std::string& message);
    
    // ========== Window Procedures ==========
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK OverlayWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // ========== Mode-Specific Handlers (forward declarations) ==========
    // These are implemented in separate files
    void handleTouchControl(bool l1, bool r1, bool l2, bool r2, bool l3, bool r3, double leftX, double leftY, double rightX, double rightY);
    void handleMouseControl(bool l1, bool r1, double leftX, double leftY, double rightX, double rightY);
    void handleKeyboardControl(bool l1, bool r1, double leftX, double leftY, double rightX, double rightY);
    
    // ========== Touch Mode Methods (forward declarations) ==========
    // These are implemented in TouchMode.cpp
    void initializeTouchInjection();
    void getTouchCoordinates(double stickX, double stickY, LONG& touchX, LONG& touchY);
    void pixelToHimetric(LONG pixelX, LONG pixelY, LONG& himetricX, LONG& himetricY);
    InjectedInputTouchInfo createTouchInfo(int touchId, double stickX, double stickY, bool isDown, bool isUp);
    InjectedInputTouchInfo createTouchInfo(int touchId, double stickX, double stickY, bool isDown, bool isUp, int contactRadius);
    void sendMultipleTouches(const std::vector<InjectedInputTouchInfo>& touches);
    void sendPalmTouch(double centerX, double centerY, int centerTouchId, int cornerStartId, bool isDown, bool isUp);
    void sendTouch(int touchId, double stickX, double stickY, bool isDown, bool isUp);
    void sendBothTouchesIfActive(double leftX, double leftY, double rightX, double rightY,
                                 double leftLockedX, double leftLockedY, bool leftLocked,
                                 double rightLockedX, double rightLockedY, bool rightLocked);
    void handleTouchMovementUpdate(int touchId, bool& touchActive, bool bumperPressed, bool stickPressPressed,
                                  int& heldDirection, bool& pointerLocked, int& lockedDirection,
                                  double currentX, double currentY, double angle, int direction,
                                  bool otherTouchActive);
    
    // ========== Mouse Mode Methods (forward declarations) ==========
    // These are implemented in MouseMode.cpp
    void moveMouseToCenter();
    void moveMouseToStickPosition(double stickX, double stickY);
    void sendMouseButton(bool down);
    
    // ========== Keyboard Mode Methods (forward declarations) ==========
    // These are implemented in KeyboardMode.cpp
    int getKeyCode(const std::string& key);
    void simulateKeyPress(int keyCode, bool isDown);
    void sendKeyPress(const std::string& key, bool isDown);
    
    // ========== Static Variables for Debug ==========
    static inline bool g_anyButtonPressed = false;
    static inline bool g_prevAnyButtonPressed = false;
    static inline int g_buttonPressCounter = 0;
    static inline int g_lastPrintedPressCounter = -1;
};

#endif // CONTROLLER_INPUT_H

