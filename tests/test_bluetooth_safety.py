import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
KEYMAP = REPO_ROOT / "config" / "keymap.keymap"

DANGEROUS = re.compile(r"&bt\b|&sys_reset\b|&bootloader\b")
SYSTEM_LAYER = 8


def keymap_source() -> str:
    return KEYMAP.read_text(encoding="utf-8")


def layer_body(keymap: str, index: int) -> str:
    return keymap.split(f"layer_{index} {{")[1].split("};", 1)[0]


def layer_rows(keymap: str, index: int) -> list[list[str]]:
    rows = []
    for line in layer_body(keymap, index).splitlines():
        if line.startswith("&"):
            rows.append(["&" + b.strip() for b in line.split("&")[1:]])
    return rows


class BluetoothSafetyTests(unittest.TestCase):
    def test_only_the_system_layer_holds_bluetooth_and_reset_keys(self):
        keymap = keymap_source()
        keymap_node = keymap.split('compatible = "zmk,keymap";')[1]
        layer_ids = [int(i) for i in re.findall(r"layer_(\d+) \{", keymap_node)]

        self.assertEqual(layer_ids, list(range(SYSTEM_LAYER + 1)))
        for index in layer_ids:
            if index == SYSTEM_LAYER:
                continue
            self.assertIsNone(
                DANGEROUS.search(layer_body(keymap, index)),
                f"layer_{index} must not hold Bluetooth or reset keys",
            )

    def test_system_layer_is_reachable_only_by_holding_y_and_p(self):
        keymap = keymap_source()
        combos = keymap.split('compatible = "zmk,combos";')[1].split("macros {")[0]
        system = combos.split("system_layer {")[1].split("};", 1)[0]
        layer0 = layer_rows(keymap, 0)

        self.assertIn(f"bindings = <&mo {SYSTEM_LAYER}>;", system)
        self.assertIn("key-positions = <18 22>;", system)
        self.assertEqual(layer0[1][18 - 12], "&kp Y")
        self.assertEqual(layer0[1][22 - 12], "&lt 5 P")
        # Typing "py" quickly (pya/pyu/pyo) must not enter the layer.
        self.assertIn("require-prior-idle-ms = <300>;", system)
        # No hold-tap, toggle or other key may reach the system layer.
        references = re.findall(
            rf"&(?:mo|lt|to|tog|sl|trackpad_lt|jp_minus_lt)\s+{SYSTEM_LAYER}\b", keymap
        )
        self.assertEqual(references, [f"&mo {SYSTEM_LAYER}"])

    def test_system_layer_layout(self):
        rows = layer_rows(keymap_source(), SYSTEM_LAYER)

        self.assertEqual([len(r) for r in rows], [12, 12, 14, 14, 14])
        # Same left-hand positions the old layer 5 used: QWERT, ASDFG, Z X C.
        self.assertEqual(rows[1][1:6], [f"&bt BT_SEL {i}" for i in range(5)])
        self.assertEqual(rows[2][1:6], [f"&bt BT_DISC {i}" for i in range(5)])
        self.assertEqual(rows[3][1:4], ["&sys_reset", "&bootloader", "&bt BT_CLR"])
        # Nothing else on the layer, in particular nothing on the right hand.
        bindings = [b for row in rows for b in row]
        self.assertEqual(sum(1 for b in bindings if b != "&none"), 13)

    def test_scroll_layer_keeps_its_navigation_and_volume_keys(self):
        layer5 = layer_body(keymap_source(), 5)

        self.assertRegex(layer5, r"&kp LA\(LEFT_ARROW\)\s+&trans\s+&kp LA\(RIGHT_ARROW\)")
        for binding in ("&kp C_MUTE", "&kp C_VOLUME_UP", "&kp C_VOLUME_DOWN", "&studio_unlock"):
            self.assertIn(binding, layer5)


if __name__ == "__main__":
    unittest.main()
