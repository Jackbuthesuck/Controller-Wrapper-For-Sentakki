# Controller Input Wrapper for Sentakki

A C++ controller input wrapper that maps analog sticks to keyboard or mouse inputs with a real-time visual overlay. Designed for playing Sentakki (Osu! lazer mod).

Built with Cursor AI assistance.

## Available Versions

### ControllerToNumberKeys.exe
Maps controller inputs to keyboard number keys (1-8) based on stick direction.

- Both bumpers trigger number keys based on stick angle
- Keys dynamically switch as you rotate the stick
- Prevents both sticks from pressing the same key simultaneously

### ControllerToMouse.exe  
Maps controller inputs to mouse cursor position with left-click.

- One bumper: Mouse follows that stick + holds LMB
- Both bumpers: Mouse alternates between stick positions every frame
- Mouse returns to screen center when released

## Visual Overlay

Both versions include a transparent overlay showing:

- Real-time stick positions (Blue = left, Pink = right)
- Angle indicators as arc segments
- Boundary circle (visible only when sticks move)
- Fade effects based on stick distance from center
- Optional debug information display
- Full-screen width, 90% height
- Click-through, doesn't interfere with other windows

## Requirements

- Windows 10/11
- Visual Studio 2022 (for building)
- XInput or DirectInput compatible controller

## Building

Run the batch file:
```bash
build.bat
```

This compiles both programs.

## Usage

1. Run `ControllerToNumberKeys.exe` or `ControllerToMouse.exe`
2. Select your controller from the menu
3. The overlay appears automatically

### Controls

**ControllerToNumberKeys:**
- Hold bumper + move stick → Triggers keys 1-8 based on direction
- Keys switch as you rotate the stick

**ControllerToMouse:**
- Hold one bumper → Mouse follows that stick, LMB held
- Hold both bumpers → Mouse alternates between sticks every frame
- Release all → Mouse returns to center

**Keyboard Shortcuts:**
- `Ctrl+Shift+` ` → Toggle debug info
- `Ctrl+Alt+Shift+` ` → Restart program
- Close console → Exit program

## Direction Mapping

```
     1   2
   8       3
   7       4
     6   5
```

Each direction corresponds to a 45-degree sector starting from top (0°) going clockwise.

## Technical Details

- DirectInput 8 + XInput 1.4 for controller input
- GDI for overlay rendering
- Auto-detects both XInput and DirectInput devices
- Updates at screen refresh rate
- Layered windows with color key transparency

## Features

**Dynamic Key Switching (NumberKeys):**
- Keys automatically switch when rotating stick while holding bumper
- Old key releases, new key presses

**Alternating Mode (Mouse):**
- When both bumpers pressed, mouse rapidly switches between stick positions
- Can simulate multi-touch behavior in some applications

**Fade System:**
- Elements fade from invisible to visible based on stick distance from center
- 0% distance = invisible
- 50%+ distance = full visibility
- Uses pen width modulation to avoid color artifacts

## Troubleshooting

**No controller detected:**
- Check controller connection
- Verify drivers are installed

**Overlay not appearing:**
- Ensure program is running (console window visible)
- Try restarting with keyboard shortcut

**Input not working in game:**
- Some games require administrator privileges
- Right-click exe → "Run as administrator"

## Notes

- Touch version (ControllerToTouch.exe) was attempted but requires touch hardware/drivers
- Use ControllerToMouse.exe as an alternative
- Both programs support controller hot-swapping via restart function
