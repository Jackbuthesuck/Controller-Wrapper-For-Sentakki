import argparse
import json
import math
import socket
import time
from pathlib import Path
from urllib.request import urlretrieve
import numpy as np
DEFAULT_CONFIG_PATH = Path(__file__).with_name("camera_config.json")

def _strip_json_comments(text):
    result = []
    in_string = False
    escaped = False
    index = 0
    while index < len(text):
        character = text[index]
        if in_string:
            result.append(character)
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == '"':
                in_string = False
            index += 1
            continue
        if character == '"':
            in_string = True
            result.append(character)
            index += 1
        elif text.startswith("//", index):
            newline = text.find("\n", index + 2)
            if newline == -1:
                break
            result.append("\n")
            index = newline + 1
        elif text.startswith("/*", index):
            end_comment = text.find("*/", index + 2)
            if end_comment == -1:
                break
            index = end_comment + 2
        else:
            result.append(character)
            index += 1
    return "".join(result)


def _load_camera_config(config_path, required=False):
    if not config_path.exists():
        if required:
            raise RuntimeError(f"Camera config not found: {config_path}")
        return {}
    try:
        with config_path.open("r", encoding="utf-8") as config_file:
            config = json.loads(_strip_json_comments(config_file.read()))
    except (OSError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"Could not read camera config {config_path}: {exc}") from exc
    if not isinstance(config, dict):
        raise RuntimeError(f"Camera config must contain a JSON object: {config_path}")
    return config


def parse_args() -> argparse.Namespace:
    config_parser = argparse.ArgumentParser(add_help=False)
    config_parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG_PATH)
    config_args, _ = config_parser.parse_known_args()
    config = _load_camera_config(config_args.config, required=config_args.config != DEFAULT_CONFIG_PATH)

    input_mode = config.get("input_mode", "push")
    if isinstance(input_mode, int) and not isinstance(input_mode, bool):
        input_mode = {1: "ds4led", 2: "push", 3: "open"}.get(input_mode)
    if input_mode not in ("push", "open", "curl", "ds4led"):
        raise RuntimeError("input_mode must be 1 (DS4 LED), 2 (push), 3 (open), or a supported mode name")

    parser = argparse.ArgumentParser(
        description="Lightweight hand tracker sender for ControllerInput.exe"
    )
    parser.add_argument("--config", type=Path, default=config_args.config, help="Camera settings JSON path")
    parser.add_argument("--host", default=config.get("host", "127.0.0.1"), help="UDP destination host")
    parser.add_argument("--port", type=int, default=config.get("port", 8765), help="UDP destination port")
    parser.add_argument("--control-port", type=int, default=config.get("control_port", 8766), help="Debug control port")
    parser.add_argument("--status-port", type=int, default=config.get("status_port", 8767), help="Startup status port")
    parser.add_argument("--camera-index", type=int, default=config.get("camera_index", -1), help="Camera index; -1 lists and selects available cameras")
    parser.add_argument("--scrcpy-window", help="Capture a scrcpy window whose title contains this text")
    parser.add_argument("--scrcpy-screen", type=int, default=config.get("scrcpy_screen"), help="Capture an entire monitor; -1 prompts when multiple monitors exist")
    parser.add_argument("--list-cameras", action="store_true", help="List available camera indices and exit")
    parser.add_argument("--max-hands", type=int, default=config.get("max_hands", 2), help="Max hands to track")
    parser.add_argument("--input-mode", choices=("push", "open", "curl", "ds4led"), default=input_mode, help="Camera input: 1=DS4 LED, 2=push, 3=open; names are also accepted")
    parser.add_argument("--push-threshold", type=float, default=config.get("push_threshold", 0.025), help="Palm depth growth required for push click")
    parser.add_argument("--push-release-threshold", type=float, default=config.get("push_release_threshold", 0.012), help="Palm depth growth required to keep a push held")
    parser.add_argument("--min-detect", type=float, default=config.get("min_detect", 0.5), help="Min detection confidence")
    parser.add_argument("--min-track", type=float, default=config.get("min_track", 0.25), help="Min tracking confidence")
    parser.add_argument("--preview", action="store_true", default=config.get("preview", False), help="Show camera preview window")
    parser.add_argument("--fps", type=float, default=config.get("fps", 60.0), help="Target send FPS")
    parser.add_argument("--width", type=int, default=config.get("width", 640), help="Requested camera width")
    parser.add_argument("--height", type=int, default=config.get("height", 480), help="Requested camera height")
    parser.add_argument("--led-jump-confirmations", type=int, default=config.get("led_jump_confirmations", 2), help="Consecutive frames required to accept a large LED move")
    parser.add_argument("--led-jump-match-distance", type=float, default=config.get("led_jump_match_distance", 0.12), help="Maximum normalized distance between frames when confirming an LED move")
    white_fallback = parser.add_mutually_exclusive_group()
    white_fallback.add_argument("--allow-white-led-fallback", dest="allow_white_led_fallback", action="store_true", default=config.get("allow_white_led_fallback", False), help="Use bright, low-saturation bars if colored LED detection fails")
    white_fallback.add_argument("--no-white-led-fallback", dest="allow_white_led_fallback", action="store_false", help="Disable white LED fallback")
    parser.add_argument("--log", action="store_true", default=config.get("log", False), help="Print outgoing packet snapshots")
    parser.add_argument("--log-interval", type=float, default=config.get("log_interval", 0.5), help="Seconds between packet logs")
    parser.add_argument("--model-path", default=config.get("model_path", "hand_landmarker.task"), help="Path to MediaPipe Hand Landmarker .task model (used when mp.solutions is unavailable)")
    parser.add_argument("--auto-download-model", action="store_true", default=config.get("auto_download_model", False), help="Auto-download hand_landmarker.task if missing (requires internet)")
    return parser.parse_args()

def _make_hand_detector(mp, args):
    """Return a callable that maps frames to hand labels, markers, and landmarks."""

    # Old MediaPipe API path (classic solutions)
    if hasattr(mp, "solutions") and hasattr(mp.solutions, "hands"):
        mp_hands = mp.solutions.hands
        hands = mp_hands.Hands(
            max_num_hands=args.max_hands,
            min_detection_confidence=args.min_detect,
            min_tracking_confidence=args.min_track,
        )

        def detect(rgb_frame):
            out = []
            results = hands.process(rgb_frame)
            if results.multi_hand_landmarks and results.multi_handedness:
                for hand_landmarks, handedness in zip(results.multi_hand_landmarks, results.multi_handedness):
                    hand_label = handedness.classification[0].label
                    idx_tip = hand_landmarks.landmark[mp_hands.HandLandmark.INDEX_FINGER_TIP]
                    mid_tip = hand_landmarks.landmark[mp_hands.HandLandmark.MIDDLE_FINGER_TIP]
                    calibration_pose = _is_calibration_pose(hand_landmarks.landmark)
                    if args.input_mode in ("open", "curl"):
                        pressed = _is_open_hand(hand_landmarks.landmark)
                    else:
                        pressed = idx_tip.y < (mid_tip.y - 0.04)
                    marker_x, marker_y = _palm_anchor(hand_landmarks.landmark)
                    landmarks = [(float(point.x), float(point.y)) for point in hand_landmarks.landmark]
                    palm_depth = _palm_depth(hand_landmarks.landmark)
                    palm_scale = _palm_scale(hand_landmarks.landmark)
                    out.append((hand_label, float(marker_x), float(marker_y), bool(pressed), calibration_pose, landmarks, palm_depth, palm_scale))
            return out

        return detect

    # Newer tasks-only API path
    try:
        from mediapipe.tasks import python as mp_python_tasks
        from mediapipe.tasks.python import vision as mp_vision_tasks
    except Exception as exc:
        raise RuntimeError(
            "MediaPipe install does not expose mp.solutions and tasks API could not be imported."
        ) from exc

    model_path = Path(args.model_path)
    if not model_path.exists():
        if args.auto_download_model:
            model_path.parent.mkdir(parents=True, exist_ok=True)
            model_url = (
                "https://storage.googleapis.com/mediapipe-models/hand_landmarker/"
                "hand_landmarker/float16/1/hand_landmarker.task"
            )
            print(f"Model not found. Downloading from: {model_url}")
            try:
                urlretrieve(model_url, str(model_path))
                print(f"Downloaded model to: {model_path}")
            except Exception as exc:
                raise RuntimeError(
                    f"Failed to download model to {model_path}: {exc}"
                ) from exc

        if not model_path.exists():
            raise RuntimeError(
                "This MediaPipe build requires tasks API + model file. "
                f"Model not found: {model_path}. "
                "Place hand_landmarker.task in the repo root, pass --model-path <file>, "
                "or use --auto-download-model."
            )

    base_options = mp_python_tasks.BaseOptions(model_asset_path=str(model_path))
    options = mp_vision_tasks.HandLandmarkerOptions(
        base_options=base_options,
        num_hands=args.max_hands,
        min_hand_detection_confidence=args.min_detect,
        min_hand_presence_confidence=args.min_track,
        min_tracking_confidence=args.min_track,
        running_mode=mp_vision_tasks.RunningMode.VIDEO,
    )
    landmarker = mp_vision_tasks.HandLandmarker.create_from_options(options)
    last_timestamp_ms = 0

    def detect(rgb_frame):
        nonlocal last_timestamp_ms
        out = []
        image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb_frame)
        timestamp_ms = time.monotonic_ns() // 1_000_000
        if timestamp_ms <= last_timestamp_ms:
            timestamp_ms = last_timestamp_ms + 1
        last_timestamp_ms = timestamp_ms
        results = landmarker.detect_for_video(image, timestamp_ms)
        for i, hand_landmarks in enumerate(results.hand_landmarks):
            label = "Unknown"
            if i < len(results.handedness) and len(results.handedness[i]) > 0:
                label = results.handedness[i][0].category_name

            idx_tip = hand_landmarks[8]  # INDEX_FINGER_TIP
            mid_tip = hand_landmarks[12]  # MIDDLE_FINGER_TIP
            calibration_pose = _is_calibration_pose(hand_landmarks)
            if args.input_mode in ("open", "curl"):
                pressed = _is_open_hand(hand_landmarks)
            else:
                pressed = idx_tip.y < (mid_tip.y - 0.04)
            marker_x, marker_y = _palm_anchor(hand_landmarks)
            landmarks = [(float(point.x), float(point.y)) for point in hand_landmarks]
            palm_depth = _palm_depth(hand_landmarks)
            palm_scale = _palm_scale(hand_landmarks)
            out.append((label, float(marker_x), float(marker_y), bool(pressed), calibration_pose, landmarks, palm_depth, palm_scale))
        return out

    return detect


def _find_cameras(cv2, maximum=10):
    try:
        from cv2_enumerate_cameras import enumerate_cameras

        available = []
        for camera in enumerate_cameras():
            if 0 <= camera.index < maximum:
                available.append({
                    "index": camera.index,
                    "name": camera.name or f"Camera {camera.index}",
                })
        if available:
            return sorted(available, key=lambda item: item["index"])
    except Exception:
        pass

    available = []
    for camera_index in range(maximum):
        camera = cv2.VideoCapture(camera_index, cv2.CAP_DSHOW)
        if camera.isOpened():
            width = int(camera.get(cv2.CAP_PROP_FRAME_WIDTH))
            height = int(camera.get(cv2.CAP_PROP_FRAME_HEIGHT))
            fps = camera.get(cv2.CAP_PROP_FPS)
            description = f"Camera {camera_index}"
            if width > 0 and height > 0:
                description += f" ({width}x{height}"
                if fps > 0:
                    description += f" @ {fps:.0f} FPS"
                description += ")"
            available.append({"index": camera_index, "name": description})
        camera.release()
    return available


def _camera_choice_key(position, total):
    """Use one key per choice; 0 selects the tenth entry when needed."""
    if total <= 9:
        return str(position + 1)
    return "0" if position == 9 else str(position + 1)


def _choose_camera(camera_devices):
    if len(camera_devices) == 1:
        camera = camera_devices[0]
        print(f"Using the only camera: {camera['name']}")
        return camera["index"]

    print("Available cameras (press a number to select):")
    for position, camera in enumerate(camera_devices):
        key = _camera_choice_key(position, len(camera_devices))
        print(f"  [{key}] {camera['name']}")

    import msvcrt

    while True:
        key = msvcrt.getwch()
        if key in ("\x03", "\x1b"):
            raise KeyboardInterrupt
        for position, camera in enumerate(camera_devices):
            if key == _camera_choice_key(position, len(camera_devices)):
                print(key)
                print(f"Selected: {camera['name']}")
                return camera["index"]


def _open_scrcpy_capture(title_query):
    try:
        import mss
        import win32gui
    except ImportError as exc:
        raise RuntimeError(
            "scrcpy capture needs mss and pywin32. Install with: "
            "python -m pip install mss pywin32"
        ) from exc

    window_handle = None

    def find_window(handle, _):
        nonlocal window_handle
        if not win32gui.IsWindowVisible(handle):
            return
        title = win32gui.GetWindowText(handle)
        if title_query.lower() in title.lower():
            window_handle = handle

    win32gui.EnumWindows(find_window, None)
    if window_handle is None:
        raise RuntimeError(f"No visible window found matching scrcpy title: {title_query}")

    left, top, right, bottom = win32gui.GetClientRect(window_handle)
    screen_left, screen_top = win32gui.ClientToScreen(window_handle, (left, top))
    width = right - left
    height = bottom - top
    if width <= 0 or height <= 0:
        outer_left, outer_top, outer_right, outer_bottom = win32gui.GetWindowRect(window_handle)
        screen_left = outer_left
        screen_top = outer_top
        width = outer_right - outer_left
        height = outer_bottom - outer_top
        print("Warning: scrcpy client area was unavailable; using its outer window area.")
    if width <= 0 or height <= 0:
        raise RuntimeError("scrcpy is minimized or has no visible area; restore the scrcpy window first")

    print(f"Capturing scrcpy window: {win32gui.GetWindowText(window_handle)}")
    return mss.mss(), {"left": screen_left, "top": screen_top, "width": width, "height": height}


def _open_screen_capture(screen_index, title_query="scrcpy"):
    try:
        import mss
    except ImportError as exc:
        raise RuntimeError(
            "Full-screen capture needs mss. Install with: python -m pip install mss"
        ) from exc

    capture = mss.mss()
    monitors = capture.monitors[1:]
    if not monitors:
        capture.close()
        raise RuntimeError("No physical monitors were found")

    if screen_index < 0:
        if len(monitors) == 1:
            screen_index = 1
        else:
            print("Available monitors:")
            for index, monitor in enumerate(monitors, start=1):
                print(f"  [{index}] {monitor['width']}x{monitor['height']} at ({monitor['left']}, {monitor['top']})")
            while True:
                try:
                    screen_index = int(input("Select monitor containing scrcpy: "))
                except (ValueError, EOFError):
                    print("Enter one of the listed monitor numbers.")
                    continue
                if 1 <= screen_index <= len(monitors):
                    break
                print("That monitor number is not available.")

    if not 1 <= screen_index <= len(monitors):
        capture.close()
        raise RuntimeError(f"Monitor {screen_index} is not available")

    monitor_region = monitors[screen_index - 1]
    region = monitor_region.copy()

    try:
        import win32gui
        scrcpy_window = None

        def find_window(handle, _):
            nonlocal scrcpy_window
            if win32gui.IsWindowVisible(handle) and title_query.lower() in win32gui.GetWindowText(handle).lower():
                scrcpy_window = handle

        win32gui.EnumWindows(find_window, None)
        if scrcpy_window is not None:
            left, top, right, bottom = win32gui.GetWindowRect(scrcpy_window)
            monitor_right = monitor_region["left"] + monitor_region["width"]
            monitor_bottom = monitor_region["top"] + monitor_region["height"]
            left = max(left, monitor_region["left"])
            top = max(top, monitor_region["top"])
            right = min(right, monitor_right)
            bottom = min(bottom, monitor_bottom)
            if right - left > 100 and bottom - top > 100:
                region = {"left": left, "top": top, "width": right - left, "height": bottom - top}
                print(f"Found scrcpy on monitor {screen_index}; capturing its area: {region['width']}x{region['height']}")
    except ImportError:
        print("pywin32 unavailable; capturing the entire selected monitor.")

    if region == monitor_region:
        print(f"Capturing entire monitor {screen_index}: {region['width']}x{region['height']}")
    return capture, region


def _is_calibration_pose(landmarks):
    """Detect a peace sign: index and middle extended, ring and pinky curled."""
    curled = all(
        landmarks[tip].y > landmarks[pip].y
        for tip, pip in ((16, 14), (20, 18))
    )
    index_extended = landmarks[8].y < landmarks[6].y
    middle_extended = landmarks[12].y < landmarks[10].y
    return curled and index_extended and middle_extended


def _is_open_hand(landmarks):
    """Treat three or more extended fingers as touch-down."""
    extended_fingers = sum(
        landmarks[tip].y < landmarks[pip].y
        for tip, pip in ((8, 6), (12, 10), (16, 14), (20, 18))
    )
    return extended_fingers >= 3


def _palm_anchor(landmarks):
    """Return a weighted palm point: wrist 0, thumb 2x, index/pinky 1x."""
    weighted_nodes = ((0, 1.0), (2, 2.0), (5, 1.0), (17, 1.0))
    total_weight = sum(weight for _, weight in weighted_nodes)
    anchor_x = sum(landmarks[index].x * weight for index, weight in weighted_nodes) / total_weight
    anchor_y = sum(landmarks[index].y * weight for index, weight in weighted_nodes) / total_weight
    return anchor_x, anchor_y


def _palm_depth(landmarks):
    """Return the same weighted palm point's MediaPipe camera-relative depth."""
    weighted_nodes = ((0, 1.0), (2, 2.0), (5, 1.0), (17, 1.0))
    total_weight = sum(weight for _, weight in weighted_nodes)
    return sum(landmarks[index].z * weight for index, weight in weighted_nodes) / total_weight

def _palm_scale(landmarks):
    """Estimate apparent palm size from two stable across-palm distances."""
    across_palm = ((landmarks[5].x - landmarks[17].x) ** 2 + (landmarks[5].y - landmarks[17].y) ** 2) ** 0.5
    palm_length = ((landmarks[0].x - landmarks[9].x) ** 2 + (landmarks[0].y - landmarks[9].y) ** 2) ** 0.5
    return (across_palm + palm_length) / 2.0


def _calibrate_position(norm_x, norm_y, left_edge, right_edge, center_y, frame_width, frame_height):
    diameter_x = max(0.1, right_edge - left_edge)
    radius_pixels = (diameter_x * frame_width) / 2.0
    diameter_y = max(0.1, (radius_pixels * 2.0) / frame_height)
    center_x = (left_edge + right_edge) / 2.0
    calibrated_x = 0.5 + (norm_x - center_x) / diameter_x
    calibrated_y = 0.5 + (norm_y - center_y) / diameter_y
    offset_x = calibrated_x - 0.5
    offset_y = calibrated_y - 0.5
    distance = (offset_x * offset_x + offset_y * offset_y) ** 0.5
    if distance > 0.5:
        scale = 0.5 / distance
        offset_x *= scale
        offset_y *= scale
    return max(0.0, min(1.0, 0.5 + offset_x)), max(0.0, min(1.0, 0.5 + offset_y))


def _measure_led_ambient(cv2, frame):
    """Estimate background brightness and saturation from the whole frame."""
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    return {
        "value": float(np.median(hsv[:, :, 2])),
        "saturation": float(np.median(hsv[:, :, 1])),
    }


def _sample_led_color(cv2, frame, center_x=0.5, center_y=0.5):
    """Build a hue profile from the brightest, most saturated pixels at the crosshair."""
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    height, width = hsv.shape[:2]
    radius = max(6, min(width, height) // 16)
    sample_x = int(center_x * width)
    sample_y = int(center_y * height)
    x1 = max(0, sample_x - radius)
    x2 = min(width, sample_x + radius + 1)
    y1 = max(0, sample_y - radius)
    y2 = min(height, sample_y + radius + 1)
    pixels = hsv[y1:y2, x1:x2].reshape(-1, 3).astype(np.float32)
    if pixels.size == 0:
        return None

    saturation_cutoff = np.percentile(pixels[:, 1], 50)
    value_cutoff = np.percentile(pixels[:, 2], 60)
    selected = pixels[(pixels[:, 1] >= max(20.0, saturation_cutoff)) & (pixels[:, 2] >= value_cutoff)]
    if selected.size == 0:
        selected = pixels

    angles = selected[:, 0] * (math.pi / 90.0)
    weights = np.maximum(1.0, selected[:, 1] * selected[:, 2])
    hue = (math.atan2(float(np.sum(np.sin(angles) * weights)), float(np.sum(np.cos(angles) * weights))) * 90.0 / math.pi) % 180.0
    return {
        "hue": hue,
        "saturation": float(np.percentile(selected[:, 1], 75)),
        "value": float(np.percentile(selected[:, 2], 75)),
    }


def _average_led_profiles(profiles):
    if not profiles:
        return None
    angles = np.asarray([profile["hue"] for profile in profiles], dtype=np.float32) * (math.pi / 90.0)
    hue = (math.atan2(float(np.mean(np.sin(angles))), float(np.mean(np.cos(angles)))) * 90.0 / math.pi) % 180.0
    return {
        "hue": hue,
        "saturation": float(np.median([profile["saturation"] for profile in profiles])),
        "value": float(np.median([profile["value"] for profile in profiles])),
    }


def _average_led_ambient(samples):
    if not samples:
        return None
    return {
        "value": float(np.median([sample["value"] for sample in samples])),
        "saturation": float(np.median([sample["saturation"] for sample in samples])),
    }


def _draw_led_crosshair(cv2, frame, label):
    height, width = frame.shape[:2]
    center = (width // 2, height // 2)
    color = (0, 255, 255)
    size = max(12, min(width, height) // 12)
    cv2.line(frame, (center[0] - size, center[1]), (center[0] + size, center[1]), color, 2, cv2.LINE_AA)
    cv2.line(frame, (center[0], center[1] - size), (center[0], center[1] + size), color, 2, cv2.LINE_AA)
    cv2.circle(frame, center, size // 2, color, 1, cv2.LINE_AA)
    cv2.putText(frame, f"RELEASE TO SET {label}", (max(10, center[0] - 130), center[1] - size - 12),
                cv2.FONT_HERSHEY_SIMPLEX, 0.55, color, 2, cv2.LINE_AA)


def _detect_led_positions(cv2, frame, allow_white_fallback=False, ambient_profile=None, color_profiles=None):
    """Track independently colored lightbars, using runtime samples when available."""
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    def find_color(hue_ranges, expected_x):
        mask = None
        for lower, upper in hue_ranges:
            part = cv2.inRange(hsv, lower, upper)
            mask = part if mask is None else cv2.bitwise_or(mask, part)
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, None, iterations=3)
        mask = cv2.dilate(mask, None, iterations=1)
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        candidates = []
        for contour in contours:
            area = cv2.contourArea(contour)
            x, y, width, height = cv2.boundingRect(contour)
            if area < 4 or width < 2 or height < 2:
                continue
            roi = hsv[y:y + height, x:x + width]
            brightness = float(roi[:, :, 2].mean()) if roi.size else 0.0
            saturation = float(roi[:, :, 1].mean()) if roi.size else 0.0
            center_x = (x + width / 2.0) / frame.shape[1]
            position_score = max(0.05, 1.0 - abs(center_x - expected_x) * 3.0)
            score = area * (brightness / 255.0) * (saturation / 255.0) * position_score
            candidates.append((score, contour))
        if not candidates:
            return None
        contour = max(candidates, key=lambda item: item[0])[1]
        x, y, width, height = cv2.boundingRect(contour)
        return (
            float((x + width / 2.0) / frame.shape[1]),
            float((y + height / 2.0) / frame.shape[0]),
            contour,
        )

    def find_profile(profile, expected_x):
        hue_distance = np.abs(hsv[:, :, 0].astype(np.float32) - profile["hue"])
        hue_distance = np.minimum(hue_distance, 180.0 - hue_distance)
        ambient_value = ambient_profile["value"] if ambient_profile else 0.0
        ambient_saturation = ambient_profile["saturation"] if ambient_profile else 0.0
        saturation_floor = max(30.0, min(profile["saturation"] * 0.65, profile["saturation"] - 5.0))
        saturation_floor = max(saturation_floor, min(ambient_saturation + 8.0, profile["saturation"] * 0.8))
        value_floor = max(profile["value"] * 0.35, ambient_value + 8.0)
        mask = (
            (hue_distance <= 14.0)
            & (hsv[:, :, 1] >= saturation_floor)
            & (hsv[:, :, 2] >= value_floor)
        ).astype(np.uint8) * 255
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, None, iterations=3)
        mask = cv2.dilate(mask, None, iterations=1)
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        candidates = []
        for contour in contours:
            area = cv2.contourArea(contour)
            x, y, width, height = cv2.boundingRect(contour)
            if area < 4 or width < 2 or height < 2:
                continue
            roi = hsv[y:y + height, x:x + width]
            brightness = float(roi[:, :, 2].mean()) if roi.size else 0.0
            saturation = float(roi[:, :, 1].mean()) if roi.size else 0.0
            center_x = (x + width / 2.0) / frame.shape[1]
            position_score = max(0.05, 1.0 - abs(center_x - expected_x) * 3.0)
            score = area * (brightness / 255.0) * (saturation / 255.0) * position_score
            candidates.append((score, contour))
        if not candidates:
            return None
        contour = max(candidates, key=lambda item: item[0])[1]
        x, y, width, height = cv2.boundingRect(contour)
        return (
            float((x + width / 2.0) / frame.shape[1]),
            float((y + height / 2.0) / frame.shape[0]),
            contour,
        )

    def find_white_bars():
        mask = cv2.inRange(hsv, (0, 0, 220), (180, 90, 255))
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, None, iterations=2)
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        bars = []
        for contour in contours:
            area = cv2.contourArea(contour)
            x, y, width, height = cv2.boundingRect(contour)
            aspect = width / max(1, height)
            fill = area / max(1, width * height)
            if area < 20 or width < 10 or height < 2 or aspect < 1.8 or fill < 0.2:
                continue
            moments = cv2.moments(contour)
            if moments["m00"] == 0:
                continue
            bars.append((
                float(moments["m10"] / moments["m00"] / frame.shape[1]),
                float(moments["m01"] / moments["m00"] / frame.shape[0]),
                contour,
                area * min(aspect, 10.0),
            ))
        return sorted(bars, key=lambda bar: bar[3], reverse=True)

    left_profile = color_profiles.get("Left") if color_profiles else None
    right_profile = color_profiles.get("Right") if color_profiles else None
    blue = find_profile(left_profile, expected_x=0.25) if left_profile else find_color([((112, 150, 55), (130, 255, 255))], expected_x=0.25)
    red = find_profile(right_profile, expected_x=0.75) if right_profile else find_color(
        [((0, 120, 55), (10, 255, 255)), ((170, 120, 55), (180, 255, 255))],
        expected_x=0.75,
    )

    if not allow_white_fallback:
        return blue, red

    white_bars = find_white_bars()
    if blue is None and red is None and len(white_bars) >= 2:
        white_bars = sorted(white_bars[:2], key=lambda bar: bar[0])
        blue = white_bars[0][:3]
        red = white_bars[1][:3]
    elif blue is None and white_bars:
        blue = min(white_bars, key=lambda bar: abs(bar[0] - 0.25))[:3]
    elif red is None and white_bars:
        red = min(white_bars, key=lambda bar: abs(bar[0] - 0.75))[:3]
    return blue, red


HAND_CONNECTIONS = (
    (0, 1), (1, 2), (2, 3), (3, 4),
    (0, 5), (5, 6), (6, 7), (7, 8),
    (5, 9), (9, 10), (10, 11), (11, 12),
    (9, 13), (13, 14), (14, 15), (15, 16),
    (13, 17), (17, 18), (18, 19), (19, 20),
    (0, 17),
)


def _draw_hand_skeleton(cv2, frame, hand_results):
    """Draw the same two-hand debug information used by the UDP sender."""
    height, width, _ = frame.shape
    for hand_label, marker_x, marker_y, pressed, calibration_pose, landmarks, _, _ in hand_results:
        color = (0, 220, 0) if hand_label == "Left" else (220, 80, 255)
        points = [(int(x * width), int(y * height)) for x, y in landmarks]
        for start, end in HAND_CONNECTIONS:
            cv2.line(frame, points[start], points[end], color, 2, cv2.LINE_AA)
        for point in points:
            cv2.circle(frame, point, 3, (255, 255, 255), -1, cv2.LINE_AA)

        marker = (int(marker_x * width), int(marker_y * height))
        cv2.circle(frame, marker, 11, (0, 255, 255) if calibration_pose else color, 2, cv2.LINE_AA)
        state = "CALIBRATE" if calibration_pose else ("CLICK" if pressed else "READY")
        cv2.putText(frame, f"{hand_label}: {state}", (marker[0] + 14, marker[1] - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 1, cv2.LINE_AA)


def main() -> int:
    args = parse_args()

    try:
        import cv2
    except Exception as exc:
        print("Missing dependencies. Install with: pip install -r requirements.txt")
        print(f"Import error: {exc}")
        return 1

    if args.list_cameras:
        found = _find_cameras(cv2)
        if not found:
            print("Available cameras: none")
        else:
            print("Available cameras:")
            for camera in found:
                print(f"  [{camera['index']}] {camera['name']}")
        return 0

    scrcpy_capture = None
    scrcpy_region = None
    if args.scrcpy_screen is not None or args.scrcpy_window == "__screen__":
        try:
            scrcpy_capture, scrcpy_region = _open_screen_capture(args.scrcpy_screen)
        except RuntimeError as exc:
            print(f"Error: {exc}")
            return 1
    elif args.scrcpy_window:
        try:
            scrcpy_capture, scrcpy_region = _open_scrcpy_capture(args.scrcpy_window)
        except RuntimeError as exc:
            print(f"Error: {exc}")
            return 1
    elif args.camera_index < 0:
        available_cameras = _find_cameras(cv2)
        if not available_cameras:
            print("Error: no cameras found")
            return 1
        args.camera_index = _choose_camera(available_cameras)

    udp_addr = (args.host, args.port)
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.setblocking(False)
    control_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        control_sock.bind(("127.0.0.1", args.control_port))
        control_sock.setblocking(False)
    except OSError as exc:
        print(f"Warning: debug control unavailable on port {args.control_port}: {exc}")
        control_sock.close()
        control_sock = None

    cap = None if scrcpy_capture is not None else cv2.VideoCapture(args.camera_index, cv2.CAP_DSHOW)
    if cap is not None and not cap.isOpened():
        print(f"Error: cannot open camera index {args.camera_index}")
        return 1
    if cap is not None:
        cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
        cap.set(cv2.CAP_PROP_FPS, args.fps)
        negotiated_fps = cap.get(cv2.CAP_PROP_FPS)
        negotiated_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        negotiated_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        print(
            f"Camera stream: {negotiated_width}x{negotiated_height} at "
            f"{negotiated_fps:.0f} FPS (target {args.fps:.0f})"
        )

    if args.input_mode == "ds4led":
        detect_hands = lambda _frame: []
        print("DS4 LED mode: hand detection disabled; using lightbar plus physical L1/R1.")
    else:
        try:
            import mediapipe as mp
            detect_hands = _make_hand_detector(mp, args)
        except Exception as exc:
            print("Failed to initialize hand detector backend.")
            print(f"Reason: {exc}")
            return 1

    status_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        status_sock.sendto(b"PYTHON_READY", ("127.0.0.1", args.status_port))
    except OSError as exc:
        print(f"Warning: could not send startup status: {exc}")
    finally:
        status_sock.close()

    min_frame_dt = 1.0 / max(1.0, args.fps)
    last_send = 0.0
    last_log = 0.0
    sent_count = 0
    send_frame_count = 0
    frame_count = 0
    fps_window_start = time.perf_counter()
    capture_fps = 0.0
    send_fps = 0.0
    calibration_left_edge = None
    calibration_right_edge = None
    calibration_center_y = 0.5
    calibration_active = False
    pending_calibration_left = None
    pending_calibration_right = None
    pending_calibration_center_y = 0.5
    pending_rest_depths = {}
    pending_rest_scales = {}
    pending_push_threshold = args.push_threshold
    rest_depths = {}
    rest_scales = {}
    calibrated_push_threshold = args.push_threshold
    calibrated_push_release_threshold = args.push_release_threshold
    push_states = {"Left": False, "Right": False}
    push_metrics = {"Left": 0.0, "Right": 0.0}
    last_pressed = {"Left": 0, "Right": 0}
    last_left_x = 0.5
    last_left_y = 0.5
    last_right_x = 0.5
    last_right_y = 0.5
    last_seen = {"Left": time.perf_counter(), "Right": time.perf_counter()}
    hand_lost_timeout = 3.0
    debug_visible = True
    calibration_armed = False
    calibration_rearm_required = False
    led_filtered = {"Left": None, "Right": None}
    led_jump_candidates = {"Left": None, "Right": None}
    ambient_profile = None
    led_color_profiles = {}
    led_sampling_action = None
    led_sample_release = None
    led_ambient_samples = []
    led_color_samples = {"Left": [], "Right": []}

    source = "DS4 LED + L1/R1" if args.input_mode == "ds4led" else f"hand {args.input_mode}"
    print(f"Sending {source} data to {args.host}:{args.port}")
    print("Press 'q' in preview window to quit." if args.preview else "Press Ctrl+C to quit.")

    try:
        while True:
            if control_sock is not None:
                try:
                    while True:
                        command = control_sock.recv(32).decode("ascii", errors="ignore").strip()
                        if command == "DEBUG 1":
                            debug_visible = True
                        elif command == "DEBUG 0":
                            debug_visible = False
                        elif command == "CALIBRATE" or command == "CALIBRATE CIRCLE":
                            calibration_armed = True
                            if args.input_mode == "ds4led":
                                print("DS4 LED circle calibration armed.")
                            else:
                                print("Calibration armed. Show peace signs with both hands at neutral rest depth, then release.")
                        elif command.startswith("LED "):
                            parts = command.split()
                            if len(parts) == 3 and parts[2] in ("0", "1") and parts[1] in ("AMBIENT", "LEFT", "RIGHT"):
                                action = parts[1].lower()
                                if parts[2] == "1":
                                    led_sampling_action = action
                                    led_sample_release = None
                                    led_ambient_samples.clear()
                                    led_color_samples["Left"].clear()
                                    led_color_samples["Right"].clear()
                                    print(f"LED {action} sampling armed. Hold the crosshair over the target, then release.")
                                elif led_sampling_action == action:
                                    led_sample_release = action
                                    led_sampling_action = None
                except BlockingIOError:
                    pass

            if scrcpy_capture is not None:
                screenshot = scrcpy_capture.grab(scrcpy_region)
                import numpy as np
                frame = cv2.cvtColor(np.asarray(screenshot), cv2.COLOR_BGRA2BGR)
                ok = True
            else:
                ok, frame = cap.read()
                if not ok:
                    time.sleep(0.01)
                    continue

            frame_count += 1
            fps_now = time.perf_counter()
            fps_elapsed = fps_now - fps_window_start
            if fps_elapsed >= 1.0:
                capture_fps = frame_count / fps_elapsed
                send_fps = send_frame_count / fps_elapsed
                frame_count = 0
                send_frame_count = 0
                fps_window_start = fps_now

            frame = cv2.flip(frame, 1)
            now = time.perf_counter()
            if args.input_mode == "ds4led":
                if led_sampling_action == "ambient":
                    led_ambient_samples.append(_measure_led_ambient(cv2, frame))
                elif led_sampling_action in ("left", "right"):
                    sample = _sample_led_color(cv2, frame)
                    if sample is not None:
                        led_color_samples["Left" if led_sampling_action == "left" else "Right"].append(sample)

                if led_sample_release is not None:
                    if led_sample_release == "ambient":
                        if not led_ambient_samples:
                            led_ambient_samples.append(_measure_led_ambient(cv2, frame))
                        ambient_profile = _average_led_ambient(led_ambient_samples)
                        print(f"LED ambient set: value {ambient_profile['value']:.0f}, saturation {ambient_profile['saturation']:.0f}.")
                    else:
                        hand_label = "Left" if led_sample_release == "left" else "Right"
                        samples = led_color_samples[hand_label]
                        if not samples:
                            sample = _sample_led_color(cv2, frame)
                            if sample is not None:
                                samples.append(sample)
                        profile = _average_led_profiles(samples)
                        if profile is not None:
                            led_color_profiles[hand_label] = profile
                            print(f"LED {led_sample_release} color set: hue {profile['hue']:.1f}, saturation {profile['saturation']:.0f}, value {profile['value']:.0f}.")
                        else:
                            print(f"LED {led_sample_release} color sample failed; keep the crosshair over the lightbar and try again.")
                    led_sample_release = None
                    led_ambient_samples.clear()
                    led_color_samples["Left"].clear()
                    led_color_samples["Right"].clear()
            for hand_label in ("Left", "Right"):
                if now - last_seen[hand_label] > hand_lost_timeout:
                    if hand_label == "Left":
                        last_left_x = 0.5
                        last_left_y = 0.5
                    else:
                        last_right_x = 0.5
                        last_right_y = 0.5
                    last_pressed[hand_label] = 0
                    push_states[hand_label] = False
                    push_metrics[hand_label] = 0.0
            # Per-frame defaults mirror C++ expectations.
            left_x = last_left_x
            left_y = last_left_y
            left_pressed = last_pressed["Left"]
            right_x = last_right_x
            right_y = last_right_y
            right_pressed = last_pressed["Right"]

            if args.input_mode == "ds4led":
                hand_results = []
            else:
                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                hand_results = detect_hands(rgb)
            led_positions = (None, None)
            if args.input_mode == "ds4led":
                led_positions = _detect_led_positions(
                    cv2,
                    frame,
                    allow_white_fallback=args.allow_white_led_fallback,
                    ambient_profile=ambient_profile,
                    color_profiles=led_color_profiles,
                )

                if calibration_armed and led_positions[0] is not None and led_positions[1] is not None:
                    blue_x, blue_y, _ = led_positions[0]
                    red_x, red_y, _ = led_positions[1]
                    calibration_left_edge = min(blue_x, red_x)
                    calibration_right_edge = max(blue_x, red_x)
                    calibration_center_y = (blue_y + red_y) / 2.0
                    if calibration_right_edge - calibration_left_edge >= 0.1:
                        calibration_armed = False
                        print("DS4 LED calibrated: circle set from blue/red lightbars.")

            calibration_points = [
                (norm_x, norm_y, hand_label, palm_depth)
                for hand_label, norm_x, norm_y, _, is_calibration, _, palm_depth, palm_scale in hand_results
                if is_calibration
            ]
            if not calibration_points:
                calibration_rearm_required = False
            elif len(calibration_points) >= 2 and not calibration_armed and not calibration_rearm_required:
                calibration_armed = True
                print("Calibration gesture detected. Hold position, then release to set the circle.")
            gesture_active = calibration_armed and len(calibration_points) >= 2
            if gesture_active:
                candidate_left = min(point[0] for point in calibration_points)
                candidate_right = max(point[0] for point in calibration_points)
                if candidate_right - candidate_left >= 0.1:
                    pending_calibration_left = candidate_left
                    pending_calibration_right = candidate_right
                    pending_calibration_center_y = sum(point[1] for point in calibration_points) / len(calibration_points)
                    pending_rest_depths = {
                        label: depth
                        for _, _, label, depth in calibration_points
                    }
                    pending_push_threshold = args.push_threshold
                    pending_rest_scales = {
                        label: scale
                        for label, _, _, scale in (
                            (hand_label, norm_x, norm_y, palm_scale)
                            for hand_label, norm_x, norm_y, _, is_calibration, _, _, palm_scale in hand_results
                            if is_calibration
                        )
                    }
                if not calibration_active:
                    print("Calibration gesture active. Hold position, then release to set the circle.")
            elif calibration_active:
                if pending_calibration_left is not None and pending_calibration_right - pending_calibration_left >= 0.1:
                    calibration_left_edge = pending_calibration_left
                    calibration_right_edge = pending_calibration_right
                    calibration_center_y = pending_calibration_center_y
                    rest_depths = pending_rest_depths.copy()
                    rest_scales = pending_rest_scales.copy()
                    calibrated_push_threshold = pending_push_threshold
                    calibrated_push_release_threshold = calibrated_push_threshold * 0.5
                    push_states = {"Left": False, "Right": False}
                    calibration_armed = False
                    calibration_rearm_required = True
                    print("Camera calibrated: circle set from released gesture.")
                pending_calibration_left = None
                pending_calibration_right = None
                pending_rest_depths = {}
                pending_rest_scales = {}
            calibration_active = gesture_active

            if args.input_mode == "ds4led":
                blue_led, red_led = led_positions
                for hand_label, led in (("Left", blue_led), ("Right", red_led)):
                    if led is None:
                        continue
                    led_x, led_y, led_contour = led
                    previous_led = led_filtered[hand_label]
                    if previous_led is not None and (
                        abs(led_x - previous_led[0]) > 0.45
                        or abs(led_y - previous_led[1]) > 0.35
                    ):
                        pending_jump = led_jump_candidates[hand_label]
                        jump_is_continuous = (
                            pending_jump is not None
                            and now - pending_jump[3] <= 0.3
                            and ((led_x - pending_jump[0]) ** 2 + (led_y - pending_jump[1]) ** 2) ** 0.5
                            <= max(0.01, args.led_jump_match_distance)
                        )
                        jump_count = pending_jump[2] + 1 if jump_is_continuous else 1
                        if jump_count < max(1, args.led_jump_confirmations):
                            led_jump_candidates[hand_label] = (led_x, led_y, jump_count, now)
                            continue
                        led_jump_candidates[hand_label] = None
                    else:
                        led_jump_candidates[hand_label] = None
                    led_filtered[hand_label] = (led_x, led_y)
                    if calibration_left_edge is not None and calibration_right_edge - calibration_left_edge >= 0.1:
                        led_x, led_y = _calibrate_position(
                            led_x,
                            led_y,
                            calibration_left_edge,
                            calibration_right_edge,
                            calibration_center_y,
                            frame.shape[1],
                            frame.shape[0],
                        )
                    if hand_label == "Left":
                        left_x, left_y = led_x, led_y
                        last_left_x, last_left_y = led_x, led_y
                        led_color = (255, 120, 0)
                    else:
                        right_x, right_y = led_x, led_y
                        last_right_x, last_right_y = led_x, led_y
                        led_color = (0, 0, 255)
                    last_seen[hand_label] = now
                    if args.preview and debug_visible:
                        cv2.drawContours(frame, [led_contour], -1, led_color, 2)
            else:
                for hand_label, norm_x, norm_y, is_shooting, is_calibration, _, palm_depth, palm_scale in hand_results:
                    if hand_label not in last_seen:
                        continue
                    last_seen[hand_label] = now
                    if is_calibration:
                        if hand_label == "Left":
                            left_pressed = 0
                        else:
                            right_pressed = 0
                        last_pressed[hand_label] = 0
                        push_states[hand_label] = False
                        push_metrics[hand_label] = 0.0
                        continue
                    if args.input_mode == "push":
                        baseline_depth = rest_depths.get(hand_label)
                        baseline_scale = rest_scales.get(hand_label)
                        if baseline_depth is None or baseline_scale is None or baseline_scale <= 0:
                            is_shooting = False
                            push_states[hand_label] = False
                            push_metrics[hand_label] = 0.0
                        else:
                            depth_growth = baseline_depth - palm_depth
                            push_metrics[hand_label] = depth_growth
                            threshold = (
                                calibrated_push_release_threshold
                                if push_states[hand_label]
                                else calibrated_push_threshold
                            )
                            is_shooting = depth_growth >= threshold
                            push_states[hand_label] = is_shooting
                    if calibration_left_edge is not None and calibration_right_edge - calibration_left_edge >= 0.1:
                        norm_x, norm_y = _calibrate_position(
                            norm_x,
                            norm_y,
                            calibration_left_edge,
                            calibration_right_edge,
                            calibration_center_y,
                            frame.shape[1],
                            frame.shape[0],
                        )
                    if hand_label == "Left":
                        left_x = max(0.0, min(1.0, norm_x))
                        left_y = max(0.0, min(1.0, norm_y))
                        last_left_x = left_x
                        last_left_y = left_y
                        left_pressed = 1 if is_shooting else 0
                        last_pressed["Left"] = left_pressed
                    elif hand_label == "Right":
                        right_x = max(0.0, min(1.0, norm_x))
                        right_y = max(0.0, min(1.0, norm_y))
                        last_right_x = right_x
                        last_right_y = right_y
                        right_pressed = 1 if is_shooting else 0
                        last_pressed["Right"] = right_pressed

            if calibration_armed:
                left_pressed = 0
                right_pressed = 0
                last_pressed["Left"] = 0
                last_pressed["Right"] = 0

            if now - last_send >= min_frame_dt:
                controller_clicks = 1 if args.input_mode == "ds4led" else 0
                msg = f"{left_x:.4f},{left_y:.4f},{left_pressed},{right_x:.4f},{right_y:.4f},{right_pressed},{controller_clicks}"
                try:
                    udp_sock.sendto(msg.encode("ascii"), udp_addr)
                    sent_count += 1
                    send_frame_count += 1
                except BlockingIOError:
                    pass
                except OSError:
                    pass
                last_send = now

                if args.log and (now - last_log) >= max(0.05, args.log_interval):
                    print(f"[{sent_count}] {msg}")
                    last_log = now

            if args.preview:
                h, w, _ = frame.shape
                if debug_visible:
                    if calibration_left_edge is not None and calibration_right_edge - calibration_left_edge >= 0.1:
                        circle_center_x = int(((calibration_left_edge + calibration_right_edge) / 2.0) * w)
                        circle_center_y = int(calibration_center_y * h)
                        circle_radius = int(((calibration_right_edge - calibration_left_edge) / 2.0) * w)
                        cv2.circle(frame, (circle_center_x, circle_center_y), circle_radius,
                                   (0, 255, 255), 2, cv2.LINE_AA)
                    if args.input_mode != "ds4led":
                        _draw_hand_skeleton(cv2, frame, hand_results)
                    if left_x > 0.0 or left_y > 0.0:
                        cv2.circle(frame, (int(left_x * w), int(left_y * h)), 10, (0, 255, 0), -1)
                        cv2.putText(frame, f"L:{left_pressed}", (int(left_x * w) + 8, int(left_y * h) - 8), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
                    if right_x > 0.0 or right_y > 0.0:
                        cv2.circle(frame, (int(right_x * w), int(right_y * h)), 10, (0, 0, 255), -1)
                        cv2.putText(frame, f"R:{right_pressed}", (int(right_x * w) + 8, int(right_y * h) - 8), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)

                    capture_label = "SCRCPY MONITOR" if scrcpy_capture is not None else f"CAMERA {args.camera_index}"
                    cv2.putText(frame, f"{capture_label} {frame.shape[1]}x{frame.shape[0]}", (10, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
                    cv2.putText(frame, f"Capture: {capture_fps:.1f} FPS | Send: {send_fps:.1f} FPS", (10, 48), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (255, 255, 255), 1)
                    cv2.putText(frame, f"Push depth L:{push_metrics['Left']:.2f} R:{push_metrics['Right']:.2f} / {calibrated_push_threshold:.2f}", (10, 96), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
                    if calibration_armed and not calibration_active:
                        calibration_status = "PEACE: ONE REST, ONE PUSH"
                    elif calibration_active:
                        calibration_status = "SETTING... RELEASE TO APPLY"
                    else:
                        calibration_status = "CALIBRATED" if calibration_left_edge is not None else "NOT CALIBRATED"
                    cv2.putText(frame, f"Mode: {args.input_mode} | Circle: {calibration_status}", (10, 72), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 255, 255) if calibration_left_edge is not None else (180, 180, 180), 1)
                    if args.input_mode == "ds4led" and led_sampling_action:
                        _draw_led_crosshair(cv2, frame, led_sampling_action.upper())
                    if args.input_mode == "ds4led":
                        profile_status = (
                            f"LED profiles A:{'Y' if ambient_profile else 'N'} "
                            f"L:{'Y' if 'Left' in led_color_profiles else 'N'} "
                            f"R:{'Y' if 'Right' in led_color_profiles else 'N'}"
                        )
                        cv2.putText(frame, profile_status, (10, 120), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
                cv2.imshow("Camera Sender", frame)
                key = cv2.waitKey(1) & 0xFF
                if key == ord("c"):
                    calibration_armed = True
                    calibration_active = False
                    print("Calibration armed. Show peace signs with both hands at neutral rest depth, then release.")
                elif key == ord("q"):
                    break

    except KeyboardInterrupt:
        pass
    finally:
        if cap is not None:
            cap.release()
        if scrcpy_capture is not None:
            scrcpy_capture.close()
        cv2.destroyAllWindows()
        udp_sock.close()
        if control_sock is not None:
            control_sock.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
