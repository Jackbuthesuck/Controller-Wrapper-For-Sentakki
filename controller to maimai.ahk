#Requires AutoHotkey v2.0
#SingleInstance

ControllerNumber := 0
lCurrentKey := "null"
rCurrentKey := "null"
lHaveKey := false
rHaveKey := false

; Auto-detect the controller number if called for:
if ControllerNumber <= 0 {
    loop 16  ; Query each controller number to find out which ones exist.
    {
        if GetKeyState(A_Index "JoyName") {
            ControllerNumber := A_Index
            break
        }
    }
}

; Failsafe button to exit script
f7:: ExitApp

G := Gui("+AlwaysOnTop", "Controller to Maimai In Osu!")
G.SetFont("s18")
G.Add("Text", "w300",
    " "
)
E := G.Add("Edit", "w300 h300 +ReadOnly")
G.Show()

; Create dynamic hotkey using the detected controller number
loop {
    if (GetKeyState(ControllerNumber "Joy5"))
        LeftRadialMenuHandler()
    else {
        SendInput "{" . (lCurrentKey) . " Up}"
        global lHaveKey := false
        global lCurrentKey := "null"
    }
    if (GetKeyState(ControllerNumber "Joy6"))
        RightRadialMenuHandler()
    else {
        SendInput "{" . (rCurrentKey) . " Up}"
        global rHaveKey := false
        global rCurrentKey := "null"
    }

    JoyX := GetKeyState(ControllerNumber "JoyX")
    JoyY := GetKeyState(ControllerNumber "JoyY")
    JoyZ := GetKeyState(ControllerNumber "JoyZ")
    JoyR := GetKeyState(ControllerNumber "JoyR")

    ; Convert joystick values to degrees
    ; JoyX: left 0, right 100 -> normalize to -1 to 1
    ; JoyY: up 100, down 0 -> normalize to -1 to 1 (inverted)
    normalizedX := (JoyX - 50) / 50  ; -1 to 1
    normalizedY := (50 - JoyY) / 50  ; -1 to 1 (inverted)
    normalizedZ := (JoyZ - 50) / 50  ; -1 to 1
    normalizedR := (50 - JoyR) / 50  ; -1 to 1 (inverted)

    ; Calculate angle in degrees (0-360, with 0 being right, 90 being up)
    if (normalizedX == 0 && normalizedY == 0) {
        lAngle := -1  ; No input
    } else {
        ; Use Atan() function - need to handle quadrants manually
        lAngle := Atan(normalizedY / normalizedX) * 180 / 3.14159265359

        ; Adjust angle based on quadrant
        if (normalizedX < 0) {
            lAngle += 180
        } else if (normalizedY < 0) {
            lAngle += 360
        }

        ; Make clockwise by subtracting from 360 degrees
        lAngle := 360 - lAngle

        ; Apply 90 degree offset, because for some reason "1" is map to buttom right top half
        lAngle += 90
        if (lAngle >= 360) {
            lAngle -= 360
        }
    }

    if (normalizedZ == 0 && normalizedR == 0) {
        rAngle := -1  ; No input
    } else {
        ; Use Atan() function - need to handle quadrants manually
        rAngle := Atan(normalizedR / normalizedZ) * 180 / 3.14159265359

        ; Adjust angle based on quadrant
        if (normalizedZ < 0) {
            rAngle += 180
        } else if (normalizedR < 0) {
            rAngle += 360
        }

        ; Make clockwise by subtracting from 360 degrees
        rAngle := 360 - rAngle

        ; Apply 90 degree offset, because for some reason "1" is map to buttom right top half
        rAngle += 90
        if (rAngle >= 360) {
            rAngle -= 360
        }
    }

    if (lAngle != -1) {
        lDirection := Floor(lAngle / 45)
        if (lDirection >= 8) {
            lDirection := 0  ; Reset to 0 if out of bounds
        }
    }

    if (rAngle != -1) {
        rDirection := Floor(rAngle / 45)
        if (rDirection >= 8) {
            rDirection := 0  ; Reset to 0 if out of bounds
        }
    }

    E.Value := "lAngle: `n" lAngle "`nrAngle: `n" rAngle "`nlDirection: " lDirection "`nrDirection: " rDirection "`nlCurrentKey: " lCurrentKey "`nrCurrentKey: " rCurrentKey "`nlHaveKey: " lHaveKey "`nrHaveKey: " rHaveKey

    Sleep 2
}

LeftRadialMenuHandler(*) {
    if (lHaveKey) {
        SendInput "{" . (lCurrentKey) . " Down}"
    }
    else {
        global lHaveKey := true

        global lDirection := lDirection
        SendInput "{" . (lDirection + 1) . " Down}"  ; Send keys 1-8
        global lCurrentKey := (lDirection + 1)
    }
}

RightRadialMenuHandler(*) {
    if (rHaveKey) {
        SendInput "{" . (rCurrentKey) . " Down}"
    }
    else {
        global rHaveKey := true

        global rDirection := rDirection
        SendInput "{" . (rDirection + 1) . " Down}"  ; Send keys 1-8
        global rCurrentKey := (rDirection + 1)
    }
}
