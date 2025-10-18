# Controller Input Wrapper for Sentakki

A C++ controller input wrapper that maps analog sticks to keyboard or mouse inputs with a real-time visual overlay. Designed for playing Sentakki (Osu! lazer mod).

Built with Cursor AI assistance.

## Available Versions

### ControllerToNumberKeys.exe
Maps controller inputs to keyboard number keys (1-8) based on stick direction.

- Both bumpers trigger number keys based on stick angle
- Keys dynamically switch as you rotate the stick
- Prevents both sticks from pressing the same key simultaneously
- Angle is used to decide what button to press.

### ControllerToMouse.exe  
Maps controller inputs to mouse cursor position with left-click.

- One bumper: Mouse follows that stick + holds LMB
- Both bumpers: Mouse alternates between stick positions every frame
- Mouse returns to screen center when released
- Position is used to decide where the cursor will be.

Will be scuffed as windows only allow for 1 cursor.

## Visual Overlay

Both versions include a transparent overlay showing:

- The overlay will only appear if the stick is moved.
- Real-time stick positions (Blue = left, Pink = right)
- Angle indicators as arc segments
- Optional debug information display
- The Overlay is fixed to 90% of screen height

## Requirements

- Windows 10/11 (other windows may work. I don't know)
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
- Left Bumper use the angle of the left stick. Right use right.

**ControllerToMouse:**
- Hold one bumper → Mouse follows that stick, LMB held
- Hold both bumpers → Mouse alternates between sticks every frame
- Left Bumper use the position of the left stick. Right use right.
- Release all → Mouse returns to center

**Keyboard Shortcuts:**
- `Ctrl+Shift+` ` → Toggle debug info
- `Ctrl+Alt+Shift+` ` → Restart program
- Close console → Exit program


## Technical Details

- DirectInput 8 + XInput 1.4 for controller input
- GDI for overlay rendering
- Auto-detects both XInput and DirectInput devices
- Updates at screen refresh rate
- Layered windows with color key transparency

## Notes

Program
- Touch version (ControllerToTouch.exe) was attempted but requires touch hardware/drivers
- Use ControllerToMouse.exe as an alternative
- Both programs support controller hot-swapping via restart function

Me
- I only have Dualshock 4 and Xbox 360 controller to test.
- Most of the code is written by cursor, as I don't know C++ at all.