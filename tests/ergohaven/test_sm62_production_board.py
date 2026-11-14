"""Production PCB GPIO contract; no hardware I/O or generated QMK model."""
import hashlib
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "keyboards/ergohaven/sm62"


class ProductionBoardTests(unittest.TestCase):
    def setUp(self):
        self.keyboard = json.loads((BOARD / "keyboard.json").read_text())

    def test_real_board_matrix_uses_schematic_gpio_order(self):
        self.assertEqual(
            self.keyboard["matrix_pins"]["cols"],
            ["GP21", "GP22", "GP23", "GP24", "GP25", "GP26", "GP27"],
        )
        self.assertEqual(
            self.keyboard["matrix_pins"]["rows"],
            ["GP28", "GP19", "GP18", "GP17", "GP16"],
        )
        pins = sum(self.keyboard["matrix_pins"].values(), [])
        self.assertEqual(len(pins), len(set(pins)))
        self.assertFalse(set(pins).intersection({f"GP{i}" for i in range(8)} | {"GP29"}))
        self.assertEqual(self.keyboard["diode_direction"], "COL2ROW")

    def test_real_board_keeps_all_62_logical_key_coordinates(self):
        self.assertEqual(len(self.keyboard["layouts"]["LAYOUT"]["layout"]), 62)
        digest = hashlib.sha256(json.dumps(
            self.keyboard["layouts"], sort_keys=True, separators=(",", ":")
        ).encode()).hexdigest()
        self.assertEqual(digest, "ee911de513c315d74c5e5e24ee275b28e542bff4929bb78af5e006e41d4bba77")
        self.assertEqual(self.keyboard["dynamic_keymap"]["layer_count"], 16)

    def test_physical_flash_is_explicitly_four_mib(self):
        rules = (BOARD / "rules.mk").read_text()
        self.assertIn("-DPICO_FLASH_SIZE_BYTES=4194304", rules)

    def test_common_image_retains_uart_display_and_handedness(self):
        config = (BOARD / "config.h").read_text()
        for define in [
            "SERIAL_USART_TX_PIN GP0", "SERIAL_USART_RX_PIN GP1", "SPLIT_HAND_PIN GP29",
            "SPI_SCK_PIN GP2", "SPI_MOSI_PIN GP3", "LCD_RST_PIN GP4",
            "LCD_DC_PIN GP5", "LCD_CS_PIN GP6",
        ]:
            self.assertIn("#define " + define, config)
        self.assertEqual(self.keyboard["backlight"]["pin"], "GP7")
        for forbidden in ["#define MASTER_LEFT", "#define MASTER_RIGHT", "#define SM62_FIRMWARE_SIDE"]:
            self.assertNotIn(forbidden, config)


if __name__ == "__main__":
    unittest.main(verbosity=2)
