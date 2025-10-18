# Controller to Maimai for Sentakki - C++ Version

This is a C++ port of the AutoHotkey script that maps game controller inputs to keyboard keys for playing Maimai in the Sentakki mod for Osu! lazer.

This program is written with the help of Cursor.

## Features

- **Auto-controller detection**: Automatically detects connected XInput controllers
- **Radial menu mapping**: Maps joystick positions to keyboard keys (1-8) in a radial pattern
- **Dual joystick support**: Uses left and right joysticks independently
- **Real-time debug display**: Shows current angles, directions, and key states

## Requirements

- Windows 10/11
- Visual Studio 2019 or later (or compatible compiler)
- CMake 3.16 or later
- DirectInput Controller. Ie. Dualshock 4

## Building

### Using the provided batch file:
```bash
build.bat
```

### Manual build:
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

## Usage

1. Connect your controller (XInput or DirectInput)
2. Run the executable: `ControllerToMaimai.exe`
3. The program will auto-detect your controller and show a debug window
4. Use the left and right shoulder buttons (LB/RB) to activate radial menus
5. Move the joysticks to select different keys (1-8)
6. Press F7 to exit

## Controller Mapping

- **Left Joystick + LB**: Maps to keys 1-8 based on joystick direction
- **Right Joystick + RB**: Maps to keys 1-8 based on joystick direction
- **F7**: Exit program

## Key Layout (Radial Pattern)

```
  8   1
7       2
6       3
  5   4
```

## Technical Details

- Uses XInput API for controller input
- Uses Windows SendInput API for keyboard simulation
- Native Windows GUI for debug display
- Real-time processing with 2ms sleep intervals

## Differences from AHK Version

- Uses XInput instead of DirectInput (better Xbox controller support)
- Native C++ performance
- More robust error handling
- Cross-platform potential (currently Windows-only)

## Troubleshooting

- **No controller detected**: Ensure your controller is XInput-compatible and properly connected
- **Keys not working**: Run as administrator if needed for SendInput to work in some games
- **Build errors**: Ensure you have Visual Studio with C++ development tools installed