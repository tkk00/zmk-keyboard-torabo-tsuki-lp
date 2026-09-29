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

    def test_single_tap_is_emitted_without_double_tap_window(self):
        source = driver_source()

        self.assertRegex(
            source,
            r"#define\s+IQS7211E_SINGLE_TAP_CODE\s+INPUT_BTN_3\b",
        )
        self.assertNotIn("IQS7211E_DOUBLE_TAP_CODE", source)
        self.assertNotIn("IQS7211E_TAP_SEQUENCE_MS", source)
        self.assertNotIn("tap_work", source)
        self.assertNotIn("single_tap_pending", source)
        self.assertRegex(source, r"if \(tap_allowed[^}]+iqs7211e_emit_click\(data, IQS7211E_SINGLE_TAP_CODE\)")

    def test_scroll_gesture_has_contact_markers_and_cannot_also_tap(self):
        source = driver_source()
        self.assertIn("IQS7211E_SCROLL_TOUCH_CODE INPUT_BTN_4", source)
        self.assertIn("data->scroll_touch_active", source)
        self.assertIn("input_report_key(dev, IQS7211E_SCROLL_TOUCH_CODE", source)
        self.assertIn("!data->gesture_scrolled", source)


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

    def test_scroll_is_one_fifth_speed_with_immediate_first_step_and_vertical_only(self):
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

        self.assertIn("&trackpad_responsive_scroll 1 5", listener)
        self.assertEqual(listener.count("&zip_scroll_scaler 1 3"), 2)
        self.assertIn("track-remainders;", listener)
        self.assertIn("CONFIG_IQS7211E_SCROLLER_HWHEEL_ZONE_MAX_PERMILLE=0", left_conf)
        self.assertIn("v-invert;", trackpad)

    def test_inertia_is_enabled_on_the_trackpad_peripheral(self):
        left_conf = (REPO_ROOT / "boards" / "shields" / "torabo_tsuki_lp" / "torabo_tsuki_lp_left.conf").read_text(encoding="utf-8")
        self.assertIn("CONFIG_IQS7211E_SCROLLER_INERTIA=y", left_conf)
        self.assertIn("CONFIG_IQS7211E_SCROLLER_INERTIA_DECAY_PERMILLE=940", left_conf)

    def test_layer_specific_tap_and_scroll_behaviors(self):
        listener = (
            REPO_ROOT
            / "snippets"
            / "input-split-listener"
            / "input-split-listener.overlay"
        ).read_text(encoding="utf-8")

        self.assertIn("&trackpad_transition_guard", listener)
        self.assertEqual(listener.count("&trackpad_transition_guard"), 3)
        self.assertIn("bindings = <&kp LA(LEFT_ARROW) &kp LC(N0) &kp C_MUTE>;", listener)
        self.assertIn("transition-guard-ms = <100>;", listener)
        self.assertIn("zoom-key-position = <56>;", listener)
        self.assertIn("volume-key-position = <57>;", listener)
        transition = (REPO_ROOT / "src" / "input_processor_trackpad_transition.c").read_text(encoding="utf-8")
        self.assertIn("event->code == IQS7211E_SCROLL_TOUCH_CODE", transition)
        self.assertIn("data->scroll_mode_latched", transition)
        self.assertIn("data->scroll_mode != current_mode(cfg)", transition)
        self.assertIn("layers = <1>;", listener)
        self.assertIn("bindings = <&kp LC(EQUAL) &kp LC(MINUS)>;", listener)
        self.assertIn("layers = <2>;", listener)
        self.assertIn("bindings = <&kp C_VOLUME_UP &kp C_VOLUME_DOWN>;", listener)

    def test_auto_mouse_clicks_use_jop_and_thumb_hold_is_shift(self):
        keymap = (REPO_ROOT / "config" / "keymap.keymap").read_text(encoding="utf-8")
        right = (REPO_ROOT / "boards" / "shields" / "torabo_tsuki_lp" / "torabo_tsuki_lp_right.overlay").read_text(encoding="utf-8")
        layer6 = keymap.split("layer_6 {")[1].split("};", 1)[0]
        rows = [re.findall(r"&(?:trans|mkp MB[123]|kp LC\([CV]\))", line)
                for line in layer6.splitlines() if line.startswith("&")]
        self.assertEqual(rows[1][8:12], ["&trans", "&mkp MB3", "&mkp MB2", "&trans"])
        self.assertEqual(rows[2][9:12], ["&mkp MB1", "&trans", "&trans"])
        self.assertEqual(layer6.count("&mkp MB"), 3)
        self.assertRegex(right, r"excluded-positions\s*=\s*<\s*21\s*// o\s*22\s*// p\s*33\s*// j")
        layer0 = keymap.split("layer_0 {")[1].split("};", 1)[0]
        self.assertIn("&mt LEFT_SHIFT LANGUAGE_1", layer0)

    def test_trackpad_layer_keys_activate_their_hold_layer_immediately(self):
        keymap = (REPO_ROOT / "config" / "keymap.keymap").read_text(encoding="utf-8")
        layer0 = keymap.split("layer_0 {")[1].split("};", 1)[0]
        self.assertIn("hold-while-undecided;", keymap)
        self.assertIn("&trackpad_lt 1 LANGUAGE_2", layer0)
        self.assertIn("&trackpad_lt 2 SPACE", layer0)


if __name__ == "__main__":
    unittest.main()
