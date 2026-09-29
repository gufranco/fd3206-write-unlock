# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import io
import sys
import unittest
import unittest.mock

from tools import isr_check

HANDLER = """
0000005a <__vector_1>:
  5a:\t0f 93       \tpush\tr16
  5c:\t03 b3       \tin\tr16, 0x13\t; 19
  5e:\t07 bb       \tout\t0x17, r16\t; 23
  60:\t1f 93       \tpush\tr17
  62:\t14 b3       \tin\tr17, 0x14\t; 20
  64:\t13 bb       \tout\t0x13, r17\t; 19
  66:\t04 bb       \tout\t0x14, r16\t; 20
  68:\ta8 9b       \tsbis\t0x15, 0\t; 21
  6a:\t02 c0       \trjmp\t.+4      \t; 0x70 <.Lset_toggle>
  6c:\ta8 98       \tcbi\t0x15, 0\t; 21
  6e:\t01 c0       \trjmp\t.+2      \t; 0x72 <.Ldone>

00000070 <.Lset_toggle>:
  70:\ta8 9a       \tsbi\t0x15, 0\t; 21

00000072 <.Ldone>:
  72:\t1f 91       \tpop\tr17
  74:\t0f 91       \tpop\tr16
  76:\t18 95       \treti
"""

COMMIT = """
000000d0 <fdswu_port_commit_heads>:
  d0:\tf8 94       \tcli
  d2:\ta8 99       \tsbic\t0x15, 0\t; 21
  d4:\t05 c0       \trjmp\t.+10     \t; 0xe0 <.Lcommit_set>
  d6:\t87 bb       \tout\t0x17, r24\t; 23
  d8:\t63 bb       \tout\t0x13, r22\t; 19
  da:\t44 bb       \tout\t0x14, r20\t; 20
  dc:\t78 94       \tsei
  de:\t08 95       \tret

000000e0 <.Lcommit_set>:
  e0:\t27 bb       \tout\t0x17, r18\t; 23
  e2:\t03 bb       \tout\t0x13, r16\t; 19
  e4:\te4 ba       \tout\t0x14, r14\t; 20
  e6:\t78 94       \tsei
  e8:\t08 95       \tret
"""


class HandlerTest(unittest.TestCase):
    def test_worst_path_counts_the_longer_branch(self) -> None:
        cycles = isr_check.handler_cycles(isr_check.instructions(HANDLER, "__vector_1"))

        self.assertEqual(cycles, 23)

    def test_flag_changing_instruction_is_reported(self) -> None:
        mutated = HANDLER.replace("in\tr17, 0x14", "andi\tr17, 0x14")

        problems = isr_check.handler_problems(
            isr_check.instructions(mutated, "__vector_1")
        )

        self.assertIn("andi is not allowed in the edge handler", problems)

    def test_register_outside_the_saved_pair_is_reported(self) -> None:
        mutated = HANDLER.replace("in\tr16, 0x13", "in\tr18, 0x13")

        problems = isr_check.handler_problems(
            isr_check.instructions(mutated, "__vector_1")
        )

        self.assertIn("r18 is not saved by the edge handler", problems)


class CommitTest(unittest.TestCase):
    def test_window_runs_from_cli_through_the_instruction_after_sei(self) -> None:
        cycles = isr_check.window_cycles(
            isr_check.instructions(COMMIT, "fdswu_port_commit_heads")
        )

        self.assertEqual(cycles, 12)


class RunTest(unittest.TestCase):
    def test_run_returns_standard_output(self) -> None:
        output = isr_check.run([sys.executable, "-c", "print('listing')"])

        self.assertEqual(output, "listing\n")


class MainTest(unittest.TestCase):
    def run_main(self, *arguments: str, listing: str) -> tuple[int, str, str]:
        with (
            unittest.mock.patch.object(isr_check, "run", return_value=listing),
            unittest.mock.patch("sys.stdout", new_callable=io.StringIO) as out,
            unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as err,
        ):
            code = isr_check.main(["isr_check.py", "avr-objdump", "fw.elf", *arguments])
        return code, out.getvalue(), err.getvalue()

    def test_within_budgets_passes_and_reports(self) -> None:
        code, out, _ = self.run_main("33", "12", listing=HANDLER + COMMIT)

        self.assertEqual(code, 0)
        self.assertIn("edge handler: 32 of 33 cycles", out)
        self.assertIn("interrupts held off: 12 of 12 cycles", out)

    def test_over_budget_fails(self) -> None:
        code, _, err = self.run_main("31", "12", listing=HANDLER + COMMIT)

        self.assertEqual(code, 1)
        self.assertIn("edge handler takes 32 cycles, over 31", err)

    def test_long_window_fails(self) -> None:
        code, _, err = self.run_main("33", "11", listing=HANDLER + COMMIT)

        self.assertEqual(code, 1)
        self.assertIn("interrupts held off 12 cycles, over 11", err)

    def test_forbidden_instruction_fails(self) -> None:
        listing = HANDLER.replace("in\tr17, 0x14", "andi\tr17, 0x14") + COMMIT

        code, _, err = self.run_main("33", "12", listing=listing)

        self.assertEqual(code, 1)
        self.assertIn("andi", err)

    def test_instruction_without_a_cycle_count_fails(self) -> None:
        listing = HANDLER + COMMIT.replace("\tsei\n  de:", "\tsleep\n  de:")

        code, _, err = self.run_main("33", "12", listing=listing)

        self.assertEqual(code, 1)
        self.assertIn("sleep has no cycle count", err)

    def test_missing_symbol_fails(self) -> None:
        code, _, err = self.run_main("33", "12", listing=HANDLER)

        self.assertEqual(code, 1)
        self.assertIn("fdswu_port_commit_heads not found", err)

    def test_wrong_arguments_print_usage(self) -> None:
        with unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as err:
            code = isr_check.main(["isr_check.py"])

        self.assertEqual(code, 2)
        self.assertIn("usage", err.getvalue())


if __name__ == "__main__":
    unittest.main()
