# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import contextlib
import io
import runpy
import subprocess
import sys
import tempfile
import unittest
import unittest.mock
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent.parent / "tools"


def exit_code(tool: str, *arguments: str) -> str | int | None:
    with (
        unittest.mock.patch.object(sys, "argv", [tool, *arguments]),
        contextlib.redirect_stdout(io.StringIO()),
        contextlib.redirect_stderr(io.StringIO()),
    ):
        try:
            runpy.run_path(str(TOOLS / tool), run_name="__main__")
        except SystemExit as stopped:
            return stopped.code
    return None


def source_file(name: str, text: str) -> Path:
    root = Path(tempfile.mkdtemp())
    (root / "src").mkdir()
    path = root / "src" / name
    path.write_text(text)
    return path


class UsageErrorTest(unittest.TestCase):
    def test_check_hex_exits_with_its_usage_error(self) -> None:
        code = exit_code("check_hex.py")

        self.assertEqual(code, 2)

    def test_commit_message_exits_with_its_usage_error(self) -> None:
        code = exit_code("commit_message.py")

        self.assertEqual(code, 2)

    def test_doc_figures_exits_with_its_usage_error(self) -> None:
        code = exit_code("doc_figures.py")

        self.assertEqual(code, 2)

    def test_isr_check_exits_with_its_usage_error(self) -> None:
        code = exit_code("isr_check.py")

        self.assertEqual(code, 2)

    def test_list_instructions_exits_with_its_usage_error(self) -> None:
        code = exit_code("list_instructions.py")

        self.assertEqual(code, 2)

    def test_mutation_exits_with_its_usage_error(self) -> None:
        code = exit_code("mutation.py")

        self.assertEqual(code, 2)

    def test_release_notes_exits_with_its_usage_error(self) -> None:
        code = exit_code("release_notes.py")

        self.assertEqual(code, 2)

    def test_reproducible_exits_with_its_usage_error(self) -> None:
        code = exit_code("reproducible.py")

        self.assertEqual(code, 2)


class FailureTest(unittest.TestCase):
    def test_layer_check_exits_non_zero_on_a_crossed_layer(self) -> None:
        path = source_file("logic.c", '#include "port/port.h"\n')

        code = exit_code("layer_check.py", str(path.parent.parent))

        self.assertEqual(code, 1)

    def test_style_gate_exits_non_zero_on_a_banned_construct(self) -> None:
        path = source_file("logic.c", "goto done;\n")

        code = exit_code("style_gate.py", str(path))

        self.assertEqual(code, 1)

    def test_docker_make_exits_with_the_code_of_the_container(self) -> None:
        failed: subprocess.CompletedProcess[bytes] = subprocess.CompletedProcess(
            args=[], returncode=7
        )
        with unittest.mock.patch.object(subprocess, "run", return_value=failed):
            code = exit_code("docker_make.py", "test")

        self.assertEqual(code, 7)


if __name__ == "__main__":
    unittest.main()
