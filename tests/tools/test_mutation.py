# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import io
import sys
import tempfile
import unittest
import unittest.mock
from pathlib import Path

from tools import mutation

MUTANT = mutation.Mutant("inverted gate", "src/gate.c", "== 0U", "!= 0U")


def make_repository() -> Path:
    root = Path(tempfile.mkdtemp())
    (root / "src").mkdir()
    (root / "src" / "gate.c").write_text("return pins == 0U;\n")
    (root / "build").mkdir()
    (root / "build" / "stale.o").write_text("x")
    return root


def exit_codes(build: int, tests: int) -> mutation.Runner:
    def runner(command: list[str], _cwd: Path) -> int:
        return build if mutation.BUILD_TARGET in command else tests

    return runner


def mutant_only(build: int, tests: int) -> mutation.Runner:
    def runner(command: list[str], cwd: Path) -> int:
        mutated = (cwd / "src" / "gate.c").read_text() != "return pins == 0U;\n"
        return exit_codes(build, tests)(command, cwd) if mutated else 0

    return runner


class ApplyTest(unittest.TestCase):
    def test_mutation_replaces_the_single_occurrence(self) -> None:
        root = make_repository()

        mutation.apply(root, MUTANT)

        self.assertEqual((root / "src" / "gate.c").read_text(), "return pins != 0U;\n")

    def test_missing_text_raises(self) -> None:
        root = make_repository()
        stale = mutation.Mutant("stale", "src/gate.c", "absent", "x")

        with self.assertRaisesRegex(mutation.MutationError, "found 0 times"):
            mutation.apply(root, stale)

    def test_repeated_text_raises(self) -> None:
        root = make_repository()
        (root / "src" / "gate.c").write_text("== 0U == 0U\n")

        with self.assertRaisesRegex(mutation.MutationError, "found 2 times"):
            mutation.apply(root, MUTANT)


class LineMutantTest(unittest.TestCase):
    def test_deletion_removes_only_that_line(self) -> None:
        mutant = mutation.LineMutant("drop", "src/x.S", "a", 1, "    wdr", None)

        text = mutant.mutate("a:\n    wdr\n    ret")

        self.assertEqual(text, "a:\n    ret")

    def test_replacement_rewrites_only_that_line(self) -> None:
        mutant = mutation.LineMutant("swap", "src/x.S", "a", 0, "    sei", "    cli")

        text = mutant.mutate("    sei\n    sei")

        self.assertEqual(text, "    cli\n    sei")

    def test_changed_line_raises(self) -> None:
        mutant = mutation.LineMutant("stale", "src/x.S", "a", 0, "    sei", None)

        with self.assertRaisesRegex(mutation.MutationError, "line 1 changed"):
            mutant.mutate("    cli")


class LineVariantsTest(unittest.TestCase):
    def test_swappable_instruction_is_deleted_and_swapped(self) -> None:
        variants = mutation.line_variants("    sbic 0x15, 0")

        self.assertEqual(
            variants, [("deleted", None), ("sbic became sbis", "    sbis 0x15, 0")]
        )

    def test_load_immediate_is_deleted_and_zeroed(self) -> None:
        variants = mutation.line_variants("    ldi rWork, WATCHDOG_60MS")

        self.assertEqual(
            variants, [("deleted", None), ("immediate became 0", "    ldi rWork, 0")]
        )

    def test_zero_immediate_is_only_deleted(self) -> None:
        variants = mutation.line_variants("    ldi rWork, 0")

        self.assertEqual(variants, [("deleted", None)])

    def test_labels_and_directives_are_skipped(self) -> None:
        lines = ["fdswu_port_poll:", "    .global fdswu_port_poll", "#define rWork r25"]

        variants = [mutation.line_variants(line) for line in lines]

        self.assertEqual(variants, [[], [], []])


class EnclosingFunctionsTest(unittest.TestCase):
    def test_local_labels_stay_inside_their_function(self) -> None:
        lines = [
            "    .section x",
            "first:",
            "    wdr",
            ".Lloop:",
            "    ret",
            "second:",
            "    ret",
        ]

        functions = mutation.enclosing_functions(lines)

        self.assertEqual(
            functions, ["", "first", "first", "first", "first", "second", "second"]
        )


class CopyTest(unittest.TestCase):
    def test_copy_leaves_build_output_behind(self) -> None:
        root = make_repository()
        destination = Path(tempfile.mkdtemp()) / "copy"

        mutation.copy_repository(root, destination)

        self.assertTrue((destination / "src" / "gate.c").is_file())
        self.assertFalse((destination / "build").exists())


class OutcomeTest(unittest.TestCase):
    def test_build_failure_kills_the_mutant(self) -> None:
        outcome = mutation.outcome(Path(), exit_codes(build=2, tests=0))

        self.assertEqual(outcome, mutation.KILLED_BY_BUILD)

    def test_test_failure_kills_the_mutant(self) -> None:
        outcome = mutation.outcome(Path(), exit_codes(build=0, tests=2))

        self.assertEqual(outcome, mutation.KILLED_BY_TESTS)

    def test_passing_tests_let_the_mutant_survive(self) -> None:
        outcome = mutation.outcome(Path(), exit_codes(build=0, tests=0))

        self.assertEqual(outcome, mutation.SURVIVED)


class RunCommandTest(unittest.TestCase):
    def test_exit_code_is_returned(self) -> None:
        code = mutation.run_command(
            [sys.executable, "-c", "raise SystemExit(3)"], Path()
        )

        self.assertEqual(code, 3)


class MainTest(unittest.TestCase):
    def run_main(self, runner: mutation.Runner, mutants: tuple) -> tuple[int, str, str]:
        root = make_repository()
        with (
            unittest.mock.patch.object(mutation, "MUTANTS", mutants),
            unittest.mock.patch.object(mutation, "assembly_mutants", return_value=()),
            unittest.mock.patch("sys.stdout", new_callable=io.StringIO) as out,
            unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as err,
        ):
            code = mutation.main(["mutation.py", str(root)], runner)
        return code, out.getvalue(), err.getvalue()

    def test_all_killed_passes(self) -> None:
        code, out, _ = self.run_main(mutant_only(build=0, tests=2), (MUTANT,))

        self.assertEqual(code, 0)
        self.assertIn("killed by tests: inverted gate", out)

    def test_survivor_fails(self) -> None:
        code, _, err = self.run_main(mutant_only(build=0, tests=0), (MUTANT,))

        self.assertEqual(code, 1)
        self.assertIn("survived: inverted gate", err)

    def test_stale_mutant_fails(self) -> None:
        stale = mutation.Mutant("stale", "src/gate.c", "absent", "x")

        code, _, err = self.run_main(mutant_only(build=0, tests=2), (stale,))

        self.assertEqual(code, 1)
        self.assertIn("found 0 times", err)

    def test_failing_baseline_stops_before_any_mutant(self) -> None:
        code, out, err = self.run_main(exit_codes(build=2, tests=0), (MUTANT,))

        self.assertEqual(code, 1)
        self.assertIn("unmutated copy is killed by build", err)
        self.assertEqual(out, "")

    def test_wrong_arguments_print_usage(self) -> None:
        with unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as err:
            code = mutation.main(["mutation.py"])

        self.assertEqual(code, 2)
        self.assertIn("usage", err.getvalue())


class CatalogueTest(unittest.TestCase):
    def test_every_mutant_applies_to_the_real_sources(self) -> None:
        root = Path(__file__).resolve().parent.parent.parent

        counts = [(root / m.path).read_text().count(m.old) for m in mutation.MUTANTS]

        self.assertEqual(counts, [1] * len(mutation.MUTANTS))

    def test_every_equivalent_mutant_names_exactly_one_candidate(self) -> None:
        root = Path(__file__).resolve().parent.parent.parent

        keys = [mutation.equivalence_key(m) for m in mutation.candidate_mutants(root)]

        counts = [keys.count(key) for key in mutation.EQUIVALENT_MUTANTS]
        self.assertEqual(counts, [1] * len(mutation.EQUIVALENT_MUTANTS))

    def test_equivalent_mutants_are_not_run(self) -> None:
        root = Path(__file__).resolve().parent.parent.parent

        keys = {mutation.equivalence_key(m) for m in mutation.assembly_mutants(root)}

        self.assertEqual(keys & set(mutation.EQUIVALENT_MUTANTS), set())

    def test_every_assembly_instruction_has_a_mutant(self) -> None:
        root = Path(__file__).resolve().parent.parent.parent

        mutants = mutation.candidate_mutants(root)

        deleted = {(m.path, m.index) for m in mutants if m.new is None}
        instructions = {
            (path, index)
            for path in mutation.ASSEMBLY_SOURCES
            for index, line in enumerate((root / path).read_text().split("\n"))
            if mutation.INSTRUCTION.match(line)
        }
        self.assertEqual(deleted, instructions)


if __name__ == "__main__":
    unittest.main()
