"""Revision-3 product naming contract; no device access."""
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


class MacropadProductName(unittest.TestCase):
    def test_rev3_product_name_and_hardware_identity(self):
        board = ROOT / "keyboards/ergohaven/macropad/rev3"
        metadata = json.loads((board / "keyboard.json").read_text())
        self.assertEqual(metadata["keyboard_name"], "Macropad")
        self.assertEqual(metadata["usb"]["vid"], "0xE126")
        self.assertEqual(metadata["usb"]["pid"], "0x0042")
        self.assertEqual(metadata["manufacturer"], "Ergohaven")
        name = re.search(r'^#define EH_SHORT_PRODUCT_NAME "([^"]+)"$', (board / "config.h").read_text(), re.M)
        assert name is not None
        self.assertEqual(name.group(1), "Macropad")


if __name__ == "__main__":
    unittest.main()
