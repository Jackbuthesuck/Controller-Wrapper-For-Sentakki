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
- DS4 LED calibration controls: hold D-pad Up with the crosshair over an empty background, then release to capture ambient light; hold D-pad Left over the left lightbar and release to learn its color; hold D-pad Right over the right lightbar and release to learn its color; press D-pad Down to arm circle calibration
- Hand modes: show a peace sign with both hands at their neutral rest depth; hold position until the circle is measured, then release to apply calibration. No controller is required
- DS4 LED mode: press the same calibration control while both blue/red lightbars are visible; their positions immediately define the circle
- `--push-threshold` controls normalized forward depth change; default is `0.025` (roughly 2.5% of the image width, not centimeters)
- Large LED position jumps must appear in consecutive frames before they are accepted, reducing one-frame noise without permanently blocking a real move. Tune `led_jump_confirmations` and `led_jump_match_distance` in `camera_config.json` if needed.
- White LED fallback is disabled by default because bright reflections can be mistaken for lightbars. Enable `allow_white_led_fallback` in `camera_config.json` only when the LEDs are intentionally appearing white and the background is clean.
- Calibration: hold both hands in a peace sign with index and middle extended and ring and pinky curled. Their weighted palm anchors define the play-space circle.
- Calibration uses a circular play area; it is intentionally not treated as a square because DirectInput axis behavior can be unreliable near the corners.
- Controller mapping note: DS4 LED calibration needs a controller mapping that exposes all four D-pad directions, such as XInput or a DirectInput POV hat. If only D-pad Right is exposed by the driver, the older circle-calibration shortcut still works but the ambient/color sampling controls cannot be used.

### DS4 LED Setup

You need a tool such as DS4Windows to set the controller lightbar colors and brightness before starting this mode. DS4Windows is the setup method used during development; `ControllerInput.exe` does not configure the lightbar, it only reads the colors from the camera.

For the most reliable LED tracking, use a dark or dim room with the DS4 lightbars as the brightest colored objects in view. Avoid direct sunlight, mirrors, glossy screens, colored lamps, and bright backgrounds behind the controller. Do not set the lightbar brightness too high: clipped blue or red light becomes overexposed and can appear white, which makes color detection less reliable. Keep the camera fixed and give the blue and red lightbars a clear separation from each other.

In DS4 LED mode, the detector starts with blue/red defaults, then can learn the actual camera colors at runtime. Hold the requested D-pad direction until the crosshair is over the target, then release it. Ambient sampling uses the whole frame's median brightness and saturation; left/right color sampling averages the frames captured while held. The preview reports learned state as `A/L/R` for ambient, left color, and right color. Press D-pad Down to arm the existing circular calibration; it applies as soon as both lightbars are visible.

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

The normal workflow does not require starting Python manually. `ControllerInput.exe` launches the sender when Camera mode is selected. The sender automatically loads `camera_config.json` from the same folder, so normal tuning does not require command-line launch. Command-line options can still override the JSON settings for standalone testing. If the camera still reports about 15 FPS, it likely cannot provide a faster mode at that resolution or is limited by its driver or USB connection.

The config accepts JSONC-style `//` or `/* ... */` comments. The startup selections use the same numeric indices shown by the native menus:

```json
{
	// 0=interactive menus, 1=skip menus and auto-start for debugging.
	"auto_start": false,
	// 0=show the mode menu, 1=camera, 2=touch, 3=keyboard, 4=mouse.
	"startup_mode": 1,
	// 0=show the camera-input menu, 1=DS4 LED, 2=push, 3=open hand.
	"input_mode": 1,
	// 0=show the camera-source menu, 1=webcam, 2=scrcpy window, 3=scrcpy monitor.
	"camera_source": 1,
	"camera_index": 0,
	"preview": true,
	"auto_download_model": true,
	"fps": 60,
	"width": 640,
	"height": 480,
	"led_jump_confirmations": 2,
	"led_jump_match_distance": 0.12,
	"allow_white_led_fallback": false
}
```

Set `auto_start` to `1` to enable config-driven startup, or `0` to keep the normal menus. With auto-start enabled, set any individual choice to `0` to leave only that choice interactive: `startup_mode` values `1-4`, `input_mode` values `1-3`, and `camera_source` values `1-3` select automatically. Use `camera_index` for a webcam device, or set it to `-1` to let the sender list and prompt for a camera. The old string names are still accepted by the native loader and Python sender.

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
cl /EHsc /std:c++20 /c main.cpp ControllerMapper.cpp CameraMode.cpp TouchMode.cpp MouseMode.cpp KeyboardMode.cpp
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
- Loopback UDP only for local camera input; optional model download is controlled by `auto_download_model`
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
