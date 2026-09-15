from __future__ import annotations

import argparse
import json
import math
import socket
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional
from urllib.request import urlretrieve

import numpy as np


DEFAULT_CONFIG_PATH = Path(__file__).with_name("camera_config.json")
MODEL_URL = (
    "https://storage.googleapis.com/mediapipe-models/hand_landmarker/"
    "hand_landmarker/float16/1/hand_landmarker.task"
)
INPUT_MODE_NAMES = {1: "ds4led", 2: "push", 3: "open"}
HAND_LABELS = ("Left", "Right")
HAND_CONNECTIONS = (
    (0, 1), (1, 2), (2, 3), (3, 4),
    (0, 5), (5, 6), (6, 7), (7, 8),
    (5, 9), (9, 10), (10, 11), (11, 12),
    (9, 13), (13, 14), (14, 15), (15, 16),
    (13, 17), (17, 18), (18, 19), (19, 20),
    (0, 17),
)


# Configuration


def _strip_json_comments(text: str) -> str:
    result: list[str] = []
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


def _load_camera_config(config_path: Path, required: bool = False) -> dict[str, Any]:
    config_path = Path(config_path)
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


def _normalize_input_mode(value: Any) -> str:
    if isinstance(value, int) and not isinstance(value, bool):
        value = INPUT_MODE_NAMES.get(value)
    if value not in ("push", "open", "curl", "ds4led"):
        raise RuntimeError(
            "input_mode must be 1 (DS4 LED), 2 (push), 3 (open), or a supported mode name"
        )
    return value


@dataclass(frozen=True)
class SenderConfig:
    config_path: Path
    host: str
    port: int
    control_port: int
    status_port: int
    camera_index: int
    scrcpy_window: Optional[str]
    scrcpy_screen: Optional[int]
    list_cameras: bool
    max_hands: int
    input_mode: str
    push_threshold: float
    push_release_threshold: float
    min_detect: float
    min_track: float
    preview: bool
    fps: float
    width: int
    height: int
    led_jump_confirmations: int
    led_jump_match_distance: float
    allow_white_led_fallback: bool
    log: bool
    log_interval: float
    model_path: str
    auto_download_model: bool

    def validate(self) -> None:
        if self.fps <= 0:
            raise RuntimeError("fps must be greater than zero")
        if self.width <= 0 or self.height <= 0:
            raise RuntimeError("width and height must be greater than zero")
        if self.max_hands <= 0:
            raise RuntimeError("max_hands must be greater than zero")
        if self.led_jump_confirmations <= 0:
            raise RuntimeError("led_jump_confirmations must be greater than zero")
        if self.led_jump_match_distance <= 0:
            raise RuntimeError("led_jump_match_distance must be greater than zero")
        if self.push_threshold < 0 or self.push_release_threshold < 0:
            raise RuntimeError("push thresholds cannot be negative")
        if self.log_interval < 0:
            raise RuntimeError("log_interval cannot be negative")


def _build_argument_parser(config: dict[str, Any], config_path: Path) -> argparse.ArgumentParser:
    input_mode = _normalize_input_mode(config.get("input_mode", "push"))
    parser = argparse.ArgumentParser(
        description="Lightweight hand tracker sender for ControllerInput.exe"
    )
    parser.add_argument("--config", type=Path, default=config_path, help="Camera settings JSON path")
    parser.add_argument("--host", default=config.get("host", "127.0.0.1"), help="UDP destination host")
    parser.add_argument("--port", type=int, default=config.get("port", 8765), help="UDP destination port")
    parser.add_argument("--control-port", type=int, default=config.get("control_port", 8766), help="Debug control port")
    parser.add_argument("--status-port", type=int, default=config.get("status_port", 8767), help="Startup status port")
    parser.add_argument(
        "--camera-index",
        type=int,
        default=config.get("camera_index", -1),
        help="Camera index; -1 lists and selects available cameras",
    )
    parser.add_argument("--scrcpy-window", help="Capture a scrcpy window whose title contains this text")
    parser.add_argument(
        "--scrcpy-screen",
        type=int,
        default=config.get("scrcpy_screen"),
        help="Capture an entire monitor; -1 prompts when multiple monitors exist",
    )
    parser.add_argument("--list-cameras", action="store_true", help="List available camera indices and exit")
    parser.add_argument("--max-hands", type=int, default=config.get("max_hands", 2), help="Max hands to track")
    parser.add_argument(
        "--input-mode",
        choices=("push", "open", "curl", "ds4led"),
        default=input_mode,
        help="Camera input: 1=DS4 LED, 2=push, 3=open; names are also accepted",
    )
    parser.add_argument(
        "--push-threshold",
        type=float,
        default=config.get("push_threshold", 0.025),
        help="Palm depth growth required for push click",
    )
    parser.add_argument(
        "--push-release-threshold",
        type=float,
        default=config.get("push_release_threshold", 0.012),
        help="Palm depth growth required to keep a push held",
    )
    parser.add_argument("--min-detect", type=float, default=config.get("min_detect", 0.5), help="Min detection confidence")
    parser.add_argument("--min-track", type=float, default=config.get("min_track", 0.25), help="Min tracking confidence")
    parser.add_argument("--preview", action="store_true", default=config.get("preview", False), help="Show camera preview window")
    parser.add_argument("--fps", type=float, default=config.get("fps", 60.0), help="Target send FPS")
    parser.add_argument("--width", type=int, default=config.get("width", 640), help="Requested camera width")
    parser.add_argument("--height", type=int, default=config.get("height", 480), help="Requested camera height")
    parser.add_argument(
        "--led-jump-confirmations",
        type=int,
        default=config.get("led_jump_confirmations", 2),
        help="Consecutive frames required to accept a large LED move",
    )
    parser.add_argument(
        "--led-jump-match-distance",
        type=float,
        default=config.get("led_jump_match_distance", 0.12),
        help="Maximum normalized distance between frames when confirming an LED move",
    )
    white_fallback = parser.add_mutually_exclusive_group()
    white_fallback.add_argument(
        "--allow-white-led-fallback",
        dest="allow_white_led_fallback",
        action="store_true",
        default=config.get("allow_white_led_fallback", False),
        help="Use bright, low-saturation bars if colored LED detection fails",
    )
    white_fallback.add_argument(
        "--no-white-led-fallback",
        dest="allow_white_led_fallback",
        action="store_false",
        help="Disable white LED fallback",
    )
    parser.add_argument("--log", action="store_true", default=config.get("log", False), help="Print outgoing packet snapshots")
    parser.add_argument("--log-interval", type=float, default=config.get("log_interval", 0.5), help="Seconds between packet logs")
    parser.add_argument(
        "--model-path",
        default=config.get("model_path", "hand_landmarker.task"),
        help="Path to MediaPipe Hand Landmarker .task model (used when mp.solutions is unavailable)",
    )
    parser.add_argument(
        "--auto-download-model",
        action="store_true",
        default=config.get("auto_download_model", False),
        help="Auto-download hand_landmarker.task if missing (requires internet)",
    )
    return parser


def parse_args(argv: Optional[list[str]] = None) -> SenderConfig:
    config_parser = argparse.ArgumentParser(add_help=False)
    config_parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG_PATH)
    config_args, _ = config_parser.parse_known_args(argv)
    config = _load_camera_config(
        config_args.config,
        required=config_args.config != DEFAULT_CONFIG_PATH,
    )
    parser = _build_argument_parser(config, config_args.config)
    namespace = parser.parse_args(argv)
    settings = SenderConfig(
        config_path=namespace.config,
        host=namespace.host,
        port=namespace.port,
        control_port=namespace.control_port,
        status_port=namespace.status_port,
        camera_index=namespace.camera_index,
        scrcpy_window=namespace.scrcpy_window,
        scrcpy_screen=namespace.scrcpy_screen,
        list_cameras=namespace.list_cameras,
        max_hands=namespace.max_hands,
        input_mode=namespace.input_mode,
        push_threshold=namespace.push_threshold,
        push_release_threshold=namespace.push_release_threshold,
        min_detect=namespace.min_detect,
        min_track=namespace.min_track,
        preview=namespace.preview,
        fps=namespace.fps,
        width=namespace.width,
        height=namespace.height,
        led_jump_confirmations=namespace.led_jump_confirmations,
        led_jump_match_distance=namespace.led_jump_match_distance,
        allow_white_led_fallback=namespace.allow_white_led_fallback,
        log=namespace.log,
        log_interval=namespace.log_interval,
        model_path=namespace.model_path,
        auto_download_model=namespace.auto_download_model,
    )
    settings.validate()
    return settings


# MediaPipe hand detection


@dataclass(frozen=True)
class HandObservation:
    label: str
    x: float
    y: float
    pressed: bool
    calibration_pose: bool
    landmarks: tuple[tuple[float, float], ...]
    depth: float
    scale: float


def _point_values(points: Any) -> list[tuple[float, float, float]]:
    return [
        (float(point.x), float(point.y), float(getattr(point, "z", 0.0)))
        for point in points
    ]


def _make_hand_observation(label: str, points: Any, input_mode: str) -> HandObservation:
    point_values = _point_values(points)
    index_tip = point_values[8]
    middle_tip = point_values[12]
    calibration_pose = _is_calibration_pose(point_values)
    pressed = (
        _is_open_hand(point_values)
        if input_mode in ("open", "curl")
        else index_tip[1] < (middle_tip[1] - 0.04)
    )
    anchor_x, anchor_y = _palm_anchor(point_values)
    return HandObservation(
        label=label,
        x=float(anchor_x),
        y=float(anchor_y),
        pressed=bool(pressed),
        calibration_pose=calibration_pose,
        landmarks=tuple((point[0], point[1]) for point in point_values),
        depth=_palm_depth(point_values),
        scale=_palm_scale(point_values),
    )


class ClassicHandDetector:
    def __init__(self, mp: Any, config: SenderConfig) -> None:
        mp_hands = mp.solutions.hands
        self.hands = mp_hands.Hands(
            max_num_hands=config.max_hands,
            min_detection_confidence=config.min_detect,
            min_tracking_confidence=config.min_track,
        )
        self.mp_hands = mp_hands
        self.input_mode = config.input_mode

    def __call__(self, rgb_frame: np.ndarray) -> list[HandObservation]:
        results = self.hands.process(rgb_frame)
        if not results.multi_hand_landmarks or not results.multi_handedness:
            return []
        observations = []
        for hand_landmarks, handedness in zip(results.multi_hand_landmarks, results.multi_handedness):
            label = handedness.classification[0].label
            observations.append(_make_hand_observation(label, hand_landmarks.landmark, self.input_mode))
        return observations

    def close(self) -> None:
        self.hands.close()


class TasksHandDetector:
    def __init__(self, mp: Any, config: SenderConfig) -> None:
        from mediapipe.tasks import python as mp_python_tasks
        from mediapipe.tasks.python import vision as mp_vision_tasks

        model_path = _ensure_model(Path(config.model_path), config.auto_download_model)
        base_options = mp_python_tasks.BaseOptions(model_asset_path=str(model_path))
        options = mp_vision_tasks.HandLandmarkerOptions(
            base_options=base_options,
            num_hands=config.max_hands,
            min_hand_detection_confidence=config.min_detect,
            min_hand_presence_confidence=config.min_track,
            min_tracking_confidence=config.min_track,
            running_mode=mp_vision_tasks.RunningMode.VIDEO,
        )
        self.mp = mp
        self.landmarker = mp_vision_tasks.HandLandmarker.create_from_options(options)
        self.last_timestamp_ms = 0
        self.input_mode = config.input_mode

    def __call__(self, rgb_frame: np.ndarray) -> list[HandObservation]:
        image = self.mp.Image(
            image_format=self.mp.ImageFormat.SRGB,
            data=np.ascontiguousarray(rgb_frame),
        )
        timestamp_ms = time.monotonic_ns() // 1_000_000
        if timestamp_ms <= self.last_timestamp_ms:
            timestamp_ms = self.last_timestamp_ms + 1
        self.last_timestamp_ms = timestamp_ms
        results = self.landmarker.detect_for_video(image, timestamp_ms)
        observations = []
        for index, hand_landmarks in enumerate(results.hand_landmarks):
            label = "Unknown"
            if index < len(results.handedness) and results.handedness[index]:
                label = results.handedness[index][0].category_name
            observations.append(_make_hand_observation(label, hand_landmarks, self.input_mode))
        return observations

    def close(self) -> None:
        close = getattr(self.landmarker, "close", None)
        if close is not None:
            close()


class EmptyHandDetector:
    def __call__(self, _rgb_frame: np.ndarray) -> list[HandObservation]:
        return []

    def close(self) -> None:
        return None


def _ensure_model(model_path: Path, auto_download: bool) -> Path:
    if model_path.exists():
        return model_path
    if auto_download:
        model_path.parent.mkdir(parents=True, exist_ok=True)
        partial_path = model_path.with_name(model_path.name + ".part")
        print(f"Model not found. Downloading from: {MODEL_URL}")
        try:
            urlretrieve(MODEL_URL, str(partial_path))
            partial_path.replace(model_path)
            print(f"Downloaded model to: {model_path}")
        except Exception as exc:
            try:
                partial_path.unlink()
            except OSError:
                pass
            raise RuntimeError(f"Failed to download model to {model_path}: {exc}") from exc
    if not model_path.exists():
        raise RuntimeError(
            "This MediaPipe build requires tasks API + model file. "
            f"Model not found: {model_path}. "
            "Place hand_landmarker.task in the repo root, pass --model-path <file>, "
            "or use --auto-download-model."
        )
    return model_path


def _make_hand_detector(mp: Any, args: SenderConfig) -> Any:
    """Select the classic or tasks-only MediaPipe backend."""
    if hasattr(mp, "solutions") and hasattr(mp.solutions, "hands"):
        return ClassicHandDetector(mp, args)
    try:
        return TasksHandDetector(mp, args)
    except Exception as exc:
        raise RuntimeError(
            "MediaPipe install does not expose mp.solutions and tasks API could not be initialized."
        ) from exc


# Camera discovery and frame sources


def _find_cameras(cv2: Any, maximum: int = 10) -> list[dict[str, Any]]:
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


def _camera_choice_key(position: int, total: int) -> str:
    """Use one key per choice; 0 selects the tenth entry when needed."""
    if total <= 9:
        return str(position + 1)
    return "0" if position == 9 else str(position + 1)


def _choose_camera(camera_devices: list[dict[str, Any]]) -> int:
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


def _open_scrcpy_capture(title_query: str) -> tuple[Any, dict[str, int]]:
    try:
        import mss
        import win32gui
    except ImportError as exc:
        raise RuntimeError(
            "scrcpy capture needs mss and pywin32. Install with: "
            "python -m pip install mss pywin32"
        ) from exc

    window_handle = None

    def find_window(handle: int, _unused: Any) -> None:
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


def _open_screen_capture(screen_index: Optional[int], title_query: str = "scrcpy") -> tuple[Any, dict[str, int]]:
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

    selected_screen = -1 if screen_index is None else screen_index
    if selected_screen < 0:
        if len(monitors) == 1:
            selected_screen = 1
        else:
            print("Available monitors:")
            for index, monitor in enumerate(monitors, start=1):
                print(
                    f"  [{index}] {monitor['width']}x{monitor['height']} "
                    f"at ({monitor['left']}, {monitor['top']})"
                )
            while True:
                try:
                    selected_screen = int(input("Select monitor containing scrcpy: "))
                except (ValueError, EOFError):
                    print("Enter one of the listed monitor numbers.")
                    continue
                if 1 <= selected_screen <= len(monitors):
                    break
                print("That monitor number is not available.")

    if not 1 <= selected_screen <= len(monitors):
        capture.close()
        raise RuntimeError(f"Monitor {selected_screen} is not available")

    monitor_region = monitors[selected_screen - 1]
    region = monitor_region.copy()
    try:
        import win32gui

        scrcpy_window = None

        def find_window(handle: int, _unused: Any) -> None:
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
                print(
                    f"Found scrcpy on monitor {selected_screen}; capturing its area: "
                    f"{region['width']}x{region['height']}"
                )
    except ImportError:
        print("pywin32 unavailable; capturing the entire selected monitor.")

    if region == monitor_region:
        print(f"Capturing entire monitor {selected_screen}: {region['width']}x{region['height']}")
    return capture, region


class FrameSource:
    label = "CAPTURE"

    def read(self) -> tuple[bool, np.ndarray]:
        raise NotImplementedError

    def close(self) -> None:
        raise NotImplementedError


class CameraFrameSource(FrameSource):
    def __init__(self, cv2: Any, config: SenderConfig) -> None:
        self.cv2 = cv2
        self.camera_index = config.camera_index
        self.capture = cv2.VideoCapture(config.camera_index, cv2.CAP_DSHOW)
        if not self.capture.isOpened():
            self.capture.release()
            raise RuntimeError(f"cannot open camera index {config.camera_index}")

        self.capture.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        self.capture.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
        self.capture.set(cv2.CAP_PROP_FRAME_WIDTH, config.width)
        self.capture.set(cv2.CAP_PROP_FRAME_HEIGHT, config.height)
        self.capture.set(cv2.CAP_PROP_FPS, config.fps)
        negotiated_fps = self.capture.get(cv2.CAP_PROP_FPS)
        negotiated_width = int(self.capture.get(cv2.CAP_PROP_FRAME_WIDTH))
        negotiated_height = int(self.capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
        print(
            f"Camera stream: {negotiated_width}x{negotiated_height} at "
            f"{negotiated_fps:.0f} FPS (target {config.fps:.0f})"
        )
        self.label = f"CAMERA {config.camera_index}"

    def read(self) -> tuple[bool, np.ndarray]:
        return self.capture.read()

    def close(self) -> None:
        self.capture.release()


class MssFrameSource(FrameSource):
    def __init__(self, cv2: Any, capture: Any, region: dict[str, int], label: str) -> None:
        self.cv2 = cv2
        self.capture = capture
        self.region = region
        self.label = label

    def read(self) -> tuple[bool, np.ndarray]:
        screenshot = self.capture.grab(self.region)
        frame = self.cv2.cvtColor(np.asarray(screenshot), self.cv2.COLOR_BGRA2BGR)
        return True, frame

    def close(self) -> None:
        self.capture.close()


def _create_frame_source(cv2: Any, config: SenderConfig) -> FrameSource:
    if config.scrcpy_screen is not None or config.scrcpy_window == "__screen__":
        capture, region = _open_screen_capture(config.scrcpy_screen)
        return MssFrameSource(cv2, capture, region, "SCRCPY MONITOR")
    if config.scrcpy_window:
        capture, region = _open_scrcpy_capture(config.scrcpy_window)
        return MssFrameSource(cv2, capture, region, "SCRCPY WINDOW")

    camera_index = config.camera_index
    if camera_index < 0:
        available_cameras = _find_cameras(cv2)
        if not available_cameras:
            raise RuntimeError("no cameras found")
        camera_index = _choose_camera(available_cameras)
        config = SenderConfig(**{**config.__dict__, "camera_index": camera_index})
    return CameraFrameSource(cv2, config)


# Geometry and LED detection


def _is_calibration_pose(landmarks: Any) -> bool:
    """Detect a peace sign: index and middle extended, ring and pinky curled."""
    curled = all(
        landmarks[tip][1] > landmarks[pip][1]
        for tip, pip in ((16, 14), (20, 18))
    )
    index_extended = landmarks[8][1] < landmarks[6][1]
    middle_extended = landmarks[12][1] < landmarks[10][1]
    return curled and index_extended and middle_extended


def _is_open_hand(landmarks: Any) -> bool:
    """Treat three or more extended fingers as touch-down."""
    extended_fingers = sum(
        landmarks[tip][1] < landmarks[pip][1]
        for tip, pip in ((8, 6), (12, 10), (16, 14), (20, 18))
    )
    return extended_fingers >= 3


def _palm_anchor(landmarks: Any) -> tuple[float, float]:
    weighted_nodes = ((0, 1.0), (2, 2.0), (5, 1.0), (17, 1.0))
    total_weight = sum(weight for _, weight in weighted_nodes)
    anchor_x = sum(landmarks[index][0] * weight for index, weight in weighted_nodes) / total_weight
    anchor_y = sum(landmarks[index][1] * weight for index, weight in weighted_nodes) / total_weight
    return anchor_x, anchor_y


def _palm_depth(landmarks: Any) -> float:
    weighted_nodes = ((0, 1.0), (2, 2.0), (5, 1.0), (17, 1.0))
    total_weight = sum(weight for _, weight in weighted_nodes)
    return sum(landmarks[index][2] * weight for index, weight in weighted_nodes) / total_weight


def _palm_scale(landmarks: Any) -> float:
    across_palm = math.hypot(
        landmarks[5][0] - landmarks[17][0],
        landmarks[5][1] - landmarks[17][1],
    )
    palm_length = math.hypot(
        landmarks[0][0] - landmarks[9][0],
        landmarks[0][1] - landmarks[9][1],
    )
    return (across_palm + palm_length) / 2.0


def _calibrate_position(
    norm_x: float,
    norm_y: float,
    left_edge: float,
    right_edge: float,
    center_y: float,
    frame_width: int,
    frame_height: int,
) -> tuple[float, float]:
    diameter_x = max(0.1, right_edge - left_edge)
    radius_pixels = (diameter_x * frame_width) / 2.0
    diameter_y = max(0.1, (radius_pixels * 2.0) / frame_height)
    center_x = (left_edge + right_edge) / 2.0
    calibrated_x = 0.5 + (norm_x - center_x) / diameter_x
    calibrated_y = 0.5 + (norm_y - center_y) / diameter_y
    offset_x = calibrated_x - 0.5
    offset_y = calibrated_y - 0.5
    distance = math.hypot(offset_x, offset_y)
    if distance > 0.5:
        scale = 0.5 / distance
        offset_x *= scale
        offset_y *= scale
    return max(0.0, min(1.0, 0.5 + offset_x)), max(0.0, min(1.0, 0.5 + offset_y))


def _measure_led_ambient(cv2: Any, frame: np.ndarray) -> dict[str, float]:
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    return {
        "value": float(np.median(hsv[:, :, 2])),
        "saturation": float(np.median(hsv[:, :, 1])),
    }


def _sample_led_color(cv2: Any, frame: np.ndarray, center_x: float = 0.5, center_y: float = 0.5) -> Optional[dict[str, float]]:
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    frame_height, frame_width = hsv.shape[:2]
    radius = max(6, min(frame_width, frame_height) // 16)
    sample_x = int(center_x * frame_width)
    sample_y = int(center_y * frame_height)
    x_start = max(0, sample_x - radius)
    x_end = min(frame_width, sample_x + radius + 1)
    y_start = max(0, sample_y - radius)
    y_end = min(frame_height, sample_y + radius + 1)
    pixels = hsv[y_start:y_end, x_start:x_end].reshape(-1, 3).astype(np.float32)
    if pixels.size == 0:
        return None

    saturation_cutoff = np.percentile(pixels[:, 1], 50)
    value_cutoff = np.percentile(pixels[:, 2], 60)
    selected = pixels[
        (pixels[:, 1] >= max(20.0, saturation_cutoff))
        & (pixels[:, 2] >= value_cutoff)
    ]
    if selected.size == 0:
        selected = pixels

    angles = selected[:, 0] * (math.pi / 90.0)
    weights = np.maximum(1.0, selected[:, 1] * selected[:, 2])
    hue = (
        math.atan2(
            float(np.sum(np.sin(angles) * weights)),
            float(np.sum(np.cos(angles) * weights)),
        )
        * 90.0
        / math.pi
    ) % 180.0
    return {
        "hue": hue,
        "saturation": float(np.percentile(selected[:, 1], 75)),
        "value": float(np.percentile(selected[:, 2], 75)),
    }


def _average_led_profiles(profiles: list[dict[str, float]]) -> Optional[dict[str, float]]:
    if not profiles:
        return None
    angles = np.asarray([profile["hue"] for profile in profiles], dtype=np.float32) * (math.pi / 90.0)
    hue = (
        math.atan2(float(np.mean(np.sin(angles))), float(np.mean(np.cos(angles))))
        * 90.0
        / math.pi
    ) % 180.0
    return {
        "hue": hue,
        "saturation": float(np.median([profile["saturation"] for profile in profiles])),
        "value": float(np.median([profile["value"] for profile in profiles])),
    }


def _average_led_ambient(samples: list[dict[str, float]]) -> Optional[dict[str, float]]:
    if not samples:
        return None
    return {
        "value": float(np.median([sample["value"] for sample in samples])),
        "saturation": float(np.median([sample["saturation"] for sample in samples])),
    }


def _draw_led_crosshair(cv2: Any, frame: np.ndarray, label: str) -> None:
    frame_height, frame_width = frame.shape[:2]
    center = (frame_width // 2, frame_height // 2)
    color = (0, 255, 255)
    size = max(12, min(frame_width, frame_height) // 12)
    cv2.line(frame, (center[0] - size, center[1]), (center[0] + size, center[1]), color, 2, cv2.LINE_AA)
    cv2.line(frame, (center[0], center[1] - size), (center[0], center[1] + size), color, 2, cv2.LINE_AA)
    cv2.circle(frame, center, size // 2, color, 1, cv2.LINE_AA)
    cv2.putText(
        frame,
        f"RELEASE TO SET {label}",
        (max(10, center[0] - 130), center[1] - size - 12),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.55,
        color,
        2,
        cv2.LINE_AA,
    )


def _find_led_candidate(
    cv2: Any,
    hsv: np.ndarray,
    mask: np.ndarray,
    frame_shape: tuple[int, ...],
    expected_x: float,
) -> Optional[tuple[float, float, Any]]:
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    candidates = []
    frame_height, frame_width = frame_shape[:2]
    for contour in contours:
        area = cv2.contourArea(contour)
        x, y, width, height = cv2.boundingRect(contour)
        if area < 4 or width < 2 or height < 2:
            continue
        roi = hsv[y:y + height, x:x + width]
        brightness = float(roi[:, :, 2].mean()) if roi.size else 0.0
        saturation = float(roi[:, :, 1].mean()) if roi.size else 0.0
        center_x = (x + width / 2.0) / frame_width
        position_score = max(0.05, 1.0 - abs(center_x - expected_x) * 3.0)
        score = area * (brightness / 255.0) * (saturation / 255.0) * position_score
        candidates.append((score, contour))
    if not candidates:
        return None
    contour = max(candidates, key=lambda candidate: candidate[0])[1]
    x, y, width, height = cv2.boundingRect(contour)
    return (
        float((x + width / 2.0) / frame_width),
        float((y + height / 2.0) / frame_height),
        contour,
    )


def _mask_from_color_ranges(cv2: Any, hsv: np.ndarray, hue_ranges: list[tuple[tuple[int, int, int], tuple[int, int, int]]]) -> np.ndarray:
    mask = None
    for lower, upper in hue_ranges:
        part = cv2.inRange(hsv, lower, upper)
        mask = part if mask is None else cv2.bitwise_or(mask, part)
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, None, iterations=3)
    return cv2.dilate(mask, None, iterations=1)


def _mask_from_profile(cv2: Any, hsv: np.ndarray, profile: dict[str, float], ambient_profile: Optional[dict[str, float]]) -> np.ndarray:
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
    return cv2.dilate(mask, None, iterations=1)


def _find_white_bars(cv2: Any, hsv: np.ndarray, frame_shape: tuple[int, ...]) -> list[tuple[float, float, Any, float]]:
    mask = cv2.inRange(hsv, (0, 0, 220), (180, 90, 255))
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, None, iterations=2)
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    frame_height, frame_width = frame_shape[:2]
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
            float(moments["m10"] / moments["m00"] / frame_width),
            float(moments["m01"] / moments["m00"] / frame_height),
            contour,
            area * min(aspect, 10.0),
        ))
    return sorted(bars, key=lambda bar: bar[3], reverse=True)


def _detect_led_positions(
    cv2: Any,
    frame: np.ndarray,
    allow_white_fallback: bool = False,
    ambient_profile: Optional[dict[str, float]] = None,
    color_profiles: Optional[dict[str, dict[str, float]]] = None,
) -> tuple[Optional[tuple[float, float, Any]], Optional[tuple[float, float, Any]]]:
    """Track independently colored lightbars, using runtime samples when available."""
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    color_profiles = color_profiles or {}
    left_profile = color_profiles.get("Left")
    right_profile = color_profiles.get("Right")

    if left_profile:
        left_mask = _mask_from_profile(cv2, hsv, left_profile, ambient_profile)
    else:
        left_mask = _mask_from_color_ranges(cv2, hsv, [((112, 150, 55), (130, 255, 255))])
    if right_profile:
        right_mask = _mask_from_profile(cv2, hsv, right_profile, ambient_profile)
    else:
        right_mask = _mask_from_color_ranges(
            cv2,
            hsv,
            [((0, 120, 55), (10, 255, 255)), ((170, 120, 55), (180, 255, 255))],
        )

    left = _find_led_candidate(cv2, hsv, left_mask, frame.shape, expected_x=0.25)
    right = _find_led_candidate(cv2, hsv, right_mask, frame.shape, expected_x=0.75)
    if not allow_white_fallback:
        return left, right

    white_bars = _find_white_bars(cv2, hsv, frame.shape)
    if left is None and right is None and len(white_bars) >= 2:
        selected_bars = sorted(white_bars[:2], key=lambda bar: bar[0])
        left = selected_bars[0][:3]
        right = selected_bars[1][:3]
    elif left is None and white_bars:
        left = min(white_bars, key=lambda bar: abs(bar[0] - 0.25))[:3]
    elif right is None and white_bars:
        right = min(white_bars, key=lambda bar: abs(bar[0] - 0.75))[:3]
    return left, right


# Runtime state


@dataclass
class CircleCalibration:
    left_edge: Optional[float] = None
    right_edge: Optional[float] = None
    center_y: float = 0.5

    @property
    def valid(self) -> bool:
        return (
            self.left_edge is not None
            and self.right_edge is not None
            and self.right_edge - self.left_edge >= 0.1
        )

    def set_edges(self, left_edge: float, right_edge: float, center_y: float) -> bool:
        if right_edge - left_edge < 0.1:
            return False
        self.left_edge = left_edge
        self.right_edge = right_edge
        self.center_y = center_y
        return True

    def apply(self, norm_x: float, norm_y: float, frame_width: int, frame_height: int) -> tuple[float, float]:
        if not self.valid:
            return norm_x, norm_y
        return _calibrate_position(
            norm_x,
            norm_y,
            self.left_edge,
            self.right_edge,
            self.center_y,
            frame_width,
            frame_height,
        )


@dataclass(frozen=True)
class CalibrationSample:
    left_edge: float
    right_edge: float
    center_y: float
    rest_depths: dict[str, float]
    rest_scales: dict[str, float]
    push_threshold: float


class HandCalibrationController:
    def __init__(self) -> None:
        self.armed = False
        self.active = False
        self.rearm_required = False
        self.pending: Optional[CalibrationSample] = None

    def request(self) -> None:
        self.armed = True
        self.rearm_required = False

    def update(self, observations: list[HandObservation], push_threshold: float) -> Optional[CalibrationSample]:
        calibration_points = [
            observation for observation in observations if observation.calibration_pose
        ]
        if not calibration_points:
            self.rearm_required = False
        elif len(calibration_points) >= 2 and not self.armed and not self.rearm_required:
            self.armed = True
            print("Calibration gesture detected. Hold position, then release to set the circle.")

        gesture_active = self.armed and len(calibration_points) >= 2
        applied_sample = None
        if gesture_active:
            candidate_left = min(point.x for point in calibration_points)
            candidate_right = max(point.x for point in calibration_points)
            if candidate_right - candidate_left >= 0.1:
                self.pending = CalibrationSample(
                    left_edge=candidate_left,
                    right_edge=candidate_right,
                    center_y=sum(point.y for point in calibration_points) / len(calibration_points),
                    rest_depths={point.label: point.depth for point in calibration_points},
                    rest_scales={point.label: point.scale for point in calibration_points},
                    push_threshold=push_threshold,
                )
            if not self.active:
                print("Calibration gesture active. Hold position, then release to set the circle.")
        elif self.active:
            if self.pending is not None:
                applied_sample = self.pending
                self.armed = False
                self.rearm_required = True
                print("Camera calibrated: circle set from released gesture.")
            self.pending = None
        self.active = gesture_active
        return applied_sample


@dataclass
class OutputState:
    left_x: float = 0.5
    left_y: float = 0.5
    left_pressed: int = 0
    right_x: float = 0.5
    right_y: float = 0.5
    right_pressed: int = 0


@dataclass
class TrackingState:
    positions: dict[str, list[float]] = field(
        default_factory=lambda: {"Left": [0.5, 0.5], "Right": [0.5, 0.5]}
    )
    pressed: dict[str, int] = field(default_factory=lambda: {"Left": 0, "Right": 0})
    last_seen: dict[str, float] = field(default_factory=lambda: {"Left": 0.0, "Right": 0.0})
    push_states: dict[str, bool] = field(default_factory=lambda: {"Left": False, "Right": False})
    push_metrics: dict[str, float] = field(default_factory=lambda: {"Left": 0.0, "Right": 0.0})
    rest_depths: dict[str, float] = field(default_factory=dict)
    rest_scales: dict[str, float] = field(default_factory=dict)
    push_threshold: float = 0.025
    push_release_threshold: float = 0.012

    def defaults(self) -> OutputState:
        return OutputState(
            left_x=self.positions["Left"][0],
            left_y=self.positions["Left"][1],
            left_pressed=self.pressed["Left"],
            right_x=self.positions["Right"][0],
            right_y=self.positions["Right"][1],
            right_pressed=self.pressed["Right"],
        )

    def expire_lost(self, now: float, timeout: float) -> None:
        for label in HAND_LABELS:
            if now - self.last_seen[label] <= timeout:
                continue
            self.positions[label] = [0.5, 0.5]
            self.pressed[label] = 0
            self.push_states[label] = False
            self.push_metrics[label] = 0.0

    def reset_press(self, label: str) -> None:
        self.pressed[label] = 0
        self.push_states[label] = False
        self.push_metrics[label] = 0.0

    def apply_calibration(self, sample: CalibrationSample) -> None:
        self.rest_depths = sample.rest_depths.copy()
        self.rest_scales = sample.rest_scales.copy()
        self.push_threshold = sample.push_threshold
        self.push_release_threshold = sample.push_threshold * 0.5
        self.pressed = {"Left": 0, "Right": 0}
        self.push_states = {"Left": False, "Right": False}
        self.push_metrics = {"Left": 0.0, "Right": 0.0}

    def update_position(self, label: str, norm_x: float, norm_y: float, pressed: bool, now: float) -> None:
        self.positions[label] = [max(0.0, min(1.0, norm_x)), max(0.0, min(1.0, norm_y))]
        self.pressed[label] = 1 if pressed else 0
        self.last_seen[label] = now


@dataclass
class LedTracker:
    filtered: dict[str, Optional[tuple[float, float]]] = field(
        default_factory=lambda: {"Left": None, "Right": None}
    )
    jump_candidates: dict[str, Optional[tuple[float, float, int, float]]] = field(
        default_factory=lambda: {"Left": None, "Right": None}
    )

    def accept(
        self,
        label: str,
        position: tuple[float, float],
        now: float,
        confirmations: int,
        match_distance: float,
    ) -> bool:
        previous = self.filtered[label]
        is_large_jump = previous is not None and (
            abs(position[0] - previous[0]) > 0.45
            or abs(position[1] - previous[1]) > 0.35
        )
        if not is_large_jump:
            self.jump_candidates[label] = None
            self.filtered[label] = position
            return True

        pending = self.jump_candidates[label]
        is_continuous = (
            pending is not None
            and now - pending[3] <= 0.3
            and math.hypot(position[0] - pending[0], position[1] - pending[1])
            <= max(0.01, match_distance)
        )
        count = pending[2] + 1 if is_continuous else 1
        if count < max(1, confirmations):
            self.jump_candidates[label] = (position[0], position[1], count, now)
            return False
        self.jump_candidates[label] = None
        self.filtered[label] = position
        return True


@dataclass
class LedSamplingController:
    action: Optional[str] = None
    release: Optional[str] = None
    ambient_samples: list[dict[str, float]] = field(default_factory=list)
    color_samples: dict[str, list[dict[str, float]]] = field(
        default_factory=lambda: {"Left": [], "Right": []}
    )

    def handle_command(self, action: str, enabled: bool) -> None:
        if enabled:
            self.action = action
            self.release = None
            self.ambient_samples.clear()
            self.color_samples["Left"].clear()
            self.color_samples["Right"].clear()
            print(f"LED {action} sampling armed. Hold the crosshair over the target, then release.")
        elif self.action == action:
            self.release = action
            self.action = None

    def observe(self, cv2: Any, frame: np.ndarray) -> Optional[tuple[str, Optional[dict[str, float]]]]:
        if self.action == "ambient":
            self.ambient_samples.append(_measure_led_ambient(cv2, frame))
        elif self.action in ("left", "right"):
            sample = _sample_led_color(cv2, frame)
            if sample is not None:
                self.color_samples["Left" if self.action == "left" else "Right"].append(sample)

        if self.release is None:
            return None
        action = self.release
        self.release = None
        if action == "ambient":
            if not self.ambient_samples:
                self.ambient_samples.append(_measure_led_ambient(cv2, frame))
            result = _average_led_ambient(self.ambient_samples)
            self.ambient_samples.clear()
            self.color_samples["Left"].clear()
            self.color_samples["Right"].clear()
            return action, result

        label = "Left" if action == "left" else "Right"
        samples = self.color_samples[label]
        if not samples:
            sample = _sample_led_color(cv2, frame)
            if sample is not None:
                samples.append(sample)
        result = _average_led_profiles(samples)
        self.ambient_samples.clear()
        self.color_samples["Left"].clear()
        self.color_samples["Right"].clear()
        return action, result


@dataclass
class RuntimeState:
    circle: CircleCalibration = field(default_factory=CircleCalibration)
    tracking: TrackingState = field(default_factory=TrackingState)
    hand_calibration: HandCalibrationController = field(default_factory=HandCalibrationController)
    led_tracker: LedTracker = field(default_factory=LedTracker)
    led_sampling: LedSamplingController = field(default_factory=LedSamplingController)
    debug_visible: bool = True
    led_calibration_armed: bool = False
    ambient_profile: Optional[dict[str, float]] = None
    led_color_profiles: dict[str, dict[str, float]] = field(default_factory=dict)


@dataclass
class FrameStats:
    window_start: float = field(default_factory=time.perf_counter)
    captured: int = 0
    sent: int = 0
    capture_fps: float = 0.0
    send_fps: float = 0.0

    def record_capture(self) -> None:
        self.captured += 1

    def record_send(self) -> None:
        self.sent += 1

    def update(self, now: float) -> None:
        elapsed = now - self.window_start
        if elapsed < 1.0:
            return
        self.capture_fps = self.captured / elapsed
        self.send_fps = self.sent / elapsed
        self.captured = 0
        self.sent = 0
        self.window_start = now


# UDP endpoints


class UdpSender:
    def __init__(self, host: str, port: int) -> None:
        self.address = (host, port)
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.setblocking(False)

    def send(self, message: str) -> bool:
        try:
            self.socket.sendto(message.encode("ascii"), self.address)
        except (BlockingIOError, OSError):
            return False
        return True

    def close(self) -> None:
        self.socket.close()


class ControlReceiver:
    def __init__(self, port: int) -> None:
        self.socket: Optional[socket.socket]
        try:
            control_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            control_socket.bind(("127.0.0.1", port))
            control_socket.setblocking(False)
            self.socket = control_socket
        except OSError as exc:
            print(f"Warning: debug control unavailable on port {port}: {exc}")
            try:
                control_socket.close()
            except (UnboundLocalError, OSError):
                pass
            self.socket = None

    def receive_all(self) -> list[str]:
        if self.socket is None:
            return []
        commands = []
        while True:
            try:
                command = self.socket.recv(128).decode("ascii", errors="ignore").strip()
            except BlockingIOError:
                break
            except OSError:
                break
            commands.append(command)
        return commands

    def close(self) -> None:
        if self.socket is not None:
            self.socket.close()
            self.socket = None


class ReadyNotifier:
    @staticmethod
    def notify(port: int) -> None:
        status_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            status_socket.sendto(b"PYTHON_READY", ("127.0.0.1", port))
        except OSError as exc:
            print(f"Warning: could not send startup status: {exc}")
        finally:
            status_socket.close()


# Preview and application lifecycle


def _observation_fields(observation: Any) -> tuple[str, float, float, bool, bool, Any, float, float]:
    if isinstance(observation, HandObservation):
        return (
            observation.label,
            observation.x,
            observation.y,
            observation.pressed,
            observation.calibration_pose,
            observation.landmarks,
            observation.depth,
            observation.scale,
        )
    return observation


def _draw_hand_skeleton(cv2: Any, frame: np.ndarray, hand_results: list[Any]) -> None:
    frame_height, frame_width = frame.shape[:2]
    for observation in hand_results:
        hand_label, marker_x, marker_y, pressed, calibration_pose, landmarks, _, _ = _observation_fields(observation)
        color = (0, 220, 0) if hand_label == "Left" else (220, 80, 255)
        points = [(int(point[0] * frame_width), int(point[1] * frame_height)) for point in landmarks]
        for start, end in HAND_CONNECTIONS:
            cv2.line(frame, points[start], points[end], color, 2, cv2.LINE_AA)
        for point in points:
            cv2.circle(frame, point, 3, (255, 255, 255), -1, cv2.LINE_AA)
        marker = (int(marker_x * frame_width), int(marker_y * frame_height))
        cv2.circle(frame, marker, 11, (0, 255, 255) if calibration_pose else color, 2, cv2.LINE_AA)
        state = "CALIBRATE" if calibration_pose else ("CLICK" if pressed else "READY")
        cv2.putText(
            frame,
            f"{hand_label}: {state}",
            (marker[0] + 14, marker[1] - 10),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.5,
            color,
            1,
            cv2.LINE_AA,
        )


def _format_packet(output: OutputState, controller_clicks: int) -> str:
    return (
        f"{output.left_x:.4f},{output.left_y:.4f},{output.left_pressed},"
        f"{output.right_x:.4f},{output.right_y:.4f},{output.right_pressed},"
        f"{controller_clicks}"
    )


class CameraSenderApp:
    def __init__(self, config: SenderConfig) -> None:
        import cv2

        self.cv2 = cv2
        self.config = config
        self.source: Optional[FrameSource] = None
        self.udp_sender: Optional[UdpSender] = None
        self.control_receiver: Optional[ControlReceiver] = None
        self.hand_detector: Any = None
        self.state = RuntimeState()
        self.stats = FrameStats()
        self.sent_count = 0
        self.last_send = 0.0
        self.last_log = 0.0
        try:
            self.source = _create_frame_source(cv2, config)
            self.udp_sender = UdpSender(config.host, config.port)
            self.control_receiver = ControlReceiver(config.control_port)
            if config.input_mode == "ds4led":
                self.hand_detector = EmptyHandDetector()
                print("DS4 LED mode: hand detection disabled; using lightbar plus physical L1/R1.")
            else:
                import mediapipe as mp

                self.hand_detector = _make_hand_detector(mp, config)
        except Exception:
            self.close()
            raise

        source_name = "DS4 LED + L1/R1" if config.input_mode == "ds4led" else f"hand {config.input_mode}"
        print(f"Sending {source_name} data to {config.host}:{config.port}")
        print("Press 'q' in preview window to quit." if config.preview else "Press Ctrl+C to quit.")

    def close(self) -> None:
        if self.hand_detector is not None:
            self.hand_detector.close()
            self.hand_detector = None
        if self.source is not None:
            self.source.close()
            self.source = None
        if self.udp_sender is not None:
            self.udp_sender.close()
            self.udp_sender = None
        if self.control_receiver is not None:
            self.control_receiver.close()
            self.control_receiver = None
        if hasattr(self, "cv2"):
            self.cv2.destroyAllWindows()

    def run(self) -> int:
        min_frame_dt = 1.0 / max(1.0, self.config.fps)
        hand_lost_timeout = 3.0
        ReadyNotifier.notify(self.config.status_port)
        try:
            while True:
                self._handle_control_commands()
                ok, frame = self.source.read()
                if not ok:
                    time.sleep(0.01)
                    continue

                self.stats.record_capture()
                frame = self.cv2.flip(frame, 1)
                now = time.perf_counter()
                self.state.tracking.expire_lost(now, hand_lost_timeout)
                hand_results: list[HandObservation] = []
                led_positions = (None, None)
                output = self.state.tracking.defaults()

                if self.config.input_mode == "ds4led":
                    sample_result = self.state.led_sampling.observe(self.cv2, frame)
                    self._apply_led_sample(sample_result)
                    led_positions = self._detect_and_update_leds(frame, output, now)
                else:
                    rgb_frame = self.cv2.cvtColor(frame, self.cv2.COLOR_BGR2RGB)
                    hand_results = self.hand_detector(rgb_frame)
                    calibration_sample = self.state.hand_calibration.update(
                        hand_results,
                        self.config.push_threshold,
                    )
                    if calibration_sample is not None:
                        self.state.circle.set_edges(
                            calibration_sample.left_edge,
                            calibration_sample.right_edge,
                            calibration_sample.center_y,
                        )
                        self.state.tracking.apply_calibration(calibration_sample)
                        output.left_pressed = 0
                        output.right_pressed = 0
                    self._update_hand_tracking(hand_results, output, now, frame.shape)

                calibration_armed = (
                    self.state.led_calibration_armed
                    if self.config.input_mode == "ds4led"
                    else self.state.hand_calibration.armed
                )
                if calibration_armed:
                    output.left_pressed = 0
                    output.right_pressed = 0
                    self.state.tracking.pressed["Left"] = 0
                    self.state.tracking.pressed["Right"] = 0

                self._send_output(output, now, min_frame_dt)
                self.stats.update(now)
                should_quit = self._render_preview(frame, output, hand_results, led_positions)
                if should_quit:
                    break
        except KeyboardInterrupt:
            pass
        finally:
            self.close()
        return 0

    def _handle_control_commands(self) -> None:
        if self.control_receiver is None:
            return
        for command in self.control_receiver.receive_all():
            if command == "DEBUG 1":
                self.state.debug_visible = True
            elif command == "DEBUG 0":
                self.state.debug_visible = False
            elif command in ("CALIBRATE", "CALIBRATE CIRCLE"):
                if self.config.input_mode == "ds4led":
                    self.state.led_calibration_armed = True
                    print("DS4 LED circle calibration armed.")
                else:
                    self.state.hand_calibration.request()
                    print("Calibration armed. Show peace signs with both hands at neutral rest depth, then release.")
            elif command.startswith("LED "):
                parts = command.split()
                if len(parts) == 3 and parts[1] in ("AMBIENT", "LEFT", "RIGHT") and parts[2] in ("0", "1"):
                    self.state.led_sampling.handle_command(parts[1].lower(), parts[2] == "1")

    def _apply_led_sample(self, sample_result: Optional[tuple[str, Optional[dict[str, float]]]]) -> None:
        if sample_result is None:
            return
        action, profile = sample_result
        if action == "ambient":
            if profile is not None:
                self.state.ambient_profile = profile
                print(
                    f"LED ambient set: value {profile['value']:.0f}, "
                    f"saturation {profile['saturation']:.0f}."
                )
            return

        label = "Left" if action == "left" else "Right"
        if profile is None:
            print(
                f"LED {action} color sample failed; keep the crosshair over the "
                "lightbar and try again."
            )
            return
        self.state.led_color_profiles[label] = profile
        print(
            f"LED {action} color set: hue {profile['hue']:.1f}, "
            f"saturation {profile['saturation']:.0f}, value {profile['value']:.0f}."
        )

    def _detect_and_update_leds(
        self,
        frame: np.ndarray,
        output: OutputState,
        now: float,
    ) -> tuple[Optional[tuple[float, float, Any]], Optional[tuple[float, float, Any]]]:
        led_positions = _detect_led_positions(
            self.cv2,
            frame,
            allow_white_fallback=self.config.allow_white_led_fallback,
            ambient_profile=self.state.ambient_profile,
            color_profiles=self.state.led_color_profiles,
        )
        if self.state.led_calibration_armed and led_positions[0] is not None and led_positions[1] is not None:
            left_led_x, left_led_y, _ = led_positions[0]
            right_led_x, right_led_y, _ = led_positions[1]
            left_edge = min(left_led_x, right_led_x)
            right_edge = max(left_led_x, right_led_x)
            if self.state.circle.set_edges(left_edge, right_edge, (left_led_y + right_led_y) / 2.0):
                self.state.led_calibration_armed = False
                print("DS4 LED calibrated: circle set from blue/red lightbars.")

        for label, led in zip(HAND_LABELS, led_positions):
            if led is None:
                continue
            led_x, led_y, contour = led
            if not self.state.led_tracker.accept(
                label,
                (led_x, led_y),
                now,
                self.config.led_jump_confirmations,
                self.config.led_jump_match_distance,
            ):
                continue
            calibrated_x, calibrated_y = self.state.circle.apply(
                led_x,
                led_y,
                frame.shape[1],
                frame.shape[0],
            )
            self.state.tracking.update_position(label, calibrated_x, calibrated_y, False, now)
            if label == "Left":
                output.left_x, output.left_y = calibrated_x, calibrated_y
            else:
                output.right_x, output.right_y = calibrated_x, calibrated_y
            if self.config.preview and self.state.debug_visible:
                led_color = (255, 120, 0) if label == "Left" else (0, 0, 255)
                self.cv2.drawContours(frame, [contour], -1, led_color, 2)
        return led_positions

    def _update_hand_tracking(
        self,
        hand_results: list[HandObservation],
        output: OutputState,
        now: float,
        frame_shape: tuple[int, ...],
    ) -> None:
        frame_height, frame_width = frame_shape[:2]
        for observation in hand_results:
            if observation.label not in HAND_LABELS:
                continue
            label = observation.label
            self.state.tracking.last_seen[label] = now
            if observation.calibration_pose:
                self.state.tracking.reset_press(label)
                if label == "Left":
                    output.left_pressed = 0
                else:
                    output.right_pressed = 0
                continue

            pressed = observation.pressed
            if self.config.input_mode == "push":
                baseline_depth = self.state.tracking.rest_depths.get(label)
                baseline_scale = self.state.tracking.rest_scales.get(label)
                if baseline_depth is None or baseline_scale is None or baseline_scale <= 0:
                    pressed = False
                    self.state.tracking.reset_press(label)
                else:
                    depth_growth = baseline_depth - observation.depth
                    self.state.tracking.push_metrics[label] = depth_growth
                    threshold = (
                        self.state.tracking.push_release_threshold
                        if self.state.tracking.push_states[label]
                        else self.state.tracking.push_threshold
                    )
                    pressed = depth_growth >= threshold
                    self.state.tracking.push_states[label] = pressed

            calibrated_x, calibrated_y = self.state.circle.apply(
                observation.x,
                observation.y,
                frame_width,
                frame_height,
            )
            self.state.tracking.update_position(label, calibrated_x, calibrated_y, pressed, now)
            if label == "Left":
                output.left_x, output.left_y, output.left_pressed = calibrated_x, calibrated_y, int(pressed)
            else:
                output.right_x, output.right_y, output.right_pressed = calibrated_x, calibrated_y, int(pressed)

    def _send_output(self, output: OutputState, now: float, min_frame_dt: float) -> None:
        if now - self.last_send < min_frame_dt:
            return
        controller_clicks = 1 if self.config.input_mode == "ds4led" else 0
        message = _format_packet(output, controller_clicks)
        if self.udp_sender is not None and self.udp_sender.send(message):
            self.sent_count += 1
            self.stats.record_send()
        self.last_send = now
        if self.config.log and now - self.last_log >= max(0.05, self.config.log_interval):
            print(f"[{self.sent_count}] {message}")
            self.last_log = now

    def _render_preview(
        self,
        frame: np.ndarray,
        output: OutputState,
        hand_results: list[HandObservation],
        led_positions: tuple[Optional[tuple[float, float, Any]], Optional[tuple[float, float, Any]]],
    ) -> bool:
        if not self.config.preview:
            return False
        frame_height, frame_width = frame.shape[:2]
        if self.state.debug_visible:
            if self.state.circle.valid:
                circle_center_x = int(((self.state.circle.left_edge + self.state.circle.right_edge) / 2.0) * frame_width)
                circle_center_y = int(self.state.circle.center_y * frame_height)
                circle_radius = int(((self.state.circle.right_edge - self.state.circle.left_edge) / 2.0) * frame_width)
                self.cv2.circle(frame, (circle_center_x, circle_center_y), circle_radius, (0, 255, 255), 2, self.cv2.LINE_AA)
            if self.config.input_mode != "ds4led":
                _draw_hand_skeleton(self.cv2, frame, hand_results)
            if output.left_x > 0.0 or output.left_y > 0.0:
                self._draw_marker(frame, output.left_x, output.left_y, output.left_pressed, (0, 255, 0), "L")
            if output.right_x > 0.0 or output.right_y > 0.0:
                self._draw_marker(frame, output.right_x, output.right_y, output.right_pressed, (0, 0, 255), "R")

            self.cv2.putText(
                frame,
                f"{self.source.label} {frame.shape[1]}x{frame.shape[0]}",
                (10, 24),
                self.cv2.FONT_HERSHEY_SIMPLEX,
                0.6,
                (255, 255, 255),
                1,
            )
            self.cv2.putText(
                frame,
                f"Capture: {self.stats.capture_fps:.1f} FPS | Send: {self.stats.send_fps:.1f} FPS",
                (10, 48),
                self.cv2.FONT_HERSHEY_SIMPLEX,
                0.55,
                (255, 255, 255),
                1,
            )
            push_metrics = self.state.tracking.push_metrics
            self.cv2.putText(
                frame,
                f"Push depth L:{push_metrics['Left']:.2f} R:{push_metrics['Right']:.2f} / {self.state.tracking.push_threshold:.2f}",
                (10, 96),
                self.cv2.FONT_HERSHEY_SIMPLEX,
                0.5,
                (255, 255, 255),
                1,
            )
            if self.config.input_mode == "ds4led":
                calibration_status = "SETTING... SHOW LIGHTBARS" if self.state.led_calibration_armed else (
                    "CALIBRATED" if self.state.circle.valid else "NOT CALIBRATED"
                )
            elif self.state.hand_calibration.armed and not self.state.hand_calibration.active:
                calibration_status = "PEACE: TWO HANDS"
            elif self.state.hand_calibration.active:
                calibration_status = "SETTING... RELEASE TO APPLY"
            else:
                calibration_status = "CALIBRATED" if self.state.circle.valid else "NOT CALIBRATED"
            self.cv2.putText(
                frame,
                f"Mode: {self.config.input_mode} | Circle: {calibration_status}",
                (10, 72),
                self.cv2.FONT_HERSHEY_SIMPLEX,
                0.55,
                (0, 255, 255) if self.state.circle.valid else (180, 180, 180),
                1,
            )
            if self.config.input_mode == "ds4led" and self.state.led_sampling.action:
                _draw_led_crosshair(self.cv2, frame, self.state.led_sampling.action.upper())
            if self.config.input_mode == "ds4led":
                profile_status = (
                    f"LED profiles A:{'Y' if self.state.ambient_profile else 'N'} "
                    f"L:{'Y' if 'Left' in self.state.led_color_profiles else 'N'} "
                    f"R:{'Y' if 'Right' in self.state.led_color_profiles else 'N'}"
                )
                self.cv2.putText(
                    frame,
                    profile_status,
                    (10, 120),
                    self.cv2.FONT_HERSHEY_SIMPLEX,
                    0.5,
                    (255, 255, 255),
                    1,
                )
        self.cv2.imshow("Camera Sender", frame)
        return (self.cv2.waitKey(1) & 0xFF) == ord("q")

    def _draw_marker(
        self,
        frame: np.ndarray,
        norm_x: float,
        norm_y: float,
        pressed: int,
        color: tuple[int, int, int],
        label: str,
    ) -> None:
        marker_x = int(norm_x * frame.shape[1])
        marker_y = int(norm_y * frame.shape[0])
        self.cv2.circle(frame, (marker_x, marker_y), 10, color, -1)
        self.cv2.putText(
            frame,
            f"{label}:{pressed}",
            (marker_x + 8, marker_y - 8),
            self.cv2.FONT_HERSHEY_SIMPLEX,
            0.5,
            color,
            1,
        )


def main(argv: Optional[list[str]] = None) -> int:
    try:
        config = parse_args(argv)
    except RuntimeError as exc:
        print(f"Error: {exc}")
        return 1

    try:
        import cv2
    except Exception as exc:
        print("Missing dependencies. Install with: pip install -r requirements.txt")
        print(f"Import error: {exc}")
        return 1

    if config.list_cameras:
        found = _find_cameras(cv2)
        if not found:
            print("Available cameras: none")
        else:
            print("Available cameras:")
            for camera in found:
                print(f"  [{camera['index']}] {camera['name']}")
        return 0

    try:
        app = CameraSenderApp(config)
        return app.run()
    except RuntimeError as exc:
        print(f"Error: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
