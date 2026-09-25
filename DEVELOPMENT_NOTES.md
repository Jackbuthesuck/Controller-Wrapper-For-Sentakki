# Development Notes

## Current Status

The project builds as a Visual Studio C++ application and starts the Python camera sender automatically in Camera Mode. The sender supports:

- OpenCV webcam and HDMI capture-device input.
- Optional scrcpy window and monitor capture.
- MediaPipe hand tracking for palm, push, and open-hand modes.
- DS4 lightbar tracking with HSV or RGB color masks.
- Runtime ambient and per-lightbar HSV color sampling.
- Separate camera capture and MediaPipe tracking resolutions.
- Named camera discovery through `cv2-enumerate-cameras`.

The Canon EOS 60D is connected through a capture device and appears to Windows/OpenCV as a normal camera source.

## Conditions

### Current Test Configuration

The root `camera_config.json` currently uses a `1280x720` camera stream and a smaller `640x360` MediaPipe tracking frame:

```json
{
  "auto_start": 1,
  "startup_mode": 1,
  "input_mode": 1,
  "camera_source": 1,
  "camera_index": 0,
  "width": 1280,
  "height": 720,
  "tracking_width": 640,
  "tracking_height": 360,
  "led_color_space": "hsv"
}
```

The included testing color defaults are green on the left and blue on the right. The colors and thresholds can be changed under `left_led_color` and `right_led_color`. RGB mode can be selected with `"led_color_space": "rgb"`.

### Hardware Conditions Owned by the Head Developer

These are hardware and capture-environment constraints, not software defects:

- The room is nearly black.
- The Canon EOS 60D is being used at approximately `1/60` shutter speed for 60 FPS capture.
- The available lens is approximately `18-55mm f/3.5-5.5`.
- ISO is intentionally kept low to limit noise.

The sender prints the negotiated stream resolution, FPS, and pixel format. Those values are authoritative; configuration values are targets.

## Problems

### Camera Recording Indicator False Positive

The previous red-detection concern was misdiagnosed. The red DualShock lightbar was not the root problem. The EOS 60D places a red recording indicator at the edge of the captured frame, and that indicator can become the strongest red object in the image.

The detector then selects the recording indicator as the best red candidate instead of the intended lightbar. This is a software false-positive problem caused by a camera UI overlay, not a failure of the red color model or necessarily a weak DualShock LED.

Potential remedies are:

- Exclude a configurable edge region from LED detection.
- Add a region of interest covering the expected controller area.
- Reject candidates that remain fixed at the recording-indicator position.
- Add a camera-overlay exclusion setting for the known EOS 60D indicator location.

### MediaPipe Performance Difference

DS4 LED mode has higher FPS than palm/push/open-hand modes. This is expected because DS4 mode uses OpenCV color masking and contours, while hand modes additionally:

- Resize and convert frames for inference.
- Run MediaPipe hand detection/tracking.
- Process two hands and their complete landmark sets.
- Perform calibration and push-depth calculations.

Two-hand tracking is mandatory, so `max_hands` should remain `2`. MediaPipe does not provide a setting to reduce the number of hand bones/landmarks for a tracked hand.

### Low-Light Hand Tracking

Poor lighting can reduce hand detection reliability. Body tracking is not currently a direct solution: a pose model would add processing cost and would not recover hand landmarks hidden by darkness, blur, clipping, or occlusion.

Pose tracking could later provide arm/torso context for short-term prediction or rejection of implausible hand positions, but it should not be added before the basic hand pipeline is stable.

## Implemented Improvements

### Separate MediaPipe Tracking Resolution

The camera and preview remain at the configured capture resolution, while MediaPipe receives a separate resized frame:

```json
"width": 1280,
"height": 720,
"tracking_width": 640,
"tracking_height": 360
```

This reduces inference cost while preserving two-hand tracking and preview quality.

### Configurable LED Colors

LED detection supports both HSV and RGB configuration:

- HSV: hue, hue tolerance, minimum saturation, and minimum value.
- RGB: 8-bit `[red, green, blue]` target, channel tolerance, and minimum value.

Runtime LED sampling is intended for HSV mode. RGB mode uses the configured RGB targets directly.

### LED Blob Size Smoothing

`led_size_smoothing` smooths the displayed blob size so exposure and threshold changes do not make the preview blob pulse rapidly. It affects preview rendering, not whether a completely lost LED is detected.

## Software Improvements To Consider

Prioritized improvements:

1. Add a configurable camera-overlay exclusion region for the EOS 60D recording indicator.
2. Add an optional controller region of interest to exclude keyboard LEDs and unrelated bright objects.
3. Add mask-debug rendering so rejected and accepted pixels can be inspected directly.
4. Log candidate area, brightness, saturation, position, and rejection reason.
5. Add temporal persistence for weak candidates without allowing stale positions to remain indefinitely.
6. Use the previous valid LED position more strongly when scoring competing candidates.
7. Add separate contour area and minimum width/height settings for small lightbars.
8. Save sampled HSV profiles between sessions.
9. Validate RGB arrays and tolerance values more strictly at startup.
10. Consider pose tracking only as a later fallback for short hand-detection gaps.

## Testing Checklist

1. Confirm the camera stream reports the expected negotiated resolution and FPS.
2. Confirm the EOS 60D recording indicator is not selected as a lightbar candidate.
3. Test HSV mode with the included green-left/blue-right defaults.
4. Test custom HSV colors using the actual lightbars.
5. Test RGB mode with DS4Windows RGB values.
6. Compare Capture and Send FPS between DS4 LED and palm/push modes.
7. Compare MediaPipe tracking at `640x360`, `480x270`, and `320x180` while keeping capture at `1280x720`.
8. Test mask-debug and region-of-interest improvements before adding body tracking.

## Build and Configuration Notes

- Debug uses the repository-root `camera_config.json` during Visual Studio debugging.
- Release prefers the configuration beside the executable.
- The native application passes the selected configuration path explicitly to `camera_sender.py`.
- `requirements.txt` is included in the Visual Studio project and remains in the repository root.
- `cv2-enumerate-cameras` is required for named camera discovery; the sender falls back to generic camera probing if it is unavailable.
