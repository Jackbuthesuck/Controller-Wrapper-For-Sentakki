#include "ControllerInput.h"

// ========== Keyboard Mode Implementation ==========

int ControllerMapper::getKeyCode(const std::string& key) {
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

void ControllerMapper::simulateKeyPress(int keyCode, bool isDown) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = keyCode;
    input.ki.dwFlags = isDown ? 0 : KEYEVENTF_KEYUP;
    input.ki.wScan = MapVirtualKey(keyCode, MAPVK_VK_TO_VSC);
    input.ki.dwExtraInfo = GetMessageExtraInfo();
    SendInput(1, &input, sizeof(INPUT));
}

void ControllerMapper::sendKeyPress(const std::string& key, bool isDown) {
    int keyCode = getKeyCode(key);
    if (keyCode == 0) return;
    simulateKeyPress(keyCode, isDown);
}

void ControllerMapper::handleKeyboardControl(bool l1, bool r1, double leftX, double leftY, double rightX, double rightY) {
    // Calculate angles and directions
    double lAngle = calculateAngle(leftX, leftY);
    double rAngle = calculateAngle(rightX, rightY);
    int lDirection = getDirection(lAngle);
    int rDirection = getDirection(rAngle);
    
    // Handle L1 + left stick
    if (l1 && lDirection != -1) {
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
        // L1 released or no direction - release key
        sendKeyPress(currentLeftKey, false);
        currentLeftKey = "";
    }
    
    // Handle R1 + right stick
    if (r1 && rDirection != -1) {
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
        // R1 released or no direction - release key
        sendKeyPress(currentRightKey, false);
        currentRightKey = "";
    }
}