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
- L1 → Left mouse button
- R1 → Right mouse button

**Keyboard Mode (Legacy):**
- L1 + Left Stick → Keys 1-8 (left side)
- R1 + Right Stick → Keys 1-8 (right side)

**Camera/UDP Input:**
- Select Camera from `ControllerInput.exe`; it automatically starts `camera_sender.py` and sends input to `127.0.0.1:8765`
- When multiple cameras are detected, press the number beside a camera to select it immediately; names come from Windows when available, with resolution/FPS fallback details
- Left/Right pointer uses a weighted palm anchor: wrist landmark 0, thumb landmark 2 weighted twice, index base 5, and pinky base 17
- Push mode uses forward palm depth relative to the neutral depth captured during calibration
- Open-hand mode uses three or more extended fingers for touch-down; a curled hand is rest
- DS4 LED mode disables hand detection, tracks the bright lightbar, and uses physical DS4 L1/R1 for clicks
- Hand modes: show a peace sign with both hands; hold position until the circle is measured, then release to apply calibration. No controller is required
- DS4 LED mode: press the same calibration control while both blue/red lightbars are visible; their positions immediately define the circle
- `--push-threshold` controls normalized forward depth change; default is `0.04` (roughly 4% of the image width, not centimeters)
- Calibration: hold both hands in a peace sign with index and middle extended and ring and pinky curled. Their weighted palm anchors define the play-space circle.
- Calibration uses a circular play area; it is intentionally not treated as a square because DirectInput axis behavior can be unreliable near the corners.
- Controller mapping note: D-pad Right is the only D-pad direction considered reliably mapped. Use it for calibration or menu actions where applicable.

### DS4 LED Setup

You need a tool such as DS4Windows to set the controller lightbar colors and brightness before starting this mode. DS4Windows is the setup method used during development; `ControllerInput.exe` does not configure the lightbar, it only reads the colors from the camera.

For the most reliable LED tracking, use a dark or dim room with the DS4 lightbars as the brightest colored objects in view. Avoid direct sunlight, mirrors, glossy screens, colored lamps, and bright backgrounds behind the controller. Do not set the lightbar brightness too high: clipped blue or red light becomes overexposed and can appear white, which makes color detection less reliable. Keep the camera fixed and give the blue and red lightbars a clear separation from each other.

Stand at the same distance you expect to use during play. Select `DS4 LED + L1/R1` in `ControllerInput.exe`; it starts the sender automatically. Calibrate while both lightbars are visible. The preview should show both detected markers and report capture/send rates near the requested 60 FPS. If the rates are low, reduce camera resolution or close other camera-using applications before changing detection settings.

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

## Camera Sender (Python, Optional)

The normal workflow does not require starting Python manually. `ControllerInput.exe` launches the sender when Camera mode is selected. Run it manually only for troubleshooting, standalone camera testing, or when the executable reports that it could not start the sender.

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
