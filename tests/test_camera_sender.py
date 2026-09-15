import unittest
from pathlib import Path

import cv2
import numpy as np

import camera_sender


class CameraSenderTests(unittest.TestCase):
    def test_jsonc_config_loads(self):
        config = camera_sender._load_camera_config(
            Path(__file__).parents[1] / "camera_config.json",
            required=True,
        )
        self.assertIn("input_mode", config)
        self.assertIn("camera_index", config)

    def test_packet_format_is_native_compatible(self):
        output = camera_sender.OutputState(
            left_x=0.125,
            left_y=0.25,
            left_pressed=1,
            right_x=0.75,
            right_y=0.875,
            right_pressed=0,
        )
        self.assertEqual(
            camera_sender._format_packet(output, 1),
            "0.1250,0.2500,1,0.7500,0.8750,0,1",
        )

    def test_circle_calibration_clamps_to_disk(self):
        calibrated = camera_sender._calibrate_position(
            0.0,
            0.0,
            0.2,
            0.8,
            0.5,
            640,
            480,
        )
        distance = np.hypot(calibrated[0] - 0.5, calibrated[1] - 0.5)
        self.assertLessEqual(distance, 0.5 + 1e-9)
        self.assertTrue(all(0.0 <= value <= 1.0 for value in calibrated))

    def test_capture_format_and_orientation_helpers(self):
        self.assertEqual(camera_sender._normalize_capture_format("yuy2"), "YUY2")
        self.assertEqual(camera_sender._normalize_capture_format("auto"), "auto")
        self.assertEqual(
            camera_sender._fourcc_to_string(cv2.VideoWriter_fourcc(*"MJPG")),
            "MJPG",
        )
        frame = np.array([[[1, 2, 3], [4, 5, 6]]], dtype=np.uint8)
        self.assertTrue(np.array_equal(camera_sender._prepare_frame(cv2, frame, False), frame))
        flipped = camera_sender._prepare_frame(cv2, frame, True)
        self.assertTrue(np.array_equal(flipped[0, 0], frame[0, 1]))

    def test_large_led_jump_requires_confirmation(self):
        tracker = camera_sender.LedTracker()
        self.assertTrue(tracker.accept("Left", (0.2, 0.5), 1.0, 2, 0.12))
        self.assertFalse(tracker.accept("Left", (0.8, 0.5), 1.1, 2, 0.12))
        self.assertTrue(tracker.accept("Left", (0.8, 0.5), 1.2, 2, 0.12))

    def test_hand_calibration_applies_on_release(self):
        observations = [
            camera_sender.HandObservation(
                "Left", 0.3, 0.5, False, True, tuple([(0.3, 0.5)] * 21), -0.1, 0.2
            ),
            camera_sender.HandObservation(
                "Right", 0.7, 0.5, False, True, tuple([(0.7, 0.5)] * 21), -0.1, 0.2
            ),
        ]
        controller = camera_sender.HandCalibrationController()
        controller.request()
        self.assertIsNone(controller.update(observations, 0.025))
        self.assertIsNotNone(controller.update([], 0.025))

    def test_synthetic_led_colors_detect_independently(self):
        frame = np.zeros((480, 640, 3), dtype=np.uint8)
        cv2.rectangle(frame, (120, 220), (220, 260), (255, 0, 0), -1)
        cv2.rectangle(frame, (420, 220), (520, 260), (0, 0, 255), -1)
        left, right = camera_sender._detect_led_positions(cv2, frame)
        self.assertIsNotNone(left)
        self.assertIsNotNone(right)
        self.assertLess(left[0], right[0])


if __name__ == "__main__":
    unittest.main()
