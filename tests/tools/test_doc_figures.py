import io
import sys
import tempfile
import unittest
import unittest.mock
from pathlib import Path

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
    for name in doc_figures.READMES:
        (root / name).write_text(README)
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
        updated = doc_figures.replace_block(
            README, doc_figures.render(FIGURES, doc_figures.ENGLISH)
        )

        self.assertIn("| Flash used | 320 bytes |", updated)
        self.assertNotIn("stale", updated)
        self.assertTrue(updated.endswith("Tail.\n"))

    def test_missing_markers_raise(self) -> None:
        with self.assertRaises(doc_figures.MarkerError):
            doc_figures.replace_block(
                "# no markers\n", doc_figures.render(FIGURES, doc_figures.ENGLISH)
            )

    def test_japanese_labels_carry_the_same_values(self) -> None:
        block = doc_figures.render(FIGURES, doc_figures.JAPANESE)

        self.assertIn("| フラッシュ使用量 | 320 バイト |", block)
        self.assertIn("| エッジ割り込みハンドラ | 3 命令 |", block)
        self.assertIn("| ファームウェアのソース | 空行を除き 42 行 |", block)

    def test_chinese_labels_carry_the_same_values(self) -> None:
        simplified = doc_figures.render(FIGURES, doc_figures.SIMPLIFIED_CHINESE)
        hong_kong = doc_figures.render(FIGURES, doc_figures.HONG_KONG_CHINESE)

        self.assertIn("| 闪存占用 | 320 字节 |", simplified)
        self.assertIn("| 快閃記憶體用量 | 320 位元組 |", hong_kong)

    def test_every_language_has_a_readme(self) -> None:
        names = sorted(doc_figures.READMES)

        self.assertEqual(
            names, ["README.ja.md", "README.md", "README.zh-CN.md", "README.zh-HK.md"]
        )


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
            unittest.mock.patch.object(
                doc_figures, "run", side_effect=lambda command: outputs[command[0]]
            ),
            unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as errors,
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
        self.assertIn("320 バイト", (root / "README.ja.md").read_text())
        self.assertIn("320 位元組", (root / "README.zh-HK.md").read_text())

    def test_stale_japanese_readme_alone_fails_the_check(self) -> None:
        root = make_root()
        self.run_main(root, "--update")
        (root / "README.ja.md").write_text(README)

        code, errors = self.run_main(root)

        self.assertEqual(code, 1)
        self.assertIn("README.ja.md", errors)
        self.assertNotIn("README.md:", errors)

    def test_missing_japanese_readme_fails(self) -> None:
        root = make_root()
        (root / "README.ja.md").unlink()

        code, errors = self.run_main(root, "--update")

        self.assertEqual(code, 1)
        self.assertIn("README.ja.md: missing", errors)

    def test_missing_markers_fail(self) -> None:
        root = make_root()
        (root / "README.md").write_text("# none\n")

        code, errors = self.run_main(root)

        self.assertEqual(code, 1)
        self.assertIn("markers", errors)

    def test_wrong_arguments_print_usage(self) -> None:
        with unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as errors:
            code = doc_figures.main(["doc_figures.py"])

        self.assertEqual(code, 2)
        self.assertIn("usage", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
