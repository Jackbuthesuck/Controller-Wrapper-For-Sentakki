#include <windows.h>
#include <dinput.h>
#include <XInput.h>
#include <iostream>
#include <string>
#include <cmath>
#include <vector>
#include <conio.h>
#include <memory>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib")

// Constants
namespace Constants {
    constexpr double PI = 3.14159265359;
    constexpr int WINDOW_WIDTH = 500;
    constexpr int WINDOW_HEIGHT = 600;
    constexpr int EDIT_PADDING = 10;
    constexpr int UPDATE_INTERVAL_MS = 200;
    constexpr int MAIN_LOOP_SLEEP_MS = 16;
    constexpr int SELECTION_SLEEP_MS = 4;
    constexpr double STICK_MAX_VALUE = 32767.0;
    constexpr double STICK_NORMALIZE_FACTOR = 32767.5;
    constexpr int DIRECTION_SECTORS = 8;
    constexpr double DEGREES_PER_SECTOR = 45.0;
}

// Enums and Structs
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

struct StickValues {
    double x, y, z, r;
};

struct ButtonState {
    bool leftPressed;
    bool rightPressed;
};

// Forward declarations
class ControllerManager;
class KeySimulator;
class DebugDisplay;
class WindowManager;

// Controller Manager - Handles all controller input
class ControllerManager {
private:
    LPDIRECTINPUT8 di;
    LPDIRECTINPUTDEVICE8 joystick;
    bool hasXInputController;
    DWORD xInputControllerIndex;
    XINPUT_STATE xInputState;

public:
    ControllerManager() : di(nullptr), joystick(nullptr), hasXInputController(false), xInputControllerIndex(0) {}
    
    ~ControllerManager() {
        cleanup();
    }

    bool initialize(const ControllerInfo& info, HWND hwnd);
    StickValues getStickValues() const;
    ButtonState getButtonState() const;
    bool isConnected() const;
    void cleanup();
    
private:
    bool initializeXInput(DWORD index);
    bool initializeDirectInput(const GUID& guid, HWND hwnd);
};

// Key Simulator - Handles key press simulation
class KeySimulator {
private:
    std::string lCurrentKey;
    std::string rCurrentKey;
    bool lHaveKey;
    bool rHaveKey;

public:
    KeySimulator() : lCurrentKey("null"), rCurrentKey("null"), lHaveKey(false), rHaveKey(false) {}
    
    void handleLeftInput(int direction);
    void handleRightInput(int direction);
    void releaseLeftKey();
    void releaseRightKey();
    
private:
    void simulateKeyPress(int keyCode, bool isDown);
    void simulateKeyPress(const std::string& key, bool isDown);
};

// Debug Display - Handles debug information
class DebugDisplay {
private:
    HWND editControl;
    ControllerManager* controllerManager;
    
public:
    DebugDisplay(HWND editControl, ControllerManager* controllerManager) 
        : editControl(editControl), controllerManager(controllerManager) {}
    
    void update(double lAngle, double rAngle, int lDirection, int rDirection, 
                const std::string& lKey, const std::string& rKey, bool lActive, bool rActive);
    
private:
    std::string formatControllerInfo() const;
    std::string formatStickValues() const;
    std::string formatButtonInfo() const;
    std::string formatActiveKeys(const std::string& lKey, const std::string& rKey, bool lActive, bool rActive) const;
};

// Window Manager - Handles GUI creation and management
class WindowManager {
private:
    HWND hwnd;
    HWND editControl;
    
public:
    WindowManager() : hwnd(nullptr), editControl(nullptr) {}
    ~WindowManager() {
        if (hwnd) DestroyWindow(hwnd);
    }
    
    bool create();
    HWND getHandle() const { return hwnd; }
    HWND getEditControl() const { return editControl; }
    
private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void createEditControl();
};

// Main Controller Class - Orchestrates everything
class SimpleController {
private:
    std::unique_ptr<ControllerManager> controllerManager;
    std::unique_ptr<KeySimulator> keySimulator;
    std::unique_ptr<DebugDisplay> debugDisplay;
    std::unique_ptr<WindowManager> windowManager;
    
    ControllerInfo selectedController;
    
public:
    SimpleController() = default;
    
    bool initialize();
    void run();
    
private:
    static std::vector<ControllerInfo> listAllControllers();
    static void displayControllerMenu(const std::vector<ControllerInfo>& controllers);
    static int getControllerSelection(int maxControllers);
    static ControllerInfo selectController();
    
    double calculateAngle(double x, double y) const;
    int getDirection(double angle) const;
    void processControllerInput();
};

// Implementation of ControllerManager
bool ControllerManager::initialize(const ControllerInfo& info, HWND hwnd) {
    selectedController = info;
    
    if (info.type == ControllerType::XInput) {
        return initializeXInput(info.index);
    } else {
        return initializeDirectInput(info.guid, hwnd);
    }
}

bool ControllerManager::initializeXInput(DWORD index) {
    XINPUT_STATE state;
    DWORD result = XInputGetState(index, &state);
    if (result == ERROR_SUCCESS) {
        hasXInputController = true;
        xInputControllerIndex = index;
        return true;
    }
    return false;
}

bool ControllerManager::initializeDirectInput(const GUID& guid, HWND hwnd) {
    if (!di) {
        HRESULT hr = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, 
                                      IID_IDirectInput8, (void**)&di, nullptr);
        if (FAILED(hr)) return false;
    }

    HRESULT hr = di->CreateDevice(guid, &joystick, nullptr);
    if (FAILED(hr)) return false;

    hr = joystick->SetDataFormat(&c_dfDIJoystick);
    if (FAILED(hr)) return false;

    hr = joystick->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
    if (FAILED(hr)) {
        hr = joystick->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
        if (FAILED(hr)) return false;
    }

    hr = joystick->Acquire();
    return SUCCEEDED(hr);
}

StickValues ControllerManager::getStickValues() const {
    StickValues values = {0, 0, 0, 0};
    
    if (hasXInputController) {
        XINPUT_STATE state;
        if (XInputGetState(xInputControllerIndex, &state) == ERROR_SUCCESS) {
            values.x = state.Gamepad.sThumbLX / Constants::STICK_MAX_VALUE;
            values.y = state.Gamepad.sThumbLY / Constants::STICK_MAX_VALUE;
            values.z = state.Gamepad.sThumbRX / Constants::STICK_MAX_VALUE;
            values.r = state.Gamepad.sThumbRY / Constants::STICK_MAX_VALUE;
        }
    } else if (joystick) {
        DIJOYSTATE state;
        if (SUCCEEDED(joystick->GetDeviceState(sizeof(DIJOYSTATE), &state))) {
            values.x = (state.lX / Constants::STICK_NORMALIZE_FACTOR) - 1.0;
            values.y = 1.0 - (state.lY / Constants::STICK_NORMALIZE_FACTOR);
            values.z = (state.lZ / Constants::STICK_NORMALIZE_FACTOR) - 1.0;
            values.r = 1.0 - (state.lRz / Constants::STICK_NORMALIZE_FACTOR);
        }
    }
    
    return values;
}

ButtonState ControllerManager::getButtonState() const {
    ButtonState state = {false, false};
    
    if (hasXInputController) {
        XINPUT_STATE xState;
        if (XInputGetState(xInputControllerIndex, &xState) == ERROR_SUCCESS) {
            state.leftPressed = (xState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
            state.rightPressed = (xState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
        }
    } else if (joystick) {
        DIJOYSTATE diState;
        if (SUCCEEDED(joystick->GetDeviceState(sizeof(DIJOYSTATE), &diState))) {
            state.leftPressed = (diState.rgbButtons[4] & 0x80) != 0;
            state.rightPressed = (diState.rgbButtons[5] & 0x80) != 0;
        }
    }
    
    return state;
}

bool ControllerManager::isConnected() const {
    return hasXInputController || joystick != nullptr;
}

void ControllerManager::cleanup() {
    if (joystick) {
        joystick->Unacquire();
        joystick->Release();
        joystick = nullptr;
    }
    if (di) {
        di->Release();
        di = nullptr;
    }
}

// Implementation of KeySimulator
void KeySimulator::handleLeftInput(int direction) {
    if (lHaveKey) {
        simulateKeyPress(lCurrentKey, true);
    } else {
        lHaveKey = true;
        lCurrentKey = std::to_string(direction + 1);
        simulateKeyPress(lCurrentKey, true);
    }
}

void KeySimulator::handleRightInput(int direction) {
    if (rHaveKey) {
        simulateKeyPress(rCurrentKey, true);
    } else {
        rHaveKey = true;
        rCurrentKey = std::to_string(direction + 1);
        simulateKeyPress(rCurrentKey, true);
    }
}

void KeySimulator::releaseLeftKey() {
    if (lHaveKey) {
        simulateKeyPress(lCurrentKey, false);
        lHaveKey = false;
        lCurrentKey = "null";
    }
}

void KeySimulator::releaseRightKey() {
    if (rHaveKey) {
        simulateKeyPress(rCurrentKey, false);
        rHaveKey = false;
        rCurrentKey = "null";
    }
}

void KeySimulator::simulateKeyPress(int keyCode, bool isDown) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = keyCode;
    input.ki.dwFlags = isDown ? 0 : KEYEVENTF_KEYUP;
    input.ki.dwExtraInfo = GetMessageExtraInfo();
    input.ki.wScan = MapVirtualKey(keyCode, MAPVK_VK_TO_VSC);
    
    SendInput(1, &input, sizeof(INPUT));
    
    if (isDown) {
        Sleep(1);
    }
}

void KeySimulator::simulateKeyPress(const std::string& key, bool isDown) {
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
    
    if (isDown) {
        keybd_event(keyCode, 0, 0, 0);
    } else {
        keybd_event(keyCode, 0, KEYEVENTF_KEYUP, 0);
    }
}

// Implementation of SimpleController
bool SimpleController::initialize() {
    selectedController = selectController();
    if (selectedController.name.empty()) {
        return false;
    }
    
    controllerManager = std::make_unique<ControllerManager>();
    keySimulator = std::make_unique<KeySimulator>();
    windowManager = std::make_unique<WindowManager>();
    
    if (!windowManager->create()) {
        return false;
    }
    
    if (!controllerManager->initialize(selectedController, windowManager->getHandle())) {
        return false;
    }
    
    debugDisplay = std::make_unique<DebugDisplay>(windowManager->getEditControl(), controllerManager.get());
    
    return true;
}

void SimpleController::run() {
    if (!controllerManager || !windowManager) {
        return;
    }
    
    MSG msg = {};
    while (true) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                return;
            }
        }
        
        processControllerInput();
        Sleep(Constants::MAIN_LOOP_SLEEP_MS);
    }
}

void SimpleController::processControllerInput() {
    if (!controllerManager->isConnected()) return;
    
    StickValues sticks = controllerManager->getStickValues();
    ButtonState buttons = controllerManager->getButtonState();
    
    double lAngle = calculateAngle(sticks.x, sticks.y);
    double rAngle = calculateAngle(sticks.z, sticks.r);
    
    int lDirection = getDirection(lAngle);
    int rDirection = getDirection(rAngle);
    
    if (buttons.leftPressed && lDirection != -1) {
        keySimulator->handleLeftInput(lDirection);
    } else {
        keySimulator->releaseLeftKey();
    }
    
    if (buttons.rightPressed && rDirection != -1) {
        keySimulator->handleRightInput(rDirection);
    } else {
        keySimulator->releaseRightKey();
    }
    
    debugDisplay->update(lAngle, rAngle, lDirection, rDirection,
                        keySimulator->lCurrentKey, keySimulator->rCurrentKey,
                        keySimulator->lHaveKey, keySimulator->rHaveKey);
}

double SimpleController::calculateAngle(double x, double y) const {
    if (x == 0 && y == 0) return -1;
    
    double angle = std::atan2(y, x) * 180.0 / Constants::PI;
    if (angle < 0) angle += 360;
    
    angle = angle - 90;
    if (angle < 0) angle += 360;
    
    return 360 - angle;
}

int SimpleController::getDirection(double angle) const {
    if (angle == -1) return -1;
    
    int direction = static_cast<int>(std::floor(angle / Constants::DEGREES_PER_SECTOR));
    return (direction >= Constants::DIRECTION_SECTORS) ? 0 : direction;
}

// Static methods for controller selection
std::vector<ControllerInfo> SimpleController::listAllControllers() {
    std::vector<ControllerInfo> controllers;
    
    // List XInput controllers
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        XINPUT_STATE state;
        if (XInputGetState(i, &state) == ERROR_SUCCESS) {
            controllers.push_back({ControllerType::XInput, "Xbox Controller " + std::to_string(i + 1), i, GUID{}});
        }
    }
    
    // List DirectInput controllers
    LPDIRECTINPUT8 di;
    if (SUCCEEDED(DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, 
                                    IID_IDirectInput8, (void**)&di, nullptr))) {
        di->EnumDevices(DI8DEVCLASS_GAMECTRL, [](LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef) -> BOOL {
            std::vector<ControllerInfo>* controllers = (std::vector<ControllerInfo>*)pvRef;
            controllers->push_back({ControllerType::DirectInput, lpddi->tszProductName, 0, lpddi->guidInstance});
            return DIENUM_CONTINUE;
        }, &controllers, DIEDFL_ATTACHEDONLY);
        di->Release();
    }
    
    return controllers;
}

void SimpleController::displayControllerMenu(const std::vector<ControllerInfo>& controllers) {
    std::cout << "\n=== CONTROLLER SELECTION ===" << std::endl;
    std::cout << "Available controllers:" << std::endl;
    
    for (size_t i = 0; i < controllers.size(); i++) {
        std::string typeStr = (controllers[i].type == ControllerType::XInput) ? "XInput" : "DirectInput";
        std::cout << "[" << (i + 1) << "] " << controllers[i].name << " (" << typeStr << ")" << std::endl;
    }
    
    std::cout << "\nPress the number key (1-" << controllers.size() << ") to select a controller:" << std::endl;
}

int SimpleController::getControllerSelection(int maxControllers) {
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
        Sleep(Constants::SELECTION_SLEEP_MS);
    }
}

ControllerInfo SimpleController::selectController() {
    std::vector<ControllerInfo> controllers = listAllControllers();
    
    if (controllers.empty()) {
        std::cerr << "No compatible controllers found!" << std::endl;
        return {};
    }
    
    displayControllerMenu(controllers);
    int selection = getControllerSelection(controllers.size());
    
    if (selection >= 0 && selection < controllers.size()) {
        std::cout << "Selected: " << controllers[selection].name << std::endl;
        return controllers[selection];
    }
    
    return {};
}

// Main function
int main() {
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
    
    std::cout << "Simple Controller to Maimai" << std::endl;
    std::cout << "Close the program by closing the console" << std::endl;
    std::cout << "Closing the GUI will restart the program" << std::endl;

    while (true) {
        try {
            SimpleController app;
            if (!app.initialize()) {
                std::cerr << "Failed to initialize application!" << std::endl;
                continue;
            }
            app.run();
            
            std::cout << "\n=== WINDOW CLOSED ===" << std::endl;
            std::cout << "Window closed. Returning to controller selection..." << std::endl;
            
            FreeConsole();
            AllocConsole();
            freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
            freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
            
            std::cout << "Simple Controller to Maimai" << std::endl;
            std::cout << "Close the program by closing the console" << std::endl;
            std::cout << "Closing the GUI will restart the program" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            std::cerr << "Press any key to continue or Ctrl+C to exit..." << std::endl;
            _getch();
        }
    }

    FreeConsole();
    return 0;
}
