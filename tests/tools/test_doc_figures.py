import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools import doc_figures

SIZE_OUTPUT = """firmware.elf  :
section           size      addr
.data                2   8388704
.text              318         0
.bss                 1   8388706
.comment            18         0
Total              339
"""

OBJDUMP_OUTPUT = """
00000000 <__vectors>:
   0:\t12 c0       \trjmp\t.+36
0000005a <__vector_1>:
  5a:\t0f 93       \tpush\tr16
  5c:\t0e b3       \tin\tr16, 0x1e
  5e:\t00 00       \tnop
0000005f <.Ldone>:
  60:\t18 95       \treti
00000062 <main>:
  62:\tff cf       \trjmp\t.-2
"""

FIGURES = doc_figures.Figures(flash_bytes=320, handler_instructions=3, source_lines=42)

README = f"""# Title

{doc_figures.BEGIN}
stale
{doc_figures.END}

Tail.
"""


def make_root() -> Path:
    root = Path(tempfile.mkdtemp())
    (root / "src").mkdir()
    (root / "include" / "fdswu").mkdir(parents=True)
    (root / "src" / "a.c").write_text("one\ntwo\n\nthree\n")
    (root / "src" / "b.S").write_text("x\n")
    (root / "include" / "fdswu" / "a.h").write_text("y\nz\n")
    (root / "README.md").write_text(README)
    return root


class ParseTest(unittest.TestCase):
    def test_flash_bytes_sum_text_and_data(self) -> None:
        flash = doc_figures.flash_bytes(SIZE_OUTPUT)

        self.assertEqual(flash, 320)

    def test_handler_instructions_skip_padding(self) -> None:
        count = doc_figures.handler_instructions(OBJDUMP_OUTPUT, "__vector_1")

        self.assertEqual(count, 3)

    def test_source_lines_count_non_blank_lines(self) -> None:
        root = make_root()

        lines = doc_figures.source_lines(root)

        self.assertEqual(lines, 6)


class RunTest(unittest.TestCase):
    def test_run_returns_standard_output(self) -> None:
        output = doc_figures.run([sys.executable, "-c", "print('measured')"])

        self.assertEqual(output, "measured\n")


class BlockTest(unittest.TestCase):
    def test_block_is_replaced_between_markers(self) -> None:
        updated = doc_figures.replace_block(README, doc_figures.render(FIGURES))

        self.assertIn("| Flash used | 320 bytes |", updated)
        self.assertNotIn("stale", updated)
        self.assertTrue(updated.endswith("Tail.\n"))

    def test_missing_markers_raise(self) -> None:
        with self.assertRaises(doc_figures.MarkerError):
            doc_figures.replace_block("# no markers\n", doc_figures.render(FIGURES))


class MainTest(unittest.TestCase):
    def arguments(self, root: Path, *extra: str) -> list[str]:
        return [
            "doc_figures.py",
            str(root),
            "avr-size",
            "avr-objdump",
            "firmware.elf",
            *extra,
        ]

    def run_main(self, root: Path, *extra: str) -> tuple[int, str]:
        outputs = {"avr-size": SIZE_OUTPUT, "avr-objdump": OBJDUMP_OUTPUT}
        with (
            mock.patch.object(
                doc_figures, "run", side_effect=lambda command: outputs[command[0]]
            ),
            mock.patch("sys.stderr", new_callable=io.StringIO) as errors,
        ):
            code = doc_figures.main(self.arguments(root, *extra))
        return code, errors.getvalue()

    def test_stale_block_fails_the_check(self) -> None:
        root = make_root()

        code, errors = self.run_main(root)

        self.assertEqual(code, 1)
        self.assertIn("make figures", errors)

    def test_update_then_check_passes(self) -> None:
        root = make_root()

        results = (self.run_main(root, "--update")[0], self.run_main(root)[0])

        self.assertEqual(results, (0, 0))

    def test_missing_markers_fail(self) -> None:
        root = make_root()
        (root / "README.md").write_text("# none\n")

        code, errors = self.run_main(root)

        self.assertEqual(code, 1)
        self.assertIn("markers", errors)

    def test_wrong_arguments_print_usage(self) -> None:
        with mock.patch("sys.stderr", new_callable=io.StringIO) as errors:
            code = doc_figures.main(["doc_figures.py"])

        self.assertEqual(code, 2)
        self.assertIn("usage", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
