# Controller Input Wrapper for Sentakki

A C++ controller input wrapper that maps analog sticks to keyboard, mouse, or **TRUE MULTI-TOUCH** inputs with a real-time visual overlay. Designed for playing Sentakki (osu! lazer mod) and potentially other touch-based games with similar control schemes.

Built with Cursor AI assistance.

## Available Versions

### ControllerToTouch.exe ⭐ (RECOMMENDED)
**TRUE multi-touch support using Windows UWP InputInjector API!**

- **No touch hardware required!** Works on any Windows 10/11 PC
- **2 independent touch points** - Both sticks work simultaneously
- Left Bumper + Left Stick = Touch point 0
- Right Bumper + Right Stick = Touch point 1
- **Auto-requests Administrator privileges**

**Important:** Enable touch input in your game's settings (osu! → Settings → Input → Touch → Enable)

### ControllerToMouse.exe  
Maps controller inputs to mouse cursor position with left-click.

- One bumper: Mouse follows that stick + holds LMB
- Both bumpers: Mouse alternates between stick positions every frame
- Mouse returns to screen center when released
- Position is used to decide where the cursor will be

Limited to single cursor (Windows limitation).

### ControllerToNumberKeys.exe
Maps controller inputs to keyboard number keys (1-8) based on stick direction.

- Both bumpers trigger number keys based on stick angle
- Keys dynamically switch as you rotate the stick
- Prevents both sticks from pressing the same key simultaneously
- Angle is used to decide what button to press

**Note:** Set button mapping to 1-8 in Sentakki settings for proper key detection.


## Visual Overlay

All versions include a beautiful transparent overlay showing:

- **Smooth fading effects** - Overlay only appears when sticks are moved
- **Real-time stick positions** (Blue = left stick, Pink = right stick)
- **Angle indicators** - Arc segments showing stick direction
- **Variable pen widths** - Get thicker as sticks move further from center
- **On-screen debug UI** - Toggle with Ctrl+Shift+` (enabled by default)
- **Full-screen overlay** - Spans entire screen for better visibility
- **Dynamic refresh rate** - Matches your monitor's refresh rate

## Requirements

### To Run (Portable!):
- **Windows 10 or 11** (Touch version requires Windows 10+)
- **XInput or DirectInput compatible controller**
- **That's it!** No installation, drivers, or DLLs needed. Just run the .exe!

### To Build from Source:
- Visual Studio 2022 with C++ Desktop Development
- Windows 10 SDK (includes C++/WinRT for Touch version)

## Building

Run the build script to compile all three versions:
```bash
build.bat
```

This compiles:
- `ControllerToNumberKeys.exe`
- `ControllerToMouse.exe`  
- `Touch Version\ControllerToTouch.exe`

All executables are **fully portable** - copy to any Windows 10/11 PC and run!

## Usage

1. **Run the program** (ControllerToTouch.exe recommended!)
2. **Allow admin privileges** when prompted (Touch version only)
3. **Select your controller** from the menu
4. **Don't forget to enable touch in osu!** (for Touch version)
5. The overlay appears automatically

### Controls

**ControllerToTouch (Multi-Touch):**
- Hold **Left Bumper** + move **Left Stick** → Touch point 0
- Hold **Right Bumper** + move **Right Stick** → Touch point 1
- **Both bumpers work simultaneously** for true multi-touch!
- Position the overlay over your game window for accurate touch placement

**ControllerToNumberKeys:**
- Hold bumper + move stick → Triggers keys 1-8 based on direction
- Keys switch as you rotate the stick
- Left Bumper uses left stick angle, Right Bumper uses right stick

**ControllerToMouse:**
- Hold one bumper → Mouse follows that stick, LMB held
- Hold both bumpers → Mouse alternates between sticks every frame
- Release all → Mouse returns to center

**Keyboard Shortcuts (All Versions):**
- `Ctrl+Shift+` ` → Toggle debug info overlay
- `Ctrl+Alt+Shift+` ` → Restart program
- Close console window → Exit program


## Technical Details

### Input APIs:
- **Touch Version:** Windows UWP InputInjector API (C++/WinRT)
  - Enables touch injection without physical hardware
- **Number Keys/Mouse Versions:** DirectInput 8 + XInput 1.4
  - SendInput API for keyboard simulation
  - Mouse events for cursor control

### Rendering:
- GDI for overlay rendering with alpha blending
- Layered windows with color key transparency
- Variable pen widths for smooth fading effects
- Dynamic update rate matching screen refresh (60Hz+)

### Features:
- Auto-detects both XInput and DirectInput devices
- Controller selection menu at startup
- Click-through transparent overlay
- Controller hot-swapping via restart function

## Portability

**All programs are 100% portable!** 

Each `.exe` file is **completely self-contained** and will work anywhere:

- ✅ No installation needed - just copy and run
- ✅ No external DLLs required - only uses Windows system libraries
- ✅ No configuration files - standalone executable
- ✅ No registry modifications
- ✅ Works on any Windows 10/11 PC
- ✅ Copy to USB drive, cloud storage, or share directly

**Touch version additional requirements:**
- Windows 10 or later (uses UWP InputInjector API)
- Administrator privileges (automatically requested on launch)
- Target application must have touch input enabled

## Notes

**Development:**
- Built entirely with Cursor AI assistance
- Tested with DualShock 4 and Xbox 360 controllers
- Works with any XInput or DirectInput compatible controller

**Compatibility:**
- ✅ osu! lazer / Sentakki (primary target - confirmed working!)
- ✅ Other touch-based rhythm games with similar controls (e.g., maimai-style games)
- ⚠️ Some anti-cheat games may block synthetic input

**Author Note:**
Most of the code is written by Cursor AI, as I don't know C++ at all!