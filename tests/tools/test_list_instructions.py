# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import io
import subprocess
import unittest
import unittest.mock

from tools import list_instructions

NM_OUTPUT = """
build/main.o:
00000000 T main
00000000 t apply_write_lines
00000000 D some_data

build/write_plan.o:
00000000 T fdswu_write_plan
"""

OBJDUMP_OUTPUT = """
00000000 <__vectors>:
   0:\t14 c0       \trjmp\t.+40
00000048 <__vector_1>:
  48:\t0f 93       \tpush\tr16
00000049 <.Ldone>:
  49:\t18 95       \treti
0000004a <main>:
  4a:\t00 00       \tnop
  4c:\t8f e9       \tldi\tr24, 0x9F
  4e:\t00 00       \t.word\t0x0000
00000050 <apply_write_lines.lto_priv.0>:
  50:\t08 95       \tret
"""


def completed(stdout: str) -> subprocess.CompletedProcess[str]:
    return subprocess.CompletedProcess(args=[], returncode=0, stdout=stdout)


class FunctionNamesTest(unittest.TestCase):
    def test_keeps_text_symbols_and_drops_data(self) -> None:
        names = list_instructions.function_names(NM_OUTPUT)

        self.assertEqual(names, {"main", "apply_write_lines", "fdswu_write_plan"})


class BaseNameTest(unittest.TestCase):
    def test_strips_link_time_suffixes(self) -> None:
        name = list_instructions.base_name("apply_write_lines.lto_priv.0")

        self.assertEqual(name, "apply_write_lines")


class InstructionAddressesTest(unittest.TestCase):
    def test_lists_executable_instructions_of_owned_functions(self) -> None:
        owned = {"main", "apply_write_lines", "__vector_1"}

        lines = list_instructions.instruction_addresses(OBJDUMP_OUTPUT, owned)

        self.assertEqual(
            lines,
            [
                (0x48, "__vector_1"),
                (0x49, "__vector_1"),
                (0x4C, "main"),
                (0x50, "apply_write_lines.lto_priv.0"),
            ],
        )


class MainTest(unittest.TestCase):
    def test_usage_error_without_objects(self) -> None:
        with unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as errors:
            exit_code = list_instructions.main(
                ["list_instructions.py", "objdump", "nm", "fw.elf"]
            )

        self.assertEqual(exit_code, 2)
        self.assertIn("usage", errors.getvalue())

    def test_fails_when_no_function_is_defined(self) -> None:
        with (
            unittest.mock.patch.object(
                list_instructions.subprocess, "run", return_value=completed("")
            ),
            unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as errors,
        ):
            exit_code = list_instructions.main(
                ["list_instructions.py", "objdump", "nm", "fw.elf", "a.o"]
            )

        self.assertEqual(exit_code, 1)
        self.assertIn("a.o", errors.getvalue())

    def test_prints_one_line_per_instruction(self) -> None:
        outputs = [completed(NM_OUTPUT), completed(OBJDUMP_OUTPUT)]
        with (
            unittest.mock.patch.object(
                list_instructions.subprocess, "run", side_effect=outputs
            ),
            unittest.mock.patch("builtins.print") as printed,
        ):
            exit_code = list_instructions.main(
                ["list_instructions.py", "objdump", "nm", "fw.elf", "a.o"]
            )

        self.assertEqual(exit_code, 0)
        printed.assert_called_once_with("76 main\n80 apply_write_lines.lto_priv.0")


if __name__ == "__main__":
    unittest.main()
