#include "ControllerInput.h"

// ========== Constructor & Initialization ==========

ControllerMapper::ControllerMapper(InputMode mode) : di(nullptr), joystick(nullptr), hwnd(nullptr), overlayHwnd(nullptr),
                    hasXInputController(false), xInputControllerIndex(0),
                    overlayLeftX(0.0), overlayLeftY(0.0), overlayRightX(0.0), overlayRightY(0.0),
                    overlayLeftAngle(-1.0), overlayRightAngle(-1.0), overlayStickRadius(150),
                    overlayLeftAlpha(0), overlayRightAlpha(0), updateIntervalMs(16),
                    overlayLeftLockedX(0.0), overlayLeftLockedY(0.0), overlayRightLockedX(0.0), overlayRightLockedY(0.0),
                    overlayLeftLockedAlpha(0), overlayRightLockedAlpha(0),
                    overlayL3CenterX(0.0), overlayL3CenterY(0.0),
                    overlayR3CenterX(0.0), overlayR3CenterY(0.0),
                    overlayL3Alpha(0), overlayR3Alpha(0),
                    prevOverlayLeftX(-999.0), prevOverlayLeftY(-999.0), prevOverlayRightX(-999.0), prevOverlayRightY(-999.0),
                    prevOverlayLeftAngle(-999.0), prevOverlayRightAngle(-999.0),
                    prevOverlayLeftAlpha(-1), prevOverlayRightAlpha(-1),
                    prevOverlayLeftLockedAlpha(-1), prevOverlayRightLockedAlpha(-1),
                    prevOverlayLeftLockedX(-999.0), prevOverlayLeftLockedY(-999.0),
                    prevOverlayRightLockedX(-999.0), prevOverlayRightLockedY(-999.0),
                    prevOverlayL3CenterX(-999.0), prevOverlayL3CenterY(-999.0),
                    prevOverlayR3CenterX(-999.0), prevOverlayR3CenterY(-999.0),
                    prevOverlayL3Alpha(-1), prevOverlayR3Alpha(-1),
                    currentMode(mode), leftTouchActive(false), rightTouchActive(false), 
                    prevL1(false), prevR1(false),
                    overlayPosX(0), overlayPosY(0), inputInjector(nullptr), inputInjectorInitialized(false),
                    currentLHeldDirection(-1), currentRHeldDirection(-1),
                    leftPointerLocked(false), rightPointerLocked(false),
                    leftLockedDirection(-1), rightLockedDirection(-1),
                    prevL2(false), prevR2(false),
                    prevL3(false), prevR3(false),
                    l3TouchActive(false), r3TouchActive(false),
                    mouseButtonPressed(false), alternateFrame(false), currentLeftKey(""), currentRightKey(""),
                    debugText(""), showDebugInfo(true), lastMousePos({-1, -1}) {
    // Don't initialize controllers or create GUI in constructor
    // This will be done in the main loop
    
    // Initialize WinRT
    init_apartment();
}

bool ControllerMapper::initialize() {
    initializeControllers();
    createGUI();
    return (hwnd != nullptr);
}

ControllerMapper::~ControllerMapper() {
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

// ========== GUI Creation ==========

void ControllerMapper::createGUI() {
    // Register window class
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            wc.lpszClassName = "ControllerMapper";
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExA(&wc);

    // Create main window (always on top, no focus stealing)
    hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            "ControllerMapper",
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
    
    // Overlay will be created after controller selection (in initializeControllers)
}

void ControllerMapper::detectMonitorFromCursor(bool verbose) {
    // Use cursor position to detect which monitor is active
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) {
        // Fallback to primary monitor if cursor position can't be retrieved
        if (verbose) {
            std::cout << "Failed to get cursor position, using primary monitor" << std::endl;
        }
        monitorLeft = 0;
        monitorTop = 0;
        monitorWidth = GetSystemMetrics(SM_CXSCREEN);
        monitorHeight = GetSystemMetrics(SM_CYSCREEN);
        monitorHandle = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
        return;
    }
    
    // Cache primary monitor info (only once)
    static int cachedPrimaryLeft = -1;
    static int cachedPrimaryTop = -1;
    if (cachedPrimaryLeft == -1) {
        HMONITOR primaryMonitorHandle = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFOEX primaryMonitorInfoEx = {};
        primaryMonitorInfoEx.cbSize = sizeof(MONITORINFOEX);
        if (GetMonitorInfo(primaryMonitorHandle, (MONITORINFO*)&primaryMonitorInfoEx)) {
            cachedPrimaryLeft = primaryMonitorInfoEx.rcMonitor.left;
            cachedPrimaryTop = primaryMonitorInfoEx.rcMonitor.top;
            primaryMonitorLeft = cachedPrimaryLeft;
            primaryMonitorTop = cachedPrimaryTop;
        } else {
            cachedPrimaryLeft = 0;
            cachedPrimaryTop = 0;
            primaryMonitorLeft = 0;
            primaryMonitorTop = 0;
        }
    } else {
        primaryMonitorLeft = cachedPrimaryLeft;
        primaryMonitorTop = cachedPrimaryTop;
    }
    
    // Enumerate all monitors to find which one contains the cursor
    struct MonitorEnumData {
        POINT point;
        HMONITOR foundMonitor;
        bool found;
    };
    
    MonitorEnumData enumData = { {cursorPos.x, cursorPos.y}, nullptr, false };
    
    struct MonitorEnumHelper {
        static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
            MonitorEnumData* data = reinterpret_cast<MonitorEnumData*>(dwData);
            MONITORINFOEX infoEx = {};
            infoEx.cbSize = sizeof(MONITORINFOEX);
            
            if (GetMonitorInfo(hMonitor, (MONITORINFO*)&infoEx)) {
                if (data->point.x >= infoEx.rcMonitor.left && data->point.x < infoEx.rcMonitor.right &&
                    data->point.y >= infoEx.rcMonitor.top && data->point.y < infoEx.rcMonitor.bottom) {
                    data->foundMonitor = hMonitor;
                    data->found = true;
                    return FALSE; // Stop enumeration
                }
            }
            return TRUE; // Continue enumeration
        }
    };
    
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumHelper::MonitorEnumProc, (LPARAM)&enumData);
    
    if (enumData.found) {
        // Check if monitor changed
        HMONITOR newMonitorHandle = enumData.foundMonitor;
        bool monitorChanged = (monitorHandle != newMonitorHandle);
        
        // Only proceed if monitor handle actually changed
        // This prevents false positives from boundary checking
        if (monitorChanged) {
            // Get info from the correct monitor first
            MONITORINFOEX monitorInfoEx = {};
            monitorInfoEx.cbSize = sizeof(MONITORINFOEX);
            if (GetMonitorInfo(newMonitorHandle, (MONITORINFO*)&monitorInfoEx)) {
                int newMonitorLeft = monitorInfoEx.rcMonitor.left;
                int newMonitorTop = monitorInfoEx.rcMonitor.top;
                int newMonitorWidth = monitorInfoEx.rcMonitor.right - monitorInfoEx.rcMonitor.left;
                int newMonitorHeight = monitorInfoEx.rcMonitor.bottom - monitorInfoEx.rcMonitor.top;
                
                // Always update when monitor handle changes (even if dimensions same)
                // This ensures we stay on the correct monitor even if two monitors have same size
                if (verbose) {
                    std::cout << "Monitor changed! New monitor: " << newMonitorWidth << "x" << newMonitorHeight 
                              << " at (" << newMonitorLeft << ", " << newMonitorTop << ")" << std::endl;
                    std::cout << "Monitor name: " << monitorInfoEx.szDevice << std::endl;
                }
                
                // Update monitor handle and info
                monitorHandle = newMonitorHandle;
                monitorLeft = newMonitorLeft;
                monitorTop = newMonitorTop;
                monitorWidth = newMonitorWidth;
                monitorHeight = newMonitorHeight;
                
                // Cache monitor bounds for quick boundary checking
                monitorRight = monitorLeft + monitorWidth;
                monitorBottom = monitorTop + monitorHeight;
                
                // Update refresh rate for the new monitor
                updateRefreshRate();
                
                // Update overlay position (function will check if position actually changed)
                updateOverlayPosition();
            }
        }
        // Note: We don't update bounds cache here if monitor didn't change
        // because bounds are only updated when monitor actually changes
    } else {
        // Fallback to primary monitor
        if (verbose) {
            std::cout << "Could not find monitor containing cursor, using primary monitor" << std::endl;
        }
        monitorLeft = 0;
        monitorTop = 0;
        monitorWidth = GetSystemMetrics(SM_CXSCREEN);
        monitorHeight = GetSystemMetrics(SM_CYSCREEN);
        monitorHandle = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
        monitorRight = monitorLeft + monitorWidth;
        monitorBottom = monitorTop + monitorHeight;
    }
}

POINT ControllerMapper::checkMonitorChange() {
    // Get cursor position once and return it so it can be reused for debug updates
    POINT cursorPos = {0, 0};
    if (GetCursorPos(&cursorPos)) {
        // Check if cursor is outside current monitor bounds
        // Use strict bounds checking to avoid edge flickering
        if (cursorPos.x < monitorLeft || cursorPos.x >= monitorRight ||
            cursorPos.y < monitorTop || cursorPos.y >= monitorBottom) {
            // Cursor crossed monitor border - detect new monitor immediately
            // detectMonitorFromCursor will check if monitor actually changed before updating
            detectMonitorFromCursor(false); // Silent detection
        }
    }
    return cursorPos;
}

void ControllerMapper::updateRefreshRate() {
    // Get screen refresh rate from the detected monitor
    MONITORINFOEX monitorInfoEx = {};
    monitorInfoEx.cbSize = sizeof(MONITORINFOEX);
    int refreshRate = 60; // Default fallback
    
    if (GetMonitorInfo(monitorHandle, (MONITORINFO*)&monitorInfoEx)) {
        HDC monitorDC = CreateDC(TEXT("DISPLAY"), monitorInfoEx.szDevice, nullptr, nullptr);
        if (monitorDC) {
            refreshRate = GetDeviceCaps(monitorDC, VREFRESH);
            DeleteDC(monitorDC);
        }
    }
    
    // Calculate update interval based on refresh rate
    int newUpdateIntervalMs;
    if (refreshRate > 1) {
        newUpdateIntervalMs = 1000 / refreshRate;
    } else {
        // Fallback to 60Hz if detection fails
        newUpdateIntervalMs = 16;
    }
    
    // Only update if refresh rate actually changed
    if (updateIntervalMs != newUpdateIntervalMs) {
        updateIntervalMs = newUpdateIntervalMs;
        std::cout << "Monitor refresh rate changed to: " << refreshRate << "Hz" << std::endl;
        std::cout << "Update interval changed to: " << updateIntervalMs << "ms" << std::endl;
    }
}

void ControllerMapper::updateOverlayPosition() {
    if (!overlayHwnd) return;
    
    // Calculate new overlay position based on current monitor
    int screenWidth = monitorWidth;
    int screenHeight = monitorHeight;
    int overlayHeight = (int)(screenHeight * 0.9);
    int overlayWidth = screenWidth;
    
    int posX = monitorLeft; // Start at left edge of detected monitor
    int posY = monitorTop + (screenHeight - overlayHeight) / 2;
    
    // Get current window position and size
    RECT currentRect;
    bool gotCurrentRect = GetWindowRect(overlayHwnd, &currentRect);
    int currentX = gotCurrentRect ? currentRect.left : overlayPosX;
    int currentY = gotCurrentRect ? currentRect.top : overlayPosY;
    int currentWidth = gotCurrentRect ? (currentRect.right - currentRect.left) : overlayWidth;
    int currentHeight = gotCurrentRect ? (currentRect.bottom - currentRect.top) : overlayHeight;
    
    // Check if position AND size actually changed to prevent unnecessary window moves
    bool posChanged = (overlayPosX != posX || overlayPosY != posY);
    bool sizeChanged = (currentWidth != overlayWidth || currentHeight != overlayHeight);
    
    if (!posChanged && !sizeChanged) {
        // Nothing changed, no need to move/resize window
        return;
    }
    
    // Update overlay position
    overlayPosX = posX;
    overlayPosY = posY;
    
    // Move the overlay window (use flags to minimize flicker)
    UINT flags = SWP_SHOWWINDOW | SWP_NOREDRAW | SWP_NOZORDER;
    if (!sizeChanged) {
        flags |= SWP_NOSIZE;
    }
    if (!posChanged) {
        flags |= SWP_NOMOVE;
    }
    
    SetWindowPos(overlayHwnd, HWND_TOPMOST, posX, posY, overlayWidth, overlayHeight, flags);
    
    // Don't force immediate redraw - let Windows handle it naturally to reduce flicker
    // The window will redraw on next paint cycle
    
    std::cout << "Overlay repositioned to: (" << overlayPosX << ", " << overlayPosY << ")" << std::endl;
}

void ControllerMapper::createOverlay() {
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

    // Monitor detection should already be done before calling createOverlay()
    // Use detected monitor dimensions
    int screenWidth = monitorWidth;
    int screenHeight = monitorHeight;
    
    // Calculate overlay size - full screen width, 90% height
    int overlayHeight = (int)(screenHeight * 0.9);
    int overlayWidth = screenWidth; // Full screen width for debug text visibility
    
    // Calculate stick radius to fill most of the overlay (leave some padding)
    // Use height as reference since width is now full-screen
    overlayStickRadius = (int)(overlayHeight * 0.45); // 45% of height (90% of half = radius)
    
    // Get screen refresh rate from the detected monitor (using helper function)
    updateRefreshRate();
    
    // Position overlay on the detected monitor (left edge of monitor, vertically centered)
    int posX = monitorLeft; // Start at left edge of detected monitor
    int posY = monitorTop + (screenHeight - overlayHeight) / 2;
    
    // Store overlay position
    overlayPosX = posX;
    overlayPosY = posY;
    
    std::cout << "Overlay positioned at: (" << overlayPosX << ", " << overlayPosY << ")" << std::endl;
    std::cout << "Overlay center will be at: (" << overlayPosX + overlayWidth / 2 << ", " << overlayPosY + overlayHeight / 2 << ")" << std::endl;
    
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
    
    // Initialize touch injection (only if in touch mode)
    if (currentMode == InputMode::Touch) {
        initializeTouchInjection();
    }
}

// ========== Controller Initialization ==========

void ControllerMapper::initializeControllers() {
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
    
    // Now that controller is selected, detect which monitor contains the cursor
    // and create the overlay on that monitor
    detectMonitorFromCursor(true); // Verbose on first detection
    createOverlay();
}

std::vector<ControllerInfo> ControllerMapper::listAllControllers() {
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

void ControllerMapper::displayControllerMenu(const std::vector<ControllerInfo>& controllers) {
    std::cout << "\r\n=== CONTROLLER SELECTION ===" << std::endl;
    std::cout << "Available controllers:" << std::endl;
    
    for (size_t i = 0; i < controllers.size(); i++) {
        std::string typeStr = (controllers[i].type == ControllerType::XInput) ? "XInput" : "DirectInput";
        std::cout << "[" << (i + 1) << "] " << controllers[i].name << " (" << typeStr << ")" << std::endl;
    }
    
    std::cout << "\r\nPress the number key (1-" << controllers.size() << ") to select a controller:" << std::endl;
}

int ControllerMapper::getControllerSelection(int maxControllers) {
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

bool ControllerMapper::initializeDirectInputWithDevice(const GUID& deviceGuid) {
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

int ControllerMapper::calculateAlpha(double distance, bool touchActive, bool pointerLocked) {
    if (touchActive || pointerLocked) {
        return 255; // Full opacity when active
    }
    // Map 0.0-0.5 distance to 0-255 alpha (max opacity at 50% from center)
    return (distance >= 0.5) ? 255 : (int)((distance / 0.5) * 255.0);
}

void ControllerMapper::updateTouchPointerPosition(bool touchActive, bool pointerLocked, int heldDirection, int lockedDirection,
                                                  double stickX, double stickY, double& lockedX, double& lockedY, int& alpha) {
    if (touchActive) {
        if (pointerLocked && heldDirection >= 0) {
            calculateLockedPosition(heldDirection, lockedDirection, stickX, stickY, lockedX, lockedY);
        } else {
            lockedX = stickX;
            lockedY = stickY;
        }
        alpha = 255;
    } else {
        alpha = 0;
    }
}

void ControllerMapper::updateOverlay(double leftX, double leftY, double rightX, double rightY, double leftAngle, double rightAngle) {
    overlayLeftX = leftX;
    overlayLeftY = leftY;
    overlayRightX = rightX;
    overlayRightY = rightY;
    overlayLeftAngle = leftAngle;
    overlayRightAngle = rightAngle;
    
    // Calculate alpha values using helper function
    double leftDistance = std::sqrt(leftX * leftX + leftY * leftY);
    double rightDistance = std::sqrt(rightX * rightX + rightY * rightY);
    overlayLeftAlpha = calculateAlpha(leftDistance, leftTouchActive, leftPointerLocked);
    overlayRightAlpha = calculateAlpha(rightDistance, rightTouchActive, rightPointerLocked);
    
    // Calculate active touch pointer positions using helper function
    updateTouchPointerPosition(leftTouchActive, leftPointerLocked, currentLHeldDirection, leftLockedDirection,
                              leftX, leftY, overlayLeftLockedX, overlayLeftLockedY, overlayLeftLockedAlpha);
    updateTouchPointerPosition(rightTouchActive, rightPointerLocked, currentRHeldDirection, rightLockedDirection,
                              rightX, rightY, overlayRightLockedX, overlayRightLockedY, overlayRightLockedAlpha);
    
    // Calculate L3/R3 5-touch X pattern positions and alpha
    overlayL3CenterX = l3TouchActive ? leftX : 0;
    overlayL3CenterY = l3TouchActive ? leftY : 0;
    overlayL3Alpha = l3TouchActive ? 255 : 0;
    
    overlayR3CenterX = r3TouchActive ? rightX : 0;
    overlayR3CenterY = r3TouchActive ? rightY : 0;
    overlayR3Alpha = r3TouchActive ? 255 : 0;
    
    // Only redraw if overlay content actually changed to prevent unnecessary blinking
    bool contentChanged = (
        overlayLeftX != prevOverlayLeftX || overlayLeftY != prevOverlayLeftY ||
        overlayRightX != prevOverlayRightX || overlayRightY != prevOverlayRightY ||
        overlayLeftAngle != prevOverlayLeftAngle || overlayRightAngle != prevOverlayRightAngle ||
        overlayLeftAlpha != prevOverlayLeftAlpha || overlayRightAlpha != prevOverlayRightAlpha ||
        overlayLeftLockedAlpha != prevOverlayLeftLockedAlpha ||
        overlayRightLockedAlpha != prevOverlayRightLockedAlpha ||
        overlayLeftLockedX != prevOverlayLeftLockedX || overlayLeftLockedY != prevOverlayLeftLockedY ||
        overlayRightLockedX != prevOverlayRightLockedX || overlayRightLockedY != prevOverlayRightLockedY ||
        overlayL3CenterX != prevOverlayL3CenterX || overlayL3CenterY != prevOverlayL3CenterY ||
        overlayR3CenterX != prevOverlayR3CenterX || overlayR3CenterY != prevOverlayR3CenterY ||
        overlayL3Alpha != prevOverlayL3Alpha || overlayR3Alpha != prevOverlayR3Alpha
    );
    
    if (contentChanged && overlayHwnd) {
        // Update previous values
        prevOverlayLeftX = overlayLeftX;
        prevOverlayLeftY = overlayLeftY;
        prevOverlayRightX = overlayRightX;
        prevOverlayRightY = overlayRightY;
        prevOverlayLeftAngle = overlayLeftAngle;
        prevOverlayRightAngle = overlayRightAngle;
        prevOverlayLeftAlpha = overlayLeftAlpha;
        prevOverlayRightAlpha = overlayRightAlpha;
        prevOverlayLeftLockedAlpha = overlayLeftLockedAlpha;
        prevOverlayRightLockedAlpha = overlayRightLockedAlpha;
        prevOverlayLeftLockedX = overlayLeftLockedX;
        prevOverlayLeftLockedY = overlayLeftLockedY;
        prevOverlayRightLockedX = overlayRightLockedX;
        prevOverlayRightLockedY = overlayRightLockedY;
        prevOverlayL3CenterX = overlayL3CenterX;
        prevOverlayL3CenterY = overlayL3CenterY;
        prevOverlayR3CenterX = overlayR3CenterX;
        prevOverlayR3CenterY = overlayR3CenterY;
        prevOverlayL3Alpha = overlayL3Alpha;
        prevOverlayR3Alpha = overlayR3Alpha;
        
        // Only redraw when content actually changed
        InvalidateRect(overlayHwnd, nullptr, TRUE); // Let Windows handle redraw timing
    }
}

// ========== Window Procedures ==========

LRESULT CALLBACK ControllerMapper::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        ControllerMapper* pThis = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (ControllerMapper*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (ControllerMapper*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
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

LRESULT CALLBACK ControllerMapper::OverlayWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        ControllerMapper* pThis = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (ControllerMapper*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (ControllerMapper*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
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

// ========== Overlay Rendering ==========

void ControllerMapper::drawOverlay(HDC hdc) {
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
        // Use active thickness if either L1/R1 is pressed, otherwise use stick thickness
        int thickness = (leftTouchActive || rightTouchActive) ? -1 : (1 + (maxAlpha * 5 / 255));
        drawDirectionIndicator(hdc, centerX, centerY, leftDirection, RGB(255, 255, 0), maxAlpha, thickness);
    } else {
        // Different directions or only one moved - draw each separately
        if (leftStickMoved && leftDirection >= 0) {
            // Use active thickness if L1/R1 is pressed, otherwise use stick thickness
            int thickness = leftTouchActive ? -1 : (1 + (overlayLeftAlpha * 5 / 255));
            drawDirectionIndicator(hdc, centerX, centerY, leftDirection, RGB(100, 150, 255), overlayLeftAlpha, thickness);
        }
        if (rightStickMoved && rightDirection >= 0) {
            // Use active thickness if L1/R1 is pressed, otherwise use stick thickness
            int thickness = rightTouchActive ? -1 : (1 + (overlayRightAlpha * 5 / 255));
            drawDirectionIndicator(hdc, centerX, centerY, rightDirection, RGB(255, 100, 150), overlayRightAlpha, thickness);
        }
    }
    
    // Draw both sticks at the same position (they will overlap)
    // Draw left stick (blue)
    drawStick(hdc, centerX, centerY, overlayLeftX, overlayLeftY, RGB(100, 150, 255), overlayLeftAlpha);
    
    // Draw right stick (pink)
    drawStick(hdc, centerX, centerY, overlayRightX, overlayRightY, RGB(255, 100, 150), overlayRightAlpha);
    
    // Draw locked pointers (solid circles, bigger, only when trigger press (L2/R2) is active)
    // Left locked pointer (darker blue) - draw below raw pointer
    drawLockedPointer(hdc, centerX, centerY, overlayLeftLockedX, overlayLeftLockedY, RGB(50, 100, 200), overlayLeftLockedAlpha);
    
    // Right locked pointer (darker pink) - draw below raw pointer
    drawLockedPointer(hdc, centerX, centerY, overlayRightLockedX, overlayRightLockedY, RGB(200, 50, 100), overlayRightLockedAlpha);
    
    // Draw L3/R3 5-touch X pattern (only when L3/R3 is active)
    // L3 pattern (darker blue-green) - draw below locked pointer
    draw5TouchXPattern(hdc, centerX, centerY, overlayL3CenterX, overlayL3CenterY, RGB(50, 200, 150), overlayL3Alpha);
    
    // R3 pattern (darker pink-purple) - draw below locked pointer
    draw5TouchXPattern(hdc, centerX, centerY, overlayR3CenterX, overlayR3CenterY, RGB(200, 50, 150), overlayR3Alpha);
    
    // Draw debug text on the middle left (if enabled)
    if (showDebugInfo && !debugText.empty()) {
        drawDebugText(hdc, rect);
    }
}

void ControllerMapper::drawDirectionIndicator(HDC hdc, int centerX, int centerY, int direction, COLORREF color, int alpha, int thickness) {
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
                // Default active thickness (when L1/R1 pressed)
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

void ControllerMapper::drawStick(HDC hdc, int centerX, int centerY, double stickX, double stickY, COLORREF color, int alpha) {
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

void ControllerMapper::drawTouchPointIndicatorAtOverlayPos(HDC hdc, int overlayX, int overlayY, COLORREF color) {
    // Draw a circle at overlay-relative coordinates (where the stick indicator is)
    // This is simpler and more reliable - just draw at the stick position
    const int TOUCH_INDICATOR_RADIUS = 40; // Larger radius for visibility
    
    // Outer circle (border) - white for maximum visibility
    HPEN borderPen = CreatePen(PS_SOLID, 5, RGB(255, 255, 255)); // Thick white border
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    
    Ellipse(hdc,
            overlayX - TOUCH_INDICATOR_RADIUS,
            overlayY - TOUCH_INDICATOR_RADIUS,
            overlayX + TOUCH_INDICATOR_RADIUS,
            overlayY + TOUCH_INDICATOR_RADIUS);
    
    // Inner circle (filled) - bright color
    HBRUSH fillBrush = CreateSolidBrush(color);
    SelectObject(hdc, fillBrush);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    
    Ellipse(hdc,
            overlayX - TOUCH_INDICATOR_RADIUS + 5,
            overlayY - TOUCH_INDICATOR_RADIUS + 5,
            overlayX + TOUCH_INDICATOR_RADIUS - 5,
            overlayY + TOUCH_INDICATOR_RADIUS - 5);
    
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    DeleteObject(fillBrush);
}

void ControllerMapper::drawTouchPointIndicator(HDC hdc, LONG screenX, LONG screenY, COLORREF color) {
    // Draw a circle at the actual screen coordinates where touch is being sent
    // This shows exactly where InputInjector is sending the touch
    RECT overlayRect;
    GetWindowRect(overlayHwnd, &overlayRect);
    
    // Convert screen coordinates to overlay window coordinates
    int indicatorX = screenX - overlayRect.left;
    int indicatorY = screenY - overlayRect.top;
    
    // Get overlay dimensions
    int overlayWidth = overlayRect.right - overlayRect.left;
    int overlayHeight = overlayRect.bottom - overlayRect.top;
    const int TOUCH_INDICATOR_RADIUS = 30; // 30 pixel radius circle - large and visible
    
    // Draw even if slightly outside - we want to see where touches are going
    // Only skip if completely off-screen (more generous bounds for visibility)
    if (indicatorX < -150 || indicatorX > overlayWidth + 150 ||
        indicatorY < -150 || indicatorY > overlayHeight + 150) {
        // Still try to draw if it's close - just clip it
        if (indicatorX < -200 || indicatorX > overlayWidth + 200 ||
            indicatorY < -200 || indicatorY > overlayHeight + 200) {
            return; // Completely outside visible area
        }
    }
    
    // Draw a large, highly visible circle with bright border to show touch point
    // Use bright, contrasting colors
    
    // Outer circle (border) - white for maximum visibility
    HPEN borderPen = CreatePen(PS_SOLID, 4, RGB(255, 255, 255)); // White border
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    
    Ellipse(hdc,
            indicatorX - TOUCH_INDICATOR_RADIUS,
            indicatorY - TOUCH_INDICATOR_RADIUS,
            indicatorX + TOUCH_INDICATOR_RADIUS,
            indicatorY + TOUCH_INDICATOR_RADIUS);
    
    // Inner circle (filled) - bright color
    HBRUSH fillBrush = CreateSolidBrush(color);
    SelectObject(hdc, fillBrush);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    
    Ellipse(hdc,
            indicatorX - TOUCH_INDICATOR_RADIUS + 4,
            indicatorY - TOUCH_INDICATOR_RADIUS + 4,
            indicatorX + TOUCH_INDICATOR_RADIUS - 4,
            indicatorY + TOUCH_INDICATOR_RADIUS - 4);
    
    // Center dot for precise location
    HBRUSH centerBrush = CreateSolidBrush(RGB(255, 255, 255)); // White center
    SelectObject(hdc, centerBrush);
    Ellipse(hdc,
            indicatorX - 5,
            indicatorY - 5,
            indicatorX + 5,
            indicatorY + 5);
    
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    DeleteObject(fillBrush);
    DeleteObject(centerBrush);
}

void ControllerMapper::drawLockedPointer(HDC hdc, int centerX, int centerY, double stickX, double stickY, COLORREF color, int alpha) {
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

void ControllerMapper::draw5TouchXPattern(HDC hdc, int centerX, int centerY, double centerStickX, double centerStickY, COLORREF color, int alpha) {
    if (alpha == 0) return; // Don't draw if invisible
    
    // Calculate the 5 positions in an X pattern
    // 150 pixels radius converted to stick coordinate space
    const int radiusPixels = X_PATTERN_RADIUS_PIXELS;  // 150 pixels
    const double offsetFactor = radiusPixels * 0.7071067811865476; // radius * sqrt(2)/2 for 45-degree offset
    double offsetStickX = (offsetFactor / overlayStickRadius);
    double offsetStickY = (offsetFactor / overlayStickRadius);
    
    // Center position
    int centerScreenX = centerX + (int)(centerStickX * (overlayStickRadius - OVERLAY_STICK_INDICATOR_RADIUS));
    int centerScreenY = centerY - (int)(centerStickY * (overlayStickRadius - OVERLAY_STICK_INDICATOR_RADIUS)); // Inverted Y
    
    // Create solid brush for filled circles
    HBRUSH solidBrush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, solidBrush);
    HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    
    // Draw center circle (Touch 2)
    Ellipse(hdc,
            centerScreenX - OVERLAY_LOCKED_INDICATOR_RADIUS,
            centerScreenY - OVERLAY_LOCKED_INDICATOR_RADIUS,
            centerScreenX + OVERLAY_LOCKED_INDICATOR_RADIUS,
            centerScreenY + OVERLAY_LOCKED_INDICATOR_RADIUS);
    
    // Draw 4 corner circles in X pattern
    // Top-left (Touch 3): centerX - offset, centerY - offset (but Y is inverted in stick coords)
    int tlX = centerX + (int)((centerStickX - offsetStickX) * (overlayStickRadius - OVERLAY_STICK_INDICATOR_RADIUS));
    int tlY = centerY - (int)((centerStickY + offsetStickY) * (overlayStickRadius - OVERLAY_STICK_INDICATOR_RADIUS));
    Ellipse(hdc, tlX - OVERLAY_LOCKED_INDICATOR_RADIUS, tlY - OVERLAY_LOCKED_INDICATOR_RADIUS,
            tlX + OVERLAY_LOCKED_INDICATOR_RADIUS, tlY + OVERLAY_LOCKED_INDICATOR_RADIUS);
    
    // Top-right (Touch 4): centerX + offset, centerY - offset
    int trX = centerX + (int)((centerStickX + offsetStickX) * (overlayStickRadius - OVERLAY_STICK_INDICATOR_RADIUS));
    int trY = centerY - (int)((centerStickY + offsetStickY) * (overlayStickRadius - OVERLAY_STICK_INDICATOR_RADIUS));
    Ellipse(hdc, trX - OVERLAY_LOCKED_INDICATOR_RADIUS, trY - OVERLAY_LOCKED_INDICATOR_RADIUS,
            trX + OVERLAY_LOCKED_INDICATOR_RADIUS, trY + OVERLAY_LOCKED_INDICATOR_RADIUS);
    
    // Bottom-left (Touch 5): centerX - offset, centerY + offset
    int blX = centerX + (int)((centerStickX - offsetStickX) * (overlayStickRadius - OVERLAY_STICK_INDICATOR_RADIUS));
    int blY = centerY - (int)((centerStickY - offsetStickY) * (overlayStickRadius - OVERLAY_STICK_INDICATOR_RADIUS));
    Ellipse(hdc, blX - OVERLAY_LOCKED_INDICATOR_RADIUS, blY - OVERLAY_LOCKED_INDICATOR_RADIUS,
            blX + OVERLAY_LOCKED_INDICATOR_RADIUS, blY + OVERLAY_LOCKED_INDICATOR_RADIUS);
    
    // Bottom-right (Touch 6): centerX + offset, centerY + offset
    int brX = centerX + (int)((centerStickX + offsetStickX) * (overlayStickRadius - OVERLAY_STICK_INDICATOR_RADIUS));
    int brY = centerY - (int)((centerStickY - offsetStickY) * (overlayStickRadius - OVERLAY_STICK_INDICATOR_RADIUS));
    Ellipse(hdc, brX - OVERLAY_LOCKED_INDICATOR_RADIUS, brY - OVERLAY_LOCKED_INDICATOR_RADIUS,
            brX + OVERLAY_LOCKED_INDICATOR_RADIUS, brY + OVERLAY_LOCKED_INDICATOR_RADIUS);
    
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(solidBrush);
}

void ControllerMapper::drawDebugText(HDC hdc, RECT rect) {
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

// ========== Utility Functions ==========

double ControllerMapper::calculateAngle(double x, double y) {
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

int ControllerMapper::getDirection(double angle) {
    if (angle == -1) return -1; // No input detected
    
    // Map 0-360 to 0-7 directions exactly like AHK script
    // Use Floor(angle / 45) without any offset for precise mapping
    int direction = static_cast<int>(std::floor(angle / DEGREES_PER_SECTOR));
    if (direction >= DIRECTION_SECTORS) {
        direction = 0;  // Reset to 0 if out of bounds (same as AHK script)
    }
    return direction;
}

void ControllerMapper::getAdjacentDirections(int direction, int& leftAdjacent, int& rightAdjacent) {
    if (direction < 0 || direction >= DIRECTION_SECTORS) {
        leftAdjacent = -1;
        rightAdjacent = -1;
        return;
    }
    
    leftAdjacent = (direction - 1 + DIRECTION_SECTORS) % DIRECTION_SECTORS;
    rightAdjacent = (direction + 1) % DIRECTION_SECTORS;
}

void ControllerMapper::getDirectionArcCenter(int direction, double& centerX, double& centerY) {
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

bool ControllerMapper::checkPointerLock(int heldDirection, int currentDirection, double currentAngle, int& lockedDirection) {
    if (heldDirection < 0 || currentDirection < 0) return false;
    
    // Calculate the opposite direction (180° away)
    int oppositeDirection = (heldDirection + 4) % 8;
    
    // Get the adjacent neighbors of the opposite direction
    // These form the two possible end points of the path
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
    // This chooses the path the user is moving toward
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

void ControllerMapper::calculateLockedPosition(int heldDirection, int lockedDirection, 
                              double currentX, double currentY,
                              double& lockedX, double& lockedY) {
    // Calculate angles for held and locked directions
    double heldAngle = (heldDirection * DEGREES_PER_SECTOR) + 22.5;
    if (heldAngle >= 360.0) heldAngle -= 360.0;
    
    double endAngle = (lockedDirection * DEGREES_PER_SECTOR) + 22.5;
    if (endAngle >= 360.0) endAngle -= 360.0;
    
    // Calculate path direction vector (from held to locked direction)
    double pathX = std::sin(endAngle * PI / 180.0) - std::sin(heldAngle * PI / 180.0);
    double pathY = std::cos(endAngle * PI / 180.0) - std::cos(heldAngle * PI / 180.0);
    
    // Normalize the path vector to get unit direction
    double pathLength = std::sqrt(pathX * pathX + pathY * pathY);
    if (pathLength > 0) {
        pathX /= pathLength;
        pathY /= pathLength;
    }
    
    // Get center position of held direction (path start point)
    // Use center of direction arc instead of captured position for consistency
    double heldX, heldY;
    getDirectionArcCenter(heldDirection, heldX, heldY);
    
    // Project current stick position onto the path using dot product
    // This projects the vector from held center to current position onto the path direction
    double rawDx = currentX - heldX;
    double rawDy = currentY - heldY;
    double t = rawDx * pathX + rawDy * pathY; // Projection length along unit path
    
    // Clamp t to [0, pathLength] to stay within the path segment
    // This prevents the locked position from going beyond the path endpoints
    if (t < 0.0) t = 0.0;
    if (t > pathLength) t = pathLength;
    
    // Calculate final locked position along the path
    lockedX = heldX + t * pathX;
    lockedY = heldY + t * pathY;
}

void ControllerMapper::updateDebugInfo(double lAngle, double rAngle, int lDirection, int rDirection, DIJOYSTATE2* diState) {
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
        
        // Add monitor information - use overlay's actual monitor for accuracy
        HMONITOR overlayMon = MonitorFromWindow(overlayHwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEX monitorInfoEx = {};
        monitorInfoEx.cbSize = sizeof(MONITORINFOEX);
        if (GetMonitorInfo(overlayMon, (MONITORINFO*)&monitorInfoEx)) {
            info += "Monitor: " + std::string(monitorInfoEx.szDevice) + "\r\n";
            LONG actualMonLeft = monitorInfoEx.rcMonitor.left;
            LONG actualMonTop = monitorInfoEx.rcMonitor.top;
            LONG actualMonWidth = monitorInfoEx.rcMonitor.right - monitorInfoEx.rcMonitor.left;
            LONG actualMonHeight = monitorInfoEx.rcMonitor.bottom - monitorInfoEx.rcMonitor.top;
            info += "  Size: " + std::to_string(actualMonWidth) + "x" + std::to_string(actualMonHeight) + "\r\n";
            info += "  Position: (" + std::to_string(actualMonLeft) + ", " + std::to_string(actualMonTop) + ")\r\n";
            
            // Also show if it's primary
            HMONITOR primaryMon = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
            if (overlayMon == primaryMon) {
                info += "  (Primary Monitor)\r\n";
            } else {
                info += "  (Secondary Monitor)\r\n";
            }
        } else {
            info += "Monitor: Unknown\r\n";
        }
        
        // Show cursor position used for detection
        POINT cursorPos;
        if (GetCursorPos(&cursorPos)) {
            info += "Cursor:\r\n";
            info += "  Position: (" + std::to_string(cursorPos.x) + ", " + std::to_string(cursorPos.y) + ")\r\n";
            info += "  (Used for monitor detection)\r\n";
        }
        info += "\r\n";
        
        info += "  Touch 0 (L1 + L Stick): " + std::string(leftTouchActive ? "ACTIVE" : "---") + "\r\n";
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
        info += "  Touch 1 (R1 + R Stick): " + std::string(rightTouchActive ? "ACTIVE" : "---") + "\r\n";
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
        bool leftPressed = prevL1;
        bool rightPressed = prevR1;
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

void ControllerMapper::logError(const std::string& message) {
    std::cerr << "[ERROR] " << message << std::endl;
}

void ControllerMapper::logInfo(const std::string& message) {
    std::cout << "[INFO] " << message << std::endl;
}

void ControllerMapper::run() {
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
        bool l1Pressed = false;
        bool r1Pressed = false;
        bool l2Pressed = false;
        bool r2Pressed = false;
        bool l3Pressed = false;
        bool r3Pressed = false;
        double joyX = 0, joyY = 0, joyZ = 0, joyR = 0;
        
        if (hasXInputController) {
            // Process XInput controller (Xbox controllers)
            DWORD result = XInputGetState(xInputControllerIndex, &xInputState);
            if (result == ERROR_SUCCESS) {
                controllerSuccess = true;
                
                // XInput button mapping: Left Shoulder = L1, Right Shoulder = R1
                l1Pressed = (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
                r1Pressed = (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
                
                // XInput trigger mapping: L2 = Left trigger, R2 = Right trigger
                // Triggers are analog (0-255), threshold at 128 (50%) to determine press
                l2Pressed = xInputState.Gamepad.bLeftTrigger > 128;
                r2Pressed = xInputState.Gamepad.bRightTrigger > 128;
                
                // XInput stick press mapping: L3 = Left stick press, R3 = Right stick press
                l3Pressed = (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
                r3Pressed = (xInputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
                
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
                l1Pressed = (state.rgbButtons[4] & 0x80) != 0;
                r1Pressed = (state.rgbButtons[5] & 0x80) != 0;
                
                // DirectInput trigger mapping - use L2 and R2 buttons
                l2Pressed = (state.rgbButtons[6] & 0x80) != 0;  // L2 button (button 6)
                r2Pressed = (state.rgbButtons[7] & 0x80) != 0;  // R2 button (button 7)
                
                // DirectInput stick press mapping - use L3 and R3 buttons
                l3Pressed = (state.rgbButtons[10] & 0x80) != 0;  // L3 button (button 10)
                r3Pressed = (state.rgbButtons[11] & 0x80) != 0;  // R3 button (button 11)
                
                
                // DirectInput stick values
                joyX = (state.lX / 32767.5) - 1.0;
                joyY = 1.0 - (state.lY / 32767.5);
                joyZ = (state.lZ / 32767.5) - 1.0;
                joyR = 1.0 - (state.lRz / 32767.5);
            }
        }
        
        if (controllerSuccess) {
            // Calculate angles
            double lAngle = calculateAngle(joyX, joyY);
            double rAngle = calculateAngle(joyZ, joyR);

            // Get directions (for debug info)
            int lDirection = getDirection(lAngle);
            int rDirection = getDirection(rAngle);

            // Handle input based on current mode
            switch (currentMode) {
                case InputMode::Touch:
                    handleTouchControl(l1Pressed, r1Pressed, l2Pressed, r2Pressed, l3Pressed, r3Pressed, joyX, joyY, joyZ, joyR);
                    break;
                case InputMode::Mouse:
                    handleMouseControl(l1Pressed, r1Pressed, joyX, joyY, joyZ, joyR);
                    break;
                case InputMode::Keyboard:
                    handleKeyboardControl(l1Pressed, r1Pressed, joyX, joyY, joyZ, joyR);
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
            
        // Real-time monitor detection - check if cursor crossed monitor border
        // This also gets cursor position which we'll use for debug overlay updates
        POINT cursorPos = checkMonitorChange();
        
        // Check for mouse movement to update debug overlay visually
        if (showDebugInfo && (lastMousePos.x != cursorPos.x || lastMousePos.y != cursorPos.y)) {
            // Mouse moved - force overlay redraw to show updated mouse position
            // The debug info text will be updated on next controller processing cycle
            // which already calls updateDebugInfo() and includes mouse position
            if (overlayHwnd) {
                InvalidateRect(overlayHwnd, nullptr, TRUE);
            }
            lastMousePos = cursorPos;
        }

        // Sleep based on refresh rate to match monitor
        Sleep(updateIntervalMs);
    }
}

void ControllerMapper::cleanup() {
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

