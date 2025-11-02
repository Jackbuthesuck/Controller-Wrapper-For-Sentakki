#include "ControllerInput.h"

// ========== Mouse Mode Implementation ==========

void ControllerMapper::moveMouseToCenter() {
    // Center cursor on the detected monitor
    int centerX = monitorLeft + monitorWidth / 2;
    int centerY = monitorTop + monitorHeight / 2;
    SetCursorPos(centerX, centerY);
}

void ControllerMapper::moveMouseToStickPosition(double stickX, double stickY) {
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

void ControllerMapper::sendMouseButton(bool down) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
}

void ControllerMapper::handleMouseControl(bool l1, bool r1, double leftX, double leftY, double rightX, double rightY) {
    bool anyButtonPressed = l1 || r1;
    bool bothPressed = l1 && r1;
    bool prevAnyPressed = prevL1 || prevR1;
    
    // Press LMB when L1 or R1 is first pressed
    if (anyButtonPressed && !prevAnyPressed) {
        sendMouseButton(true);
        mouseButtonPressed = true;
        std::cout << "L1/R1 pressed: LMB activated" << std::endl;
    }
    
    // Release LMB when all buttons are released
    if (!anyButtonPressed && prevAnyPressed) {
        sendMouseButton(false);
        mouseButtonPressed = false;
        moveMouseToCenter();
        std::cout << "L1/R1 released: LMB released, mouse to center" << std::endl;
    }
    
    // Handle mouse positioning
    if (bothPressed) {
        // Both L1/R1 pressed - alternate between sticks every frame
        alternateFrame = !alternateFrame;
        if (alternateFrame) {
            moveMouseToStickPosition(leftX, leftY);
        } else {
            moveMouseToStickPosition(rightX, rightY);
        }
    } else if (l1) {
        // Only L1 - follow left stick
        moveMouseToStickPosition(leftX, leftY);
    } else if (r1) {
        // Only R1 - follow right stick
        moveMouseToStickPosition(rightX, rightY);
    }
    
    prevL1 = l1;
    prevR1 = r1;
}