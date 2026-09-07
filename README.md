# Controller Input Mapper

A C++ controller input mapper with Touch, Mouse, Keyboard, and Camera/UDP input support. Works with DirectInput/XInput and also supports running with no physical controller.

Designed for playing Sentakki (osu! lazer mod) and anything else if controls apply.

I love maimai.

## Quick Start

1. Run `ControllerInput.exe`
2. Select your mode in the startup menu.
3. If no controller is connected, the app can continue and accept external camera input over UDP.

**Requirements:**
- Windows 10 or 11
- Optional: XInput or DirectInput controller

**osu! lazer settings:**
- Enable touch input: Settings → Input → Touch → Enable
- Use 100% UI scaling

---

## Controls

**Touch Mode (Recommended):**
- Stick → Aim
- L1/R1 → Touch
- L2/R2 → Slide Note Path Locking (Currently only support 90 degree and 45 degree streight slide, hold the trigger then treat them as if they were edge slide)
- L3/R3 → Palm Touch (for touch note and such)

**Mouse Mode (Legacy):**
- Left Stick → Cursor position
- LB → Left mouse button
- RB → Right mouse button

**Keyboard Mode (Legacy):**
- LB + Left Stick → Keys 1-8 (left side)
- RB + Right Stick → Keys 1-8 (right side)

**Camera/UDP Input:**
- Run `camera_sender.py` to send hand positions to `127.0.0.1:8765`
- Left/Right hand index fingertip controls Left/Right pointer
- Gesture `index_tip.y < middle_tip.y - 0.04` maps to press state

**Shortcuts:**
- `Ctrl+Shift+~` → Toggle debug info, Will also hide the touch IDs on the overlay
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

## Camera Sender (Python)

Install dependencies:

```bash
pip install -r requirements.txt
```

Run sender with preview:

```bash
python camera_sender.py --preview
```

Run sender with console packet logs:

```bash
python camera_sender.py --preview --log --log-interval 0.5
```

If your `mediapipe` package is tasks-only (no `mp.solutions`), run with auto model download:

```bash
python camera_sender.py --preview --auto-download-model
```

Then run `ControllerInput.exe` and select mode `4` (Camera Mode), or any mode while no controller is attached.

**Manual build:**
```bash
cl /EHsc /std:c++20 /c main.cpp ControllerMapper.cpp TouchMode.cpp MouseMode.cpp KeyboardMode.cpp
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
