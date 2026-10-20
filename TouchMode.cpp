#include "ControllerInput.h"

// ========== Touch Mode Implementation ==========

void ControllerMapper::sendBothTouchesIfActive(double leftX, double leftY, double rightX, double rightY,
                             double leftLockedX, double leftLockedY, bool leftLocked,
                             double rightLockedX, double rightLockedY, bool rightLocked) {
    // Only send both touches if both are active
    if (!leftTouchActive || !rightTouchActive) return;
    if (!inputInjectorInitialized || !inputInjector) return;
    
    // Calculate final send positions (locked or unlocked)
    double leftSendX = leftLocked ? leftLockedX : leftX;
    double leftSendY = leftLocked ? leftLockedY : leftY;
    double rightSendX = rightLocked ? rightLockedX : rightX;
    double rightSendY = rightLocked ? rightLockedY : rightY;
    
    // Update touch tracking for overlay (same as sendTouch does)
    if (0 >= 0 && 0 < 20) {
        touchX[0] = leftSendX;
        touchY[0] = leftSendY;
    }
    if (1 >= 0 && 1 < 20) {
        touchX[1] = rightSendX;
        touchY[1] = rightSendY;
    }

    try {
        std::vector<InjectedInputTouchInfo> touchData;

        // Add left touch
        touchData.push_back(createTouchInfo(0, leftSendX, leftSendY, false, false));

        // Add right touch
        touchData.push_back(createTouchInfo(1, rightSendX, rightSendY, false, false));

        // Send both touches together
        sendMultipleTouches(touchData);
    } catch (hresult_error const& ex) {
        static int errorCount = 0;
        if (errorCount < 3) {
            std::wcout << L"[Touch] Multi-touch failed: " << ex.message().c_str() << std::endl;
            std::cout << "Error: 0x" << std::hex << ex.code() << std::dec << std::endl;
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
    
    // Handle L1 - touch ID 0 (disabled when L3 is active)
    if (!l3TouchActive) {
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
        }
        
        // Handle L2 movement while held
        if (leftTouchActive && l2) {
            // Use helper function to handle movement updates with locking logic
            handleTouchMovementUpdate(0, leftTouchActive, l1 || l2, l2,
                                     currentLHeldDirection, leftPointerLocked, leftLockedDirection,
                                     leftX, leftY, lAngle, lDirection,
                                     rightTouchActive); // Skip if right is also active
        }
    }
    
    // Handle R1 - touch ID 1 (disabled when R3 is active)
    if (!r3TouchActive) {
        if (r1Pressed && !rightTouchActive) {
            rightTouchActive = true;
            sendTouch(1, rightX, rightY, true, false); // Touch down - only send once on press
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
        }
        
        // Handle R2 movement while held
        if (rightTouchActive && r2) {
            // Use helper function to handle movement updates with locking logic
            handleTouchMovementUpdate(1, rightTouchActive, r1 || r2, r2,
                                     currentRHeldDirection, rightPointerLocked, rightLockedDirection,
                                     rightX, rightY, rAngle, rDirection,
                                     leftTouchActive); // Skip if left is also active
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
    
    // Handle L3 as a large single touch and lock out L1/L2 while active
    if (l3Pressed && !l3TouchActive) {
        l3TouchActive = true;
        // Release L1/L2 if they're active (L3 takes priority)
        if (leftTouchActive) {
            sendTouch(0, leftX, leftY, false, true); // Touch up for L1/L2
            leftTouchActive = false;
            leftPointerLocked = false;
            leftLockedDirection = -1;
            currentLHeldDirection = -1;
        }
        sendTouch(0, leftX, leftY, true, false, LARGE_TOUCH_CONTACT_RADIUS_PIXELS);
    }
    
    // Send updates every frame while L3 is held
    if (l3TouchActive && l3) {
        sendTouch(0, leftX, leftY, false, false, LARGE_TOUCH_CONTACT_RADIUS_PIXELS);
    }
    
    if (l3Released && l3TouchActive) {
        sendTouch(0, leftX, leftY, false, true, LARGE_TOUCH_CONTACT_RADIUS_PIXELS);
        l3TouchActive = false;
    }
    
    // Disable L1/L2 when L3 is active
    if (l3TouchActive) {
        // L1/L2 are locked out when L3 is active
    } else {
        // Normal L1/L2 handling (already done above)
    }
    
    // Handle R3 as a large single touch and lock out R1/R2 while active
    if (r3Pressed && !r3TouchActive) {
        r3TouchActive = true;
        // Release R1/R2 if they're active (R3 takes priority)
        if (rightTouchActive) {
            sendTouch(1, rightX, rightY, false, true); // Touch up for R1/R2
            rightTouchActive = false;
            rightPointerLocked = false;
            rightLockedDirection = -1;
            currentRHeldDirection = -1;
        }
        sendTouch(1, rightX, rightY, true, false, LARGE_TOUCH_CONTACT_RADIUS_PIXELS);
    }
    
    // Send updates every frame while R3 is held
    if (r3TouchActive && r3) {
        sendTouch(1, rightX, rightY, false, false, LARGE_TOUCH_CONTACT_RADIUS_PIXELS);
    }
    
    if (r3Released && r3TouchActive) {
        sendTouch(1, rightX, rightY, false, true, LARGE_TOUCH_CONTACT_RADIUS_PIXELS);
        r3TouchActive = false;
    }
    
    // Disable R1/R2 when R3 is active
    if (r3TouchActive) {
        // R1/R2 are locked out when R3 is active
    } else {
        // Normal R1/R2 handling (already done above)
    }
    
    // Update previous button states
    prevL1 = l1;
    prevR1 = r1;
    prevL2 = l2;
    prevR2 = r2;
    prevL3 = l3;
    prevR3 = r3;
}

