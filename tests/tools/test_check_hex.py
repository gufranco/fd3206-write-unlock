import io
import tempfile
import unittest
import unittest.mock
from pathlib import Path

from tools import check_hex

DATA_RECORD = ":0400000012C018C052"
SECOND_RECORD = ":02000400FFCF2C"
EOF_RECORD = ":00000001FF"
VALID_HEX = f"{DATA_RECORD}\n{SECOND_RECORD}\n{EOF_RECORD}\n"


def write_hex(text: str) -> Path:
    path = Path(tempfile.mkdtemp()) / "firmware.hex"
    path.write_text(text)
    return path


class ParseTest(unittest.TestCase):
    def test_valid_image_reports_its_data_bytes(self) -> None:
        image = check_hex.parse(VALID_HEX)

        self.assertEqual(image.data_bytes, 6)
        self.assertEqual(image.end_address, 6)

    def test_bad_checksum_is_rejected(self) -> None:
        with self.assertRaisesRegex(check_hex.HexError, "line 1: checksum"):
            check_hex.parse(":0400000012C018C053\n:00000001FF\n")

    def test_length_mismatch_is_rejected(self) -> None:
        with self.assertRaisesRegex(check_hex.HexError, "line 1: length"):
            check_hex.parse(":0500000012C018C052\n:00000001FF\n")

    def test_missing_colon_is_rejected(self) -> None:
        with self.assertRaisesRegex(check_hex.HexError, "line 1: not a record"):
            check_hex.parse("0400000012C018C052\n:00000001FF\n")

    def test_non_hex_digits_are_rejected(self) -> None:
        with self.assertRaisesRegex(check_hex.HexError, "line 1: not a record"):
            check_hex.parse(":04000000ZZC018C052\n:00000001FF\n")

    def test_unexpected_record_type_is_rejected(self) -> None:
        with self.assertRaisesRegex(check_hex.HexError, "line 1: record type 04"):
            check_hex.parse(":020000040000FA\n:00000001FF\n")

    def test_missing_end_record_is_rejected(self) -> None:
        with self.assertRaisesRegex(check_hex.HexError, "no end-of-file record"):
            check_hex.parse(f"{DATA_RECORD}\n")

    def test_data_after_end_record_is_rejected(self) -> None:
        with self.assertRaisesRegex(check_hex.HexError, "line 2: after end-of-file"):
            check_hex.parse(f"{EOF_RECORD}\n{DATA_RECORD}\n")


class MainTest(unittest.TestCase):
    def run_main(self, *arguments: str) -> tuple[int, str, str]:
        with (
            unittest.mock.patch("sys.stdout", new_callable=io.StringIO) as out,
            unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as err,
        ):
            code = check_hex.main(["check_hex.py", *arguments])
        return code, out.getvalue(), err.getvalue()

    def test_image_within_flash_passes(self) -> None:
        path = write_hex(VALID_HEX)

        code, out, _ = self.run_main(str(path), "--max-bytes", "2048")

        self.assertEqual(code, 0)
        self.assertIn("6 bytes", out)

    def test_image_past_flash_end_fails(self) -> None:
        path = write_hex(VALID_HEX)

        code, _, err = self.run_main(str(path), "--max-bytes", "4")

        self.assertEqual(code, 1)
        self.assertIn("ends at byte 6, past the 4-byte flash", err)

    def test_malformed_image_fails(self) -> None:
        path = write_hex(":00000001FE\n")

        code, _, err = self.run_main(str(path), "--max-bytes", "2048")

        self.assertEqual(code, 1)
        self.assertIn("checksum", err)

    def test_missing_file_fails(self) -> None:
        missing = Path(tempfile.mkdtemp()) / "absent.hex"

        code, _, err = self.run_main(str(missing), "--max-bytes", "2048")

        self.assertEqual(code, 1)
        self.assertIn("cannot read", err)

    def test_wrong_arguments_print_usage(self) -> None:
        code, _, err = self.run_main("only-one")

        self.assertEqual(code, 2)
        self.assertIn("usage", err)


if __name__ == "__main__":
    unittest.main()
