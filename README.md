# Controller Input Mapper

A C++ controller input mapper with three modes: Touch, Mouse, and Keyboard. Works with any XInput (current version had'nt been tested) or DirectInput controller.

Designed for playing Sentakki (osu! lazer mod) and anything else if controls apply.

Now with multi-screen support (Why not (this took me 2 days)).
---


## Quick Start

1. Run `ControllerInput.exe`
2. Select your mode (1-3)
3. Choose your controller
4. Play!

**Requirements:**
- Windows 10 or 11
- XInput or DirectInput controller

**osu! lazer settings:**
- Enable touch input: Settings → Input → Touch → Enable
- Use 100% UI scaling

---

## Controls

**Touch Mode (Recommended):**
- Stick → Aim
- L1/R1 → Touch
- L2/R2 → Slide note path locking (Currently only support 90 degree and 45 degree streight slide, hold the trigger then treat them as if they were edge slide)
- L3/R3 → Wide touch (for touch note and such)

**Mouse Mode (Legacy):**
- Left Stick → Cursor position
- LB → Left mouse button
- RB → Right mouse button

**Keyboard Mode (Legacy):**
- LB + Left Stick → Keys 1-8 (left side)
- RB + Right Stick → Keys 1-8 (right side)

**Shortcuts:**
- `Ctrl+Shift+~` → Toggle debug info
- `Ctrl+Alt+Shift+~` → Restart

---

## Building

**Requirements:**
- Visual Studio 2022 with C++ Desktop Development
- Windows 10 SDK

**Build:**
```bash
build.bat
```

**Manual build:**
```bash
cl /EHsc /std:c++17 /await /c main.cpp ControllerMapper.cpp TouchMode.cpp MouseMode.cpp KeyboardMode.cpp
link main.obj ControllerMapper.obj TouchMode.obj MouseMode.obj KeyboardMode.obj dinput8.lib dxguid.lib xinput.lib user32.lib gdi32.lib msimg32.lib windowsapp.lib /out:ControllerInput.exe
```

**Note:** The code is split into multiple files:
- `main.cpp` - Entry point and mode selection
- `ControllerMapper.cpp` - Core controller logic, GUI, overlay rendering
- `TouchMode.cpp` - Touch input implementation
- `MouseMode.cpp` - Mouse input implementation  
- `KeyboardMode.cpp` - Keyboard input implementation
- `ControllerInput.h` - Header with all declarations

---

## Security

This program uses input injection APIs and may be flagged by antivirus software. It's safe to use:

- Open source code
- No network activity
- No system modifications
- Portable executable

If flagged, add to antivirus exclusions or build from source.

---

## Technical Details

**APIs:**
- Touch: Windows UWP InputInjector
- Mouse/Keyboard: SendInput API
- Controller: DirectInput 8 + XInput 1.4

**Rendering:**
- GDI overlay

## License

MIT License - Copyright (c) 2025 WazuHonde / Jackbuthesuck
