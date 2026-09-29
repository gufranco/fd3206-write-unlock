# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import io
import tempfile
import unittest
import unittest.mock
from pathlib import Path

from tools import reproducible


def make_repository() -> Path:
    root = Path(tempfile.mkdtemp())
    (root / "src").mkdir()
    (root / "src" / "main.c").write_text("int main(void);\n")
    return root


def writing_runner(builds: list[bytes]) -> reproducible.Runner:
    remaining = list(builds)

    def runner(_command: list[str], cwd: Path) -> int:
        contents = remaining.pop(0)
        for path in reproducible.HEX_PATHS:
            hex_file = cwd / path
            hex_file.parent.mkdir(parents=True, exist_ok=True)
            hex_file.write_bytes(contents + path.encode())
        return 0

    return runner


def failing_runner(_command: list[str], _cwd: Path) -> int:
    return 2


class MainTest(unittest.TestCase):
    def run_main(self, runner: reproducible.Runner) -> tuple[int, str, str]:
        root = make_repository()
        with (
            unittest.mock.patch("sys.stdout", new_callable=io.StringIO) as out,
            unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as err,
        ):
            code = reproducible.main(["reproducible.py", str(root)], runner)
        return code, out.getvalue(), err.getvalue()

    def test_identical_builds_pass_and_print_the_hash(self) -> None:
        code, out, _ = self.run_main(writing_runner([b":00000001FF\n"] * 2))

        self.assertEqual(code, 0)
        self.assertEqual(out.count("reproducible: "), len(reproducible.HEX_PATHS))

    def test_different_builds_fail(self) -> None:
        code, _, err = self.run_main(writing_runner([b"a", b"b"]))

        self.assertEqual(code, 1)
        self.assertEqual(err.count(" differs: "), len(reproducible.HEX_PATHS))

    def test_failed_build_fails(self) -> None:
        code, _, err = self.run_main(failing_runner)

        self.assertEqual(code, 1)
        self.assertIn("build failed", err)

    def test_wrong_arguments_print_usage(self) -> None:
        with unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as err:
            code = reproducible.main(["reproducible.py"])

        self.assertEqual(code, 2)
        self.assertIn("usage", err.getvalue())


if __name__ == "__main__":
    unittest.main()
