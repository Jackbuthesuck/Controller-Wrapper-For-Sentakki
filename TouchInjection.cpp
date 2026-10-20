#include "ControllerInput.h"

// ========== Touch Injection Implementation ==========

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

	// Map relative position (within current monitor) to equivalent position on other monitor
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
		// Primary RIGHT, on primary: use offset approach
		// Send coordinate past current monitor (relativeX + width) that InputInjector will route back
		touchX = relativeX + currentInfo->width;
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
	return createTouchInfo(touchId, stickX, stickY, isDown, isUp, DEFAULT_TOUCH_CONTACT_RADIUS_PIXELS);
}

InjectedInputTouchInfo ControllerMapper::createTouchInfo(int touchId, double stickX, double stickY, bool isDown, bool isUp, int contactRadius) {
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

	// Set contact area (contactRadius pixels on each side from center)
	InjectedInputRectangle contactArea{};
	contactArea.Left = contactRadius;
	contactArea.Top = contactRadius;
	contactArea.Bottom = contactRadius;
	contactArea.Right = contactRadius;
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
			std::wcout << L"[Touch] Failed: " << ex.message().c_str() << std::endl;
			std::cout << "Error: 0x" << std::hex << ex.code() << std::dec << std::endl;
			errorCount++;
			if (errorCount >= 3) {
				std::cout << "(Further errors suppressed)" << std::endl;
			}
		}
	}
}

void ControllerMapper::sendTouch(int touchId, double stickX, double stickY, bool isDown, bool isUp, int contactRadius) {
	if (!inputInjectorInitialized || !inputInjector) {
		static bool warnedNotReady = false;
		if (!warnedNotReady) {
			std::cout << "[Touch] Ignored: InputInjector is not initialized." << std::endl;
			warnedNotReady = true;
		}
		return;
	}

	// Update touch tracking for overlay
	if (touchId >= 0 && touchId < 20) {
		if (isDown) {
			touchActive[touchId] = true;
			touchX[touchId] = stickX;
			touchY[touchId] = stickY;
		} else if (isUp) {
			touchActive[touchId] = false;
		} else {
			touchX[touchId] = stickX;
			touchY[touchId] = stickY;
		}
	}

	// Use the helper function to create touch info
	InjectedInputTouchInfo touchInfo = createTouchInfo(touchId, stickX, stickY, isDown, isUp, contactRadius);

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
			std::cout << "[Touch] Enabled: Touch " << touchId << " at (" << touchX << "," << touchY << ")" << std::endl;
			firstSuccess = false;
		}

	} catch (hresult_error const& ex) {
		static int errorCount = 0;
		if (errorCount < 3) {
			std::wcout << L"[Touch] Failed: " << ex.message().c_str() << std::endl;
			std::cout << "Error: 0x" << std::hex << ex.code() << std::dec << std::endl;
			errorCount++;
			if (errorCount >= 3) {
				std::cout << "(Further errors suppressed)" << std::endl;
			}
		}
	}
}
