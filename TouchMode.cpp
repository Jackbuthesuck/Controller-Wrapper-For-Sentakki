#include "ControllerInput.h"

// ========== Touch Mode Implementation ==========

void ControllerMapper::initializeTouchInjection() {
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

void ControllerMapper::getTouchCoordinates(double stickX, double stickY, LONG& touchX, LONG& touchY) {
    // Note: Button state is passed through static variables from handleTouchControl
    // Get the actual overlay window position and size from Windows
    RECT overlayRect;
    GetWindowRect(overlayHwnd, &overlayRect);
    int overlayWidth = overlayRect.right - overlayRect.left;
    int overlayHeight = overlayRect.bottom - overlayRect.top;
    
    // Get actual overlay position in virtual screen coordinates
    LONG overlayX = overlayRect.left;
    LONG overlayY = overlayRect.top;
    
    // Detect which monitor the overlay is actually on (more reliable than cursor-based detection)
    HMONITOR overlayMonitor = MonitorFromWindow(overlayHwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEX overlayMonitorInfo = {};
    overlayMonitorInfo.cbSize = sizeof(MONITORINFOEX);
    LONG actualMonitorLeft = 0;
    LONG actualMonitorTop = 0;
    
    if (GetMonitorInfo(overlayMonitor, (MONITORINFO*)&overlayMonitorInfo)) {
        actualMonitorLeft = overlayMonitorInfo.rcMonitor.left;
        actualMonitorTop = overlayMonitorInfo.rcMonitor.top;
    } else {
        // Fallback to detected monitor
        actualMonitorLeft = monitorLeft;
        actualMonitorTop = monitorTop;
    }
    
    // Calculate center of overlay relative to the overlay window
    int overlayCenterXRelative = overlayWidth / 2;
    int overlayCenterYRelative = overlayHeight / 2;
    
    // Calculate touch position relative to overlay center
    int touchXRelative = overlayCenterXRelative + (int)(stickX * overlayStickRadius);
    int touchYRelative = overlayCenterYRelative - (int)(stickY * overlayStickRadius); // Invert Y
    
    // Calculate coordinates in virtual screen space using actual window position
    LONG virtualX = overlayX + touchXRelative;
    LONG virtualY = overlayY + touchYRelative;
    
    // Calculate position relative to the overlay's monitor
    LONG relativeX = virtualX - actualMonitorLeft;
    LONG relativeY = virtualY - actualMonitorTop;
    
    // Enumerate all monitors and calculate proper coordinate mapping
    // InputInjector has "opposite monitor" behavior: send coordinates for "other" monitor,
    // and InputInjector routes them back to current monitor
    
    // Structure to collect all monitor information
    struct MonitorInfo {
        HMONITOR handle;
        RECT rect;
        LONG left, top, right, bottom;
        LONG width, height;
        bool isPrimary;
    };
    
    std::vector<MonitorInfo> allMonitors;
    HMONITOR currentMonitor = overlayMonitor;
    MonitorInfo currentMonitorInfo = {};
    
    // Enumerate all monitors and collect their information
    struct MonitorEnumHelper {
        static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
            std::vector<MonitorInfo>* monitors = reinterpret_cast<std::vector<MonitorInfo>*>(dwData);
            
            MONITORINFOEX monitorInfo = {};
            monitorInfo.cbSize = sizeof(MONITORINFOEX);
            
            if (GetMonitorInfo(hMonitor, (MONITORINFO*)&monitorInfo)) {
                MonitorInfo info;
                info.handle = hMonitor;
                info.rect = monitorInfo.rcMonitor;
                info.left = monitorInfo.rcMonitor.left;
                info.top = monitorInfo.rcMonitor.top;
                info.right = monitorInfo.rcMonitor.right;
                info.bottom = monitorInfo.rcMonitor.bottom;
                info.width = info.right - info.left;
                info.height = info.bottom - info.top;
                
                // Check if this is the primary monitor
                HMONITOR primaryMonitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
                info.isPrimary = (hMonitor == primaryMonitor);
                
                monitors->push_back(info);
            }
            return TRUE; // Continue enumeration
        }
    };
    
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumHelper::MonitorEnumProc, (LPARAM)&allMonitors);
    
    // Find the current monitor's info
    MonitorInfo* currentInfo = nullptr;
    MonitorInfo* otherInfo = nullptr;
    
    for (auto& monitor : allMonitors) {
        if (monitor.handle == currentMonitor) {
            currentInfo = &monitor;
            break;
        }
    }
    
    // If we couldn't find current monitor, fall back
    if (!currentInfo) {
        touchX = virtualX;
        touchY = virtualY;
        return;
    }
    
    // Find the "other" monitor (first monitor that's not the current one)
    for (auto& monitor : allMonitors) {
        if (monitor.handle != currentMonitor) {
            otherInfo = &monitor;
            break;
        }
    }
    
    // If only one monitor, use virtual coordinates directly
    if (!otherInfo || allMonitors.size() <= 1) {
        touchX = virtualX;
        touchY = virtualY;
        return;
    }
    
    // Calculate mapping based on actual monitor layout
    // Use currentInfo dimensions instead of overlayWidth/Height to avoid redeclaration
    
    // Scale relative position from current monitor to other monitor's coordinate space
    double scaleX = (otherInfo->width == currentInfo->width) ? 1.0 : ((double)otherInfo->width / currentInfo->width);
    double scaleY = (otherInfo->height == currentInfo->height) ? 1.0 : ((double)otherInfo->height / currentInfo->height);
    
    // Map relative position (within current monitor) to equivalent position in other monitor
    LONG mappedRelativeX = (LONG)(relativeX * scaleX);
    LONG mappedRelativeY = (LONG)(relativeY * scaleY);
    
    // Clamp mapped coordinates to other monitor's bounds
    if (mappedRelativeX < 0) mappedRelativeX = 0;
    if (mappedRelativeX >= otherInfo->width) mappedRelativeX = otherInfo->width - 1;
    if (mappedRelativeY < 0) mappedRelativeY = 0;
    if (mappedRelativeY >= otherInfo->height) mappedRelativeY = otherInfo->height - 1;
    
    // Calculate virtual screen coordinates for this position on the other monitor
    LONG mappedX = otherInfo->left + mappedRelativeX;
    LONG mappedY = otherInfo->top + mappedRelativeY;
    
    // Unified coordinate transformation for any monitor arrangement
    // Strategy: Handle InputInjector's "opposite monitor" routing behavior
    // InputInjector routes coordinates to the "opposite" monitor in certain arrangements
    // We need to send coordinates that InputInjector will route back to the current monitor
    
    bool currentIsPrimary = currentInfo->isPrimary;
    
    // Find primary and secondary monitor info
    MonitorInfo* primaryMonitorInfo = nullptr;
    MonitorInfo* secondaryMonitorInfo = nullptr;
    for (auto& monitor : allMonitors) {
        if (monitor.isPrimary) {
            primaryMonitorInfo = &monitor;
        } else {
            secondaryMonitorInfo = &monitor;
        }
    }
    
    if (!primaryMonitorInfo || !secondaryMonitorInfo) {
        // Shouldn't happen with 2 monitors, but fallback
        touchX = virtualX;
        touchY = virtualY;
        return;
    }
    
    // Calculate relative positions of primary and secondary
    bool primaryIsLeft = (primaryMonitorInfo->left < secondaryMonitorInfo->left);
    bool primaryIsRight = (primaryMonitorInfo->left > secondaryMonitorInfo->left);
    bool primaryIsAbove = (primaryMonitorInfo->top < secondaryMonitorInfo->top);
    bool primaryIsBelow = (primaryMonitorInfo->top > secondaryMonitorInfo->top);
    
    // Calculate horizontal offset (absolute distance between monitors horizontally)
    // Used for diagonal detection and coordinate calculations
    LONG horizontalOffset = (secondaryMonitorInfo->left > primaryMonitorInfo->left) 
        ? (secondaryMonitorInfo->left - primaryMonitorInfo->left)
        : (primaryMonitorInfo->left - secondaryMonitorInfo->left);
    bool isDiagonal = (horizontalOffset > 0); // Any horizontal offset = diagonal arrangement
    
    // InputInjector's "opposite monitor" routing behavior:
    // - When primary is RIGHT of secondary: coordinates on primary route to secondary (wrong)
    // - When primary is LEFT of secondary: virtual coordinates work correctly
    // - When primary is BELOW secondary: coordinates may route wrong
    // - When primary is ABOVE secondary: virtual coordinates work correctly on secondary, wrong on primary
    
    // X coordinate transformation
    if (primaryIsRight && currentIsPrimary) {
        // Primary RIGHT, on primary
        if (isDiagonal) {
            // Diagonal RIGHT: use formula (center of secondary + horizontal offset)
            touchX = (secondaryMonitorInfo->width / 2) + horizontalOffset;
        } else {
            // Purely horizontal RIGHT: use offset approach
            touchX = relativeX + currentInfo->width;
        }
    } else if (primaryIsRight && !currentIsPrimary) {
        // Primary RIGHT, on secondary: use mappedX (InputInjector routes back to current)
        touchX = mappedX;
    } else {
        // Primary LEFT: virtual coordinates work correctly
        touchX = virtualX;
    }
    
    // Y coordinate transformation
    if (primaryIsBelow && currentIsPrimary) {
        // Primary BELOW, on primary: always use offset approach (works for both vertical and diagonal)
        touchY = relativeY + currentInfo->height;
    } else if (primaryIsBelow && !currentIsPrimary) {
        // Primary BELOW, on secondary: use mappedY
        touchY = mappedY;
    } else if (primaryIsAbove && currentIsPrimary) {
        // Primary ABOVE, on primary: virtualY works correctly
        touchY = virtualY;
    } else {
        // Primary ABOVE and on secondary: virtualY works correctly
        touchY = virtualY;
    }
    
    // Debug: Log all monitor positions for troubleshooting
    // Print immediately when button is pressed (to catch wrong screen mapping quickly)
    // Check button state from static variable set in handleTouchControl
    bool anyButtonPressed = g_anyButtonPressed;
    bool prevAnyButtonPressed = g_prevAnyButtonPressed;
    
    // Print immediately when any button is first pressed (only once per press event)
    // Use press counter to ensure we only print once per button press, even if called multiple times
    bool shouldPrintNow = false;
    if (anyButtonPressed && !prevAnyButtonPressed) {
        // Button just pressed - check if we've already printed for this press counter
        if (g_buttonPressCounter != g_lastPrintedPressCounter) {
            // New press event we haven't printed for yet
            g_lastPrintedPressCounter = g_buttonPressCounter;
            shouldPrintNow = true;
        }
    }
    
    // Print monitor info only on button press (not at intervals)
    // This helps catch wrong screen mapping immediately when it happens
    if (shouldPrintNow) {
        std::cout << "=== ALL MONITORS ===" << std::endl;
        for (size_t i = 0; i < allMonitors.size(); i++) {
            const auto& m = allMonitors[i];
            std::cout << "Monitor " << i << ": " << m.width << "x" << m.height 
                      << " at (" << m.left << ", " << m.top << ")"
                      << (m.isPrimary ? " [PRIMARY]" : " [SECONDARY]")
                      << (m.handle == currentMonitor ? " [CURRENT]" : "")
                      << (m.handle == otherInfo->handle ? " [OTHER]" : "") << std::endl;
        }
        std::cout << "Current: relative=(" << relativeX << "," << relativeY 
                  << ") mappedRelative=(" << mappedRelativeX << "," << mappedRelativeY
                  << ") mappedVirtual=(" << mappedX << "," << mappedY
                  << ") virtualX/Y=(" << virtualX << "," << virtualY
                  << ") final=(" << touchX << "," << touchY << ")" << std::endl;
        std::cout << "Primary status: Current=" << (currentIsPrimary ? "PRIMARY" : "SECONDARY")
                  << " Other=" << (otherInfo->isPrimary ? "PRIMARY" : "SECONDARY") << std::endl;
        std::cout << "Arrangement: primaryIsLeft=" << primaryIsLeft
                  << " primaryIsRight=" << primaryIsRight
                  << " primaryIsAbove=" << primaryIsAbove
                  << " primaryIsBelow=" << primaryIsBelow << std::endl;
        std::cout << "Routing: useOppositeX=" << primaryIsRight
                  << " useOppositeY=" << ((primaryIsBelow && !currentIsPrimary) || (primaryIsAbove && currentIsPrimary)) << std::endl;
        std::cout << "----------------------------------------" << std::endl;
    }
    
    // Don't clamp - let InputInjector handle it, clamping might interfere
}

void ControllerMapper::pixelToHimetric(LONG pixelX, LONG pixelY, LONG& himetricX, LONG& himetricY) {
    // Convert pixel coordinates to HIMETRIC (hundredths of a millimeter)
    // Use DPI from the detected monitor
    MONITORINFOEX monitorInfoEx = {};
    monitorInfoEx.cbSize = sizeof(MONITORINFOEX);
    int dpiX = 96; // Default DPI fallback
    int dpiY = 96;
    
    if (GetMonitorInfo(monitorHandle, (MONITORINFO*)&monitorInfoEx)) {
        HDC monitorDC = CreateDC(TEXT("DISPLAY"), monitorInfoEx.szDevice, nullptr, nullptr);
        if (monitorDC) {
            dpiX = GetDeviceCaps(monitorDC, LOGPIXELSX);
            dpiY = GetDeviceCaps(monitorDC, LOGPIXELSY);
            DeleteDC(monitorDC);
        }
    }
    
    // HIMETRIC calculation: (pixels * 2540) / DPI
    himetricX = (pixelX * 2540) / dpiX;
    himetricY = (pixelY * 2540) / dpiY;
}

InjectedInputTouchInfo ControllerMapper::createTouchInfo(int touchId, double stickX, double stickY, bool isDown, bool isUp) {
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

void ControllerMapper::sendMultipleTouches(const std::vector<InjectedInputTouchInfo>& touches) {
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

void ControllerMapper::send5TouchXPattern(double centerX, double centerY, bool isDown, bool isUp) {
    if (!inputInjectorInitialized || !inputInjector) return;
    
    // Get center screen coordinates
    LONG centerScreenX, centerScreenY;
    getTouchCoordinates(centerX, centerY, centerScreenX, centerScreenY);
    
    // Calculate offsets for X pattern at 45-degree angles
    // Using screen coordinates directly
    // For X pattern: center + offsets at 45, 135, 225, 315 degrees
    const int radius = X_PATTERN_RADIUS_PIXELS;  // 150 pixels
    
    // Calculate offsets using trigonometry
    // cos(45°) = sin(45°) = sqrt(2)/2 ≈ 0.707
    const double offsetFactor = radius * 0.7071067811865476;  // radius * sqrt(2)/2
    
    // Create 5 touch points in X pattern:
    // Touch 2: Center
    // Touch 3: Top-left (225° or -135°)
    // Touch 4: Top-right (135° or -45°)
    // Touch 5: Bottom-left (315° or -45°)
    // Touch 6: Bottom-right (45°)
    
    std::vector<InjectedInputTouchInfo> touches;
    
    // Convert pixel radius to stick coordinate offsets
    // The overlayStickRadius determines the scale
    double offsetStickX = (offsetFactor / overlayStickRadius);
    double offsetStickY = (offsetFactor / overlayStickRadius);
    
    // Touch 2: Center
    touches.push_back(createTouchInfo(2, centerX, centerY, isDown, isUp));
    
    // Touch 3: Top-left (225°: x = -offset, y = -offset) - but Y is inverted in stick coords
    // In screen coords: top-left is negative X, negative Y (from center)
    // In stick coords: Y is inverted, so positive stick Y = negative screen Y
    // So for top-left screen: stickX = centerX - offset, stickY = centerY + offset (inverted)
    touches.push_back(createTouchInfo(3, centerX - offsetStickX, centerY + offsetStickY, isDown, isUp));
    
    // Touch 4: Top-right (135°: x = +offset, y = -offset)
    // In stick coords: stickX = centerX + offset, stickY = centerY + offset (inverted)
    touches.push_back(createTouchInfo(4, centerX + offsetStickX, centerY + offsetStickY, isDown, isUp));
    
    // Touch 5: Bottom-left (315°: x = -offset, y = +offset)
    // In stick coords: stickX = centerX - offset, stickY = centerY - offset (inverted)
    touches.push_back(createTouchInfo(5, centerX - offsetStickX, centerY - offsetStickY, isDown, isUp));
    
    // Touch 6: Bottom-right (45°: x = +offset, y = +offset)
    // In stick coords: stickX = centerX + offset, stickY = centerY - offset (inverted)
    touches.push_back(createTouchInfo(6, centerX + offsetStickX, centerY - offsetStickY, isDown, isUp));
    
    try {
        inputInjector.InjectTouchInput(touches);
        
        if (isDown) {
            static bool firstSuccess = true;
            if (firstSuccess) {
                std::cout << "L3 5-touch X pattern active! 5 touches at (" 
                         << centerScreenX << ", " << centerScreenY << ") with 150px radius" << std::endl;
                firstSuccess = false;
            }
        }
    } catch (hresult_error const& ex) {
        static int errorCount = 0;
        if (errorCount < 3) {
            std::wcout << L"5-touch X pattern injection failed: " << ex.message().c_str() << std::endl;
            std::cout << "Error code: 0x" << std::hex << ex.code() << std::dec << std::endl;
            errorCount++;
            if (errorCount >= 3) {
                std::cout << "(Further errors suppressed)" << std::endl;
            }
        }
    }
}

void ControllerMapper::sendTouch(int touchId, double stickX, double stickY, bool isDown, bool isUp) {
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
        
        // Debug: Print every 5th touch to show it's working
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

void ControllerMapper::sendBothTouchesIfActive(double leftX, double leftY, double rightX, double rightY,
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

void ControllerMapper::handleTouchMovementUpdate(int touchId, bool& touchActive, bool bumperPressed, bool stickPressPressed,
                                  int& heldDirection, bool& lockedState, int& lockedDirection,
                                  double currentX, double currentY, double currentAngle, int currentDirection,
                                  bool skipIfOtherActive) {
    // Early return if touch is not active or neither L1/R1 nor stick press is pressed
    if (!touchActive || (!bumperPressed && !stickPressPressed)) return;
    
    // Determine if we should check for locking
    // Locking is checked when:
    // 1. Trigger press (L2/R2) is pressed AND heldDirection is set (trigger-based locking)
    // 2. When L1/R1 is held, we only lock if trigger press (L2/R2) is also pressed
    // 3. When stick press is held separately, we check if heldDirection >= 0
    bool shouldCheckLocking = (stickPressPressed && heldDirection >= 0);
    
    if (shouldCheckLocking) {
        int newLockedDirection;
        if (checkPointerLock(heldDirection, currentDirection, currentAngle, newLockedDirection)) {
            // Update locked direction if it changed
            if (!lockedState || lockedDirection != newLockedDirection) {
                lockedState = true;
                lockedDirection = newLockedDirection;
            }
            // Calculate locked position using helper function
            double lockedX, lockedY;
            calculateLockedPosition(heldDirection, lockedDirection, currentX, currentY, lockedX, lockedY);
            
            // Skip individual send if both touches are active (will send both together at end)
            if (!skipIfOtherActive) {
                sendTouch(touchId, lockedX, lockedY, false, false); // Touch move/update with locked position
            }
        } else {
            // No locking - unlock and use current position
            if (lockedState) {
                lockedState = false;
                lockedDirection = -1;
            }
            // Skip individual send if both touches are active (will send both together at end)
            if (!skipIfOtherActive) {
                sendTouch(touchId, currentX, currentY, false, false); // Touch move/update
            }
        }
    } else {
        // No trigger press (L2/R2) pressed or no held direction - unlock and use current position
        if (lockedState) {
            lockedState = false;
            lockedDirection = -1;
            std::cout << (touchId == 0 ? "Left" : "Right") << " pointer UNLOCKED" << std::endl;
        }
        // Skip individual send if both touches are active (will send both together at end)
        if (!skipIfOtherActive) {
            sendTouch(touchId, currentX, currentY, false, false); // Touch move/update
        }
    }
}

void ControllerMapper::handleTouchControl(bool l1, bool r1, bool l2, bool r2, bool l3, bool r3, double leftX, double leftY, double rightX, double rightY) {
    // Update static button state for debug output in getTouchCoordinates
    bool newButtonState = (l1 || r1 || l2 || r2 || l3 || r3);
    
    // Increment press counter when button is first pressed
    if (!g_anyButtonPressed && newButtonState) {
        g_buttonPressCounter++; // New button press event
    }
    
    g_prevAnyButtonPressed = g_anyButtonPressed;
    g_anyButtonPressed = newButtonState;
    // Calculate angles and directions (like in keyboard mode) - for future use
    double lAngle = calculateAngle(leftX, leftY);
    double rAngle = calculateAngle(rightX, rightY);
    int lDirection = getDirection(lAngle);
    int rDirection = getDirection(rAngle);
    
    // Detect L2/R2 press edges for pointer locking
    bool l2Pressed = l2 && !prevL2;
    bool r2Pressed = r2 && !prevR2;
    bool l2Released = !l2 && prevL2;
    bool r2Released = !r2 && prevR2;
    
    // Detect L3/R3 press edges (kept for future use)
    bool l3Pressed = l3 && !prevL3;
    bool r3Pressed = r3 && !prevR3;
    bool l3Released = !l3 && prevL3;
    bool r3Released = !r3 && prevR3;
    
    // Detect L1/R1 press edges for touch input
    bool l1Pressed = l1 && !prevL1;
    bool r1Pressed = r1 && !prevR1;
    bool l1Released = !l1 && prevL1;
    bool r1Released = !r1 && prevR1;
    
    // Handle L2/R2 based pointer locking and touch events
    // L2 controls left touch locking
    if (l2Pressed) {
        currentLHeldDirection = lDirection; // Capture current direction when L2 pressed
        currentLHeldX = leftX; // Capture actual stick position
        currentLHeldY = leftY;
        leftTouchActive = true; // Start touch event
        sendTouch(0, leftX, leftY, true, false); // Touch down
    }
    
    if (l2Released) {
        leftPointerLocked = false; // Release lock when L2 released
        leftLockedDirection = -1;
        currentLHeldDirection = -1;
        if (leftTouchActive) {
            sendTouch(0, leftX, leftY, false, true); // Touch up
            leftTouchActive = false;
        }
    }
    
    // R2 controls right touch locking
    if (r2Pressed) {
        currentRHeldDirection = rDirection; // Capture current direction when R2 pressed
        currentRHeldX = rightX; // Capture actual stick position
        currentRHeldY = rightY;
        rightTouchActive = true; // Start touch event
        sendTouch(1, rightX, rightY, true, false); // Touch down
    }
    
    if (r2Released) {
        rightPointerLocked = false; // Release lock when R2 released
        rightLockedDirection = -1;
        currentRHeldDirection = -1;
        if (rightTouchActive) {
            sendTouch(1, rightX, rightY, false, true); // Touch up
            rightTouchActive = false;
        }
    }
    
    // Handle L1 - touch ID 0
    if (l1Pressed && !leftTouchActive) {
        leftTouchActive = true;
        sendTouch(0, leftX, leftY, true, false); // Touch down - only send once on press
    } else if (leftTouchActive && l1) {
        // Use helper function to handle movement updates with locking logic
        handleTouchMovementUpdate(0, leftTouchActive, l1, l2,
                                 currentLHeldDirection, leftPointerLocked, leftLockedDirection,
                                 leftX, leftY, lAngle, lDirection,
                                 rightTouchActive); // Skip if right is also active
    }
    
    if (l1Released && leftTouchActive) {
        sendTouch(0, leftX, leftY, false, true); // Touch up
        leftTouchActive = false;
        std::cout << "L1 released: Sending Touch 0 UP" << std::endl;
    }
    
    // Handle L2 movement while held
    if (leftTouchActive && l2) {
        // Use helper function to handle movement updates with locking logic
        handleTouchMovementUpdate(0, leftTouchActive, l1 || l2, l2,
                                 currentLHeldDirection, leftPointerLocked, leftLockedDirection,
                                 leftX, leftY, lAngle, lDirection,
                                 rightTouchActive); // Skip if right is also active
    }
    
    // Handle R1 - touch ID 1
    if (r1Pressed && !rightTouchActive) {
        rightTouchActive = true;
        sendTouch(1, rightX, rightY, true, false); // Touch down - only send once on press
        std::cout << "R1 pressed: Sending Touch 1 DOWN" << std::endl;
    } else if (rightTouchActive && r1) {
        // Use helper function to handle movement updates with locking logic
        handleTouchMovementUpdate(1, rightTouchActive, r1, r2,
                                 currentRHeldDirection, rightPointerLocked, rightLockedDirection,
                                 rightX, rightY, rAngle, rDirection,
                                 leftTouchActive); // Skip if left is also active
    }
    
    if (r1Released && rightTouchActive) {
        sendTouch(1, rightX, rightY, false, true); // Touch up
        rightTouchActive = false;
        std::cout << "R1 released: Sending Touch 1 UP" << std::endl;
    }
    
    // Handle R2 movement while held
    if (rightTouchActive && r2) {
        // Use helper function to handle movement updates with locking logic
        handleTouchMovementUpdate(1, rightTouchActive, r1 || r2, r2,
                                 currentRHeldDirection, rightPointerLocked, rightLockedDirection,
                                 rightX, rightY, rAngle, rDirection,
                                 leftTouchActive); // Skip if left is also active
    }
    
    // Send both touches together if both are active (for true multi-touch support)
    // This ensures both touches can be held independently
    if (leftTouchActive && rightTouchActive) {
        // Calculate positions for both touches, taking locks into account
        double leftSendX = leftX, leftSendY = leftY;
        double rightSendX = rightX, rightSendY = rightY;
        
        // Check if left touch is locked
        if (leftPointerLocked && currentLHeldDirection >= 0) {
            // Use helper function to calculate locked position
            calculateLockedPosition(currentLHeldDirection, leftLockedDirection, leftX, leftY, 
                                   leftSendX, leftSendY);
        }
        
        // Check if right touch is locked
        if (rightPointerLocked && currentRHeldDirection >= 0) {
            // Use helper function to calculate locked position
            calculateLockedPosition(currentRHeldDirection, rightLockedDirection, rightX, rightY, 
                                   rightSendX, rightSendY);
        }
        
        // Send both touches together
        sendBothTouchesIfActive(leftSendX, leftSendY, rightSendX, rightSendY,
                                leftSendX, leftSendY, leftPointerLocked,
                                rightSendX, rightSendY, rightPointerLocked);
    }
    
    // Handle L3 - 5-touch X pattern (like L1 but with 5 touches)
    if (l3Pressed && !l3TouchActive) {
        l3TouchActive = true;
        send5TouchXPattern(leftX, leftY, true, false); // Touch down for all 5 touches
    } else if (l3TouchActive && l3) {
        // Update 5-touch pattern position while held
        send5TouchXPattern(leftX, leftY, false, false); // Touch move for all 5 touches
    }
    
    if (l3Released && l3TouchActive) {
        send5TouchXPattern(leftX, leftY, false, true); // Touch up for all 5 touches
        l3TouchActive = false;
        std::cout << "L3 released: Sending 5-touch X pattern UP" << std::endl;
    }
    
    // Handle R3 - 5-touch X pattern (like R1 but with 5 touches)
    if (r3Pressed && !r3TouchActive) {
        r3TouchActive = true;
        send5TouchXPattern(rightX, rightY, true, false); // Touch down for all 5 touches
    } else if (r3TouchActive && r3) {
        // Update 5-touch pattern position while held
        send5TouchXPattern(rightX, rightY, false, false); // Touch move for all 5 touches
    }
    
    if (r3Released && r3TouchActive) {
        send5TouchXPattern(rightX, rightY, false, true); // Touch up for all 5 touches
        r3TouchActive = false;
        std::cout << "R3 released: Sending 5-touch X pattern UP" << std::endl;
    }
    
    // Update previous button states
    prevL1 = l1;
    prevR1 = r1;
    prevL2 = l2;
    prevR2 = r2;
    prevL3 = l3;
    prevR3 = r3;
}

