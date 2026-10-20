# Controller Wrapper For Sentakki

Controller Wrapper For Sentakki turns controller, camera, and hand-tracking input into touch, mouse, or keyboard controls for Sentakki and other compatible applications.

## Features

- Touch input through Windows `InputInjector`
- DirectInput and XInput controller support
- Camera tracking through MediaPipe
- DS4 lightbar tracking with physical L1/R1 clicks
- Mouse and keyboard output modes
- Circular play-area calibration
- Optional scrcpy window or monitor capture
- Configurable startup and camera settings through JSONC

## Requirements

- Windows 10 or Windows 11
- Visual Studio with C++ Desktop Development, when building from source
- A DirectInput or XInput controller for controller-based modes
- A webcam, HDMI capture device, or Android scrcpy source for camera modes
- Python 3.10 or newer for camera tracking

For DS4 lightbar tracking:

- Two visible DS4 lightbars
- DS4Windows or another tool that can set their colors and brightness
- A camera positioned so both lightbars remain visible

## Quick Start

1. Install the Python dependencies:

   ```powershell
   python -m pip install -r requirements.txt
   ```

2. Build the native application, or use an existing `ControllerInput.exe`:

   ```powershell
   .\build.bat
   ```

3. Run `ControllerInput.exe`.

4. Choose an input mode and follow the on-screen prompts.

The native application starts `camera_sender.py` automatically when Camera Mode is selected. Debug builds use the repository-root `camera_config.json`; Release builds use the copy beside the executable. Standalone sender runs use the config beside `camera_sender.py` unless `--config` is provided.

## Sentakki Setup

In osu! lazer, enable touch input:

`Settings -> Input -> Touch -> Enable`

Use 100% UI scaling when testing the touch overlay.

## Input Modes

### Touch Mode

- Stick: aim
- L1/R1: touch
- L2/R2: slide note path locking
- L3/R3: palm touch

### Mouse Mode

- Left stick: cursor position
- L1: left mouse button
- R1: right mouse button

### Keyboard Mode

- L1 + left stick: keys 1-8 on the left side
- R1 + right stick: keys 1-8 on the right side

### Camera Mode

Camera mode supports three hand-tracking styles and DS4 LED tracking:

- `DS4 LED`: tracks two colored lightbars; physical L1/R1 provide clicks
- `Push`: uses forward palm depth for clicks
- `Open`: uses three or more extended fingers for clicks

The camera pointer is calibrated to a circular play area. This prevents unreliable corner behavior when the output is mapped to controller-style axes.

### Hand Calibration

Hand modes use a two-hand peace-sign gesture to define the circular play area:

1. Show a peace sign with both hands at your neutral playing position. The sender detects this automatically.
2. Stop showing the peace signs. Releasing the gesture immediately captures and applies the calibration.

Use the controller calibration command, only when you want to manually re-arm calibration.

Both hands must be visible, with the index and middle fingers extended and the ring and pinky fingers curled.

## DS4 LED Tracking

Use DS4Windows to set the left and right lightbars to distinct colors before starting the application. Keep the camera fixed and avoid bright reflections, mirrors, direct sunlight, and colored lights behind the controller.

The runtime calibration controls are sent from the controller:

- L1/R2 : Touch down
- D-pad Up: hold over an empty background, then release to sample ambient light
- D-pad Left: hold over the left lightbar, then release to sample its color
- D-pad Right: hold over the right lightbar, then release to sample its color
- D-pad Down: arm circular calibration while both lightbars are visible

The learned profiles last for the current sender session. Use moderate lightbar brightness; overexposed LEDs can appear white and become harder to distinguish from reflections.

## Keyboard Shortcuts

- `Ctrl+Shift+Q`: show or hide debug information
- `Ctrl+Alt+Shift+Q`: restart the mapper
- `Ctrl+Alt+Shift+W`: move the overlay to the monitor containing the cursor
- `C` in the camera preview: manually re-arm hand-mode circular calibration
- `Q` in the camera preview: quit the camera sender

Monitor switching is manual. Move the cursor to the desired monitor, then press the shortcut.

## Camera Configuration

Edit the included `camera_config.json` for persistent settings. It accepts JSONC comments using `//` and `/* ... */`.

The startup choices use numeric menu values:

- `auto_start`: `0` keeps the menus; `1` enables config-driven startup
- `startup_mode`: `0` interactive, `1` camera, `2` touch, `3` keyboard, `4` mouse
- `input_mode`: `0` interactive, `1` DS4 LED, `2` push, `3` open palm
- `camera_source`: `0` interactive, `1` webcam, `2` scrcpy window, `3` scrcpy monitor
- `camera_index`: webcam index, or `-1` to list cameras and choose interactively

Other useful settings include `preview`, `fps`, `width`, `height`, `tracking_width`,
`tracking_height`, `capture_format`,
`flip_horizontal`, `led_jump_confirmations`, `led_jump_match_distance`, and
`led_size_smoothing`, `allow_white_led_fallback`.

LED detection can use either HSV or RGB thresholds:

```json
"led_color_space": "hsv",
"left_led_color": {
	"hue": 60,
  "hue_tolerance": 18,
  "min_saturation": 100,
  "min_value": 35,
	"color": [0, 255, 0],
  "tolerance": 80
},
"right_led_color": {
  "hue": 120,
  "hue_tolerance": 18,
  "min_saturation": 150,
  "min_value": 55,
  "color": [0, 0, 255],
  "tolerance": 80
}
```

HSV uses `hue`, `hue_tolerance`, `min_saturation`, and `min_value`. The included
testing defaults are green on the left and blue on the right. RGB uses the
8-bit `[red, green, blue]` `color`, `tolerance`, and `min_value`. To use DS4Windows-style
RGB values, set `led_color_space` to `"rgb"`; runtime LED color sampling is intended
for HSV mode.

For DS4 LED mode, `led_size_smoothing` controls how quickly the preview blob follows
changes in the detected lightbar size. Lower values reduce visible pulsing; `0.2` is
a good starting point. Runtime color samples override the defaults, which assume red
on the left and blue on the right.

With `auto_start` enabled, setting an individual choice to `0` leaves only that choice interactive. Use `camera_index: -1` to list available cameras and select one interactively. The list uses `cv2-enumerate-cameras` and displays Windows device names; the selected index is the ordinary index for the backend shown by the enumerator.

To list camera names without starting the native wrapper:

```powershell
python camera_sender.py --list-cameras
```

Install the dependencies first with `python -m pip install -r requirements.txt`. If `cv2-enumerate-cameras` is unavailable, the sender falls back to generic OpenCV camera probing and can only display names such as `Camera 0`.

### Webcam Sources

For a typical webcam, start with the resolution and format supported by the device. A common low-latency starting point is:

```json
"width": 640,
"height": 480,
"fps": 60,
"capture_format": "MJPG",
"flip_horizontal": true
```

If the image appears mirrored, change `flip_horizontal`. The sender prints the negotiated resolution, FPS, and pixel format at startup. The measured capture and send FPS are more reliable than the requested camera FPS.

For hand modes, `tracking_width` and `tracking_height` control only the frame sent to
MediaPipe. The camera and preview can remain at a higher resolution. The default
`640x360` tracking frame reduces inference cost while preserving two-hand tracking;
`max_hands` remains `2`.

MediaPipe Pose/body tracking is not enabled by default. It could provide torso and arm
context, but it would add another model and would not restore hand landmarks that are
lost because the hand is too dark or blurred. Improving lighting, exposure, and hand
contrast is usually more effective; pose tracking can be considered later as a fallback
for predicting a missing hand position.

### HDMI Capture Devices

HDMI capture devices usually appear as ordinary camera indices. Start with a supported mode and keep the source in its native orientation:

```json
"camera_index": 1,
"width": 1280,
"height": 720,
"fps": 60,
"capture_format": "auto",
"flip_horizontal": false
```

The device may expose `YUY2`, `NV12`, `MJPG`, or another format. Use `capture_format: "auto"` when unsure. H.264 may work with some devices; H.265 and H.266 are generally not useful capture formats for this OpenCV path. Trust the negotiated format printed by the sender.

## Standalone Camera Sender

The native application normally launches the sender for you. For camera-only testing:

```powershell
python camera_sender.py --preview
```

List available camera indices:

```powershell
python camera_sender.py --list-cameras
```

Useful overrides:

```powershell
python camera_sender.py --preview --camera-index 1 --capture-format auto --no-flip
python camera_sender.py --preview --log --log-interval 0.5
```

If the installed MediaPipe package uses the tasks API, the sender can download the model when `auto_download_model` is enabled. Internet access is required for the download.

## Building From Source

Build with:

```powershell
.\build.bat
```

The manual Visual Studio commands are:

```powershell
cl /EHsc /std:c++20 /c main.cpp ControllerMapper.cpp CameraMode.cpp TouchMode.cpp MouseMode.cpp KeyboardMode.cpp
link main.obj ControllerMapper.obj TouchMode.obj MouseMode.obj KeyboardMode.obj dinput8.lib dxguid.lib xinput.lib user32.lib gdi32.lib msimg32.lib windowsapp.lib /out:ControllerInput.exe
```

## Project Layout

- `main.cpp`: startup, mode selection, and configuration loading
- `ControllerMapper.cpp`: controller processing, overlay, shortcuts, and monitor selection
- `CameraMode.cpp`: camera sender startup and UDP control
- `TouchMode.cpp`: Windows touch injection
- `MouseMode.cpp`: mouse output
- `KeyboardMode.cpp`: keyboard output
- `camera_sender.py`: camera capture, detection, calibration, preview, and UDP packets
- `camera_config.json`: JSONC camera and startup configuration

## Troubleshooting

**The camera runs below the requested FPS**

Check the negotiated startup values and measured preview FPS. Try a lower resolution, `capture_format: "MJPG"` for a webcam, or `capture_format: "auto"` for a capture card.

**DS4 LEDs are not detected**

Set distinct lightbar colors, reduce brightness, darken the background, and keep both bars separated in the camera view. Use the ambient and per-lightbar calibration controls before changing detection thresholds.

**The overlay is on the wrong monitor**

Move the cursor to the desired monitor and press `Ctrl+Alt+Shift+W`.

**The camera sender does not start**

Install the dependencies with `python -m pip install -r requirements.txt`, confirm that the camera index is correct, and run `python camera_sender.py --list-cameras`.

## License

MIT License - Copyright (c) 2025 WazuHonde / Jackbuthesuck
