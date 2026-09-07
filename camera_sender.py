import argparse
import socket
import time
from pathlib import Path
from urllib.request import urlretrieve


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Lightweight hand tracker sender for ControllerInput.exe"
    )
    parser.add_argument("--host", default="127.0.0.1", help="UDP destination host")
    parser.add_argument("--port", type=int, default=8765, help="UDP destination port")
    parser.add_argument("--camera-index", type=int, default=0, help="Camera index")
    parser.add_argument("--max-hands", type=int, default=2, help="Max hands to track")
    parser.add_argument("--min-detect", type=float, default=0.7, help="Min detection confidence")
    parser.add_argument("--min-track", type=float, default=0.5, help="Min tracking confidence")
    parser.add_argument("--preview", action="store_true", help="Show camera preview window")
    parser.add_argument("--fps", type=float, default=30.0, help="Target send FPS")
    parser.add_argument("--log", action="store_true", help="Print outgoing packet snapshots")
    parser.add_argument("--log-interval", type=float, default=0.5, help="Seconds between packet logs")
    parser.add_argument(
        "--model-path",
        default="hand_landmarker.task",
        help="Path to MediaPipe Hand Landmarker .task model (used when mp.solutions is unavailable)",
    )
    parser.add_argument(
        "--auto-download-model",
        action="store_true",
        help="Auto-download hand_landmarker.task if missing (requires internet)",
    )
    return parser.parse_args()


def _make_hand_detector(mp, args):
    """Return a callable that maps rgb frame -> [(label, x, y, pressed), ...]."""

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
                    pressed = idx_tip.y < (mid_tip.y - 0.04)
                    out.append((hand_label, float(idx_tip.x), float(idx_tip.y), bool(pressed)))
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
        running_mode=mp_vision_tasks.RunningMode.IMAGE,
    )
    landmarker = mp_vision_tasks.HandLandmarker.create_from_options(options)

    def detect(rgb_frame):
        out = []
        image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb_frame)
        results = landmarker.detect(image)
        for i, hand_landmarks in enumerate(results.hand_landmarks):
            label = "Unknown"
            if i < len(results.handedness) and len(results.handedness[i]) > 0:
                label = results.handedness[i][0].category_name

            idx_tip = hand_landmarks[8]  # INDEX_FINGER_TIP
            mid_tip = hand_landmarks[12]  # MIDDLE_FINGER_TIP
            pressed = idx_tip.y < (mid_tip.y - 0.04)
            out.append((label, float(idx_tip.x), float(idx_tip.y), bool(pressed)))
        return out

    return detect


def main() -> int:
    args = parse_args()

    try:
        import cv2
        import mediapipe as mp
    except Exception as exc:
        print("Missing dependencies. Install with: pip install -r requirements.txt")
        print(f"Import error: {exc}")
        return 1

    udp_addr = (args.host, args.port)
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.setblocking(False)

    cap = cv2.VideoCapture(args.camera_index, cv2.CAP_DSHOW)
    if not cap.isOpened():
        print(f"Error: cannot open camera index {args.camera_index}")
        return 1

    try:
        detect_hands = _make_hand_detector(mp, args)
    except Exception as exc:
        print("Failed to initialize hand detector backend.")
        print(f"Reason: {exc}")
        return 1

    min_frame_dt = 1.0 / max(1.0, args.fps)
    last_send = 0.0
    last_log = 0.0
    sent_count = 0

    print(f"Sending hand data to {args.host}:{args.port}")
    print("Press 'q' in preview window to quit." if args.preview else "Press Ctrl+C to quit.")

    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                time.sleep(0.01)
                continue

            frame = cv2.flip(frame, 1)
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            # Per-frame defaults mirror C++ expectations.
            left_x = 0.0
            left_y = 0.0
            left_pressed = 0
            right_x = 0.0
            right_y = 0.0
            right_pressed = 0

            for hand_label, norm_x, norm_y, is_shooting in detect_hands(rgb):
                if hand_label == "Left":
                    left_x = max(0.0, min(1.0, norm_x))
                    left_y = max(0.0, min(1.0, norm_y))
                    left_pressed = 1 if is_shooting else 0
                elif hand_label == "Right":
                    right_x = max(0.0, min(1.0, norm_x))
                    right_y = max(0.0, min(1.0, norm_y))
                    right_pressed = 1 if is_shooting else 0

            now = time.perf_counter()
            if now - last_send >= min_frame_dt:
                msg = f"{left_x:.4f},{left_y:.4f},{left_pressed},{right_x:.4f},{right_y:.4f},{right_pressed}"
                try:
                    udp_sock.sendto(msg.encode("ascii"), udp_addr)
                    sent_count += 1
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
                if left_x > 0.0 or left_y > 0.0:
                    cv2.circle(frame, (int(left_x * w), int(left_y * h)), 10, (0, 255, 0), -1)
                    cv2.putText(frame, f"L:{left_pressed}", (int(left_x * w) + 8, int(left_y * h) - 8), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
                if right_x > 0.0 or right_y > 0.0:
                    cv2.circle(frame, (int(right_x * w), int(right_y * h)), 10, (255, 0, 0), -1)
                    cv2.putText(frame, f"R:{right_pressed}", (int(right_x * w) + 8, int(right_y * h) - 8), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 1)

                cv2.putText(
                    frame,
                    f"UDP {args.host}:{args.port}",
                    (10, 24),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.6,
                    (255, 255, 255),
                    1,
                )
                cv2.putText(
                    frame,
                    f"Sent: {sent_count}",
                    (10, 48),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.6,
                    (255, 255, 255),
                    1,
                )
                cv2.imshow("Camera Sender", frame)
                if (cv2.waitKey(1) & 0xFF) == ord("q"):
                    break

    except KeyboardInterrupt:
        pass
    finally:
        cap.release()
        cv2.destroyAllWindows()
        udp_sock.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
