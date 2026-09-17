import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
VENDORED_DRIVER = REPO_ROOT / "drivers" / "iqs7211e" / "src" / "iqs7211e.c"
WORKSPACE_DRIVER = (
    REPO_ROOT.parent
    / "zmk-workspace"
    / "zmk-driver-iqs7211e"
    / "src"
    / "iqs7211e.c"
)


def driver_source() -> str:
    path = VENDORED_DRIVER if VENDORED_DRIVER.exists() else WORKSPACE_DRIVER
    return path.read_text(encoding="utf-8")


class TrackpadNavigationTests(unittest.TestCase):
    def test_single_finger_taps_cannot_hold_the_left_button(self):
        source = driver_source()
        start = source.index("// Single finger tap handling")
        end = source.index("#if defined(CONFIG_IQS7211E_SCROLLER_INERTIA)", start)
        tap_path = source[start:end]

        self.assertNotIn("double_tap_hold", tap_path)
        self.assertNotIn("is_clicking", tap_path)
        self.assertNotIn("INPUT_BTN_0", tap_path)

    def test_taps_emit_distinct_back_and_forward_markers(self):
        source = driver_source()

        self.assertRegex(
            source,
            r"#define\s+IQS7211E_SINGLE_TAP_CODE\s+INPUT_BTN_3\b",
        )
        self.assertRegex(
            source,
            r"#define\s+IQS7211E_DOUBLE_TAP_CODE\s+INPUT_BTN_4\b",
        )

    def test_double_tap_cancels_the_pending_single_tap(self):
        source = driver_source()
        double_tap = re.search(
            r"if\s*\([^\n]*tap_interval[^\n]*\)\s*\{(?P<body>.*?)\n\s*\}\s*else\s*\{",
            source,
            flags=re.DOTALL,
        )

        self.assertIsNotNone(double_tap)
        body = double_tap.group("body")
        self.assertIn("k_work_cancel_delayable(&data->tap_work)", body)
        self.assertIn("IQS7211E_DOUBLE_TAP_CODE", body)


class TrackpadConfigurationTests(unittest.TestCase):
    def test_central_input_thread_has_room_for_navigation_behavior(self):
        central_conf = (
            REPO_ROOT
            / "boards"
            / "shields"
            / "torabo_tsuki_lp"
            / "torabo_tsuki_lp_right.conf"
        ).read_text(encoding="utf-8")

        self.assertIn("CONFIG_INPUT_THREAD_STACK_SIZE=4096", central_conf)

    def test_vendored_driver_devicetree_binding_is_registered(self):
        module = (REPO_ROOT / "zephyr" / "module.yml").read_text(encoding="utf-8")

        self.assertIn("dts_root: drivers/iqs7211e", module)

    def test_scroll_is_one_third_speed_and_vertical_only(self):
        listener = (
            REPO_ROOT
            / "snippets"
            / "input-split-listener"
            / "input-split-listener.overlay"
        ).read_text(encoding="utf-8")
        left_conf = (
            REPO_ROOT
            / "boards"
            / "shields"
            / "torabo_tsuki_lp"
            / "torabo_tsuki_lp_left.conf"
        ).read_text(encoding="utf-8")
        trackpad = (
            REPO_ROOT
            / "snippets"
            / "input-trackpad-mini"
            / "input-trackpad-mini.overlay"
        ).read_text(encoding="utf-8")

        self.assertIn("&zip_scroll_scaler 1 3", listener)
        self.assertIn("CONFIG_IQS7211E_SCROLLER_HWHEEL_ZONE_MAX_PERMILLE=0", left_conf)
        self.assertIn("v-invert;", trackpad)

    def test_tap_markers_invoke_alt_arrow_behaviors(self):
        listener = (
            REPO_ROOT
            / "snippets"
            / "input-split-listener"
            / "input-split-listener.overlay"
        ).read_text(encoding="utf-8")

        self.assertIn("codes = <INPUT_BTN_3 INPUT_BTN_4>;", listener)
        self.assertIn("bindings = <&kp LA(LEFT_ARROW) &kp LA(RIGHT_ARROW)>;", listener)


if __name__ == "__main__":
    unittest.main()
