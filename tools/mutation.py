# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import re
import shutil
import subprocess
import sys
import tempfile
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

Runner = Callable[[list[str], Path], int]

BUILD_TARGET = "images"
TEST_TARGET = "test"
KILLED_BY_BUILD = "killed by build"
KILLED_BY_TESTS = "killed by tests"
SURVIVED = "survived"
IGNORED = shutil.ignore_patterns(
    "build", ".git", "node_modules", "docs", "specs", "__pycache__", "dist"
)
ASSEMBLY_SOURCES = ("src/port.S", "src/write_data_edge.S")
INSTRUCTION = re.compile(r"^    ([a-z]+)\b(.*)$")
LOAD_IMMEDIATE = re.compile(r"^(    ldi \w+, )(.+)$")
FUNCTION_LABEL = re.compile(r"^([A-Za-z_]\w*):")
SWAPPED_MNEMONICS = {
    "sbis": "sbic",
    "sbic": "sbis",
    "sbi": "cbi",
    "cbi": "sbi",
    "cli": "sei",
    "sei": "cli",
    "breq": "brne",
    "brne": "breq",
}
EQUIVALENT_MUTANTS = {
    ("src/port.S", "fdswu_port_start", "deleted", "wdr"): (
        "the datasheet's timed watchdog sequence starts with wdr, and the reset "
        "that ran a few cycles earlier already restarted the counter"
    ),
    ("src/port.S", "fdswu_port_start", "deleted", "ret"): (
        "the linker places fdswu_port_poll next on both chips, and its own ret "
        "returns to the caller, which ignores the registers it loaded"
    ),
}
USAGE_ERROR = 2
ARGUMENT_COUNT = 2


class MutationError(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class Mutant:
    name: str
    path: str
    old: str
    new: str

    def mutate(self, text: str) -> str:
        count = text.count(self.old)
        if count != 1:
            raise MutationError(f"{self.name}: text found {count} times in {self.path}")
        return text.replace(self.old, self.new)


@dataclass(frozen=True, slots=True)
class LineMutant:
    name: str
    path: str
    function: str
    index: int
    old: str
    new: str | None

    def mutate(self, text: str) -> str:
        lines = text.split("\n")
        if self.index >= len(lines) or lines[self.index] != self.old:
            raise MutationError(
                f"{self.name}: line {self.index + 1} changed in {self.path}"
            )
        replacement = [] if self.new is None else [self.new]
        return "\n".join([*lines[: self.index], *replacement, *lines[self.index + 1 :]])


MUTANTS = (
    Mutant(
        "rising edge instead of falling",
        "src/port.S",
        "    ldi rWork, _BV(ISC01)\n",
        "    ldi rWork, _BV(ISC01) | _BV(ISC00)\n",
    ),
    Mutant(
        "WRITE DATA pull-up dropped",
        "include/fdswu/pins.h",
        "((uint8_t)0x7FU)",
        "((uint8_t)0x7BU)",
    ),
    Mutant(
        "write line latch set, so an enabled line drives high",
        "src/port.S",
        "    out _SFR_IO_ADDR(PORTB), rSecond\n",
        "    ori rSecond, 0x0C\n    out _SFR_IO_ADDR(PORTB), rSecond\n",
    ),
    Mutant(
        "watchdog never serviced",
        "src/port.S",
        "fdswu_port_poll:\n    wdr\n",
        "fdswu_port_poll:\n",
    ),
    Mutant(
        "watchdog left disabled",
        "src/port.S",
        "    ldi rWork, WATCHDOG_60MS\n",
        "    ldi rWork, 0\n",
    ),
    Mutant(
        "clock left divided by 8",
        "src/port.S",
        "    out _SFR_IO_ADDR(CLKPR), r1\n",
        "    ldi rWork, 3\n    out _SFR_IO_ADDR(CLKPR), rWork\n",
    ),
    Mutant(
        "gate and writable condition inverted",
        "src/conditions.c",
        "FDSWU_GATE_AND_WRITABLE_MASK) == 0U)",
        "FDSWU_GATE_AND_WRITABLE_MASK) != 0U)",
    ),
    Mutant(
        "ready ignored",
        "src/conditions.c",
        " && ((port_b_pins & FDSWU_READY_MASK) == 0U)",
        "",
    ),
    Mutant(
        "write lines stay enabled when writing is not allowed",
        "src/write_plan.c",
        "allowed ? FDSWU_WRITE_LINES_MASK : 0U",
        "allowed ? FDSWU_WRITE_LINES_MASK : FDSWU_WRITE_LINES_MASK",
    ),
    Mutant(
        "interrupts left off after an update",
        "src/port.S",
        "    sei\n    ret\n.Lcommit_set:",
        "    ret\n.Lcommit_set:",
    ),
    Mutant(
        "write plan assertion inverted",
        "src/write_plan.c",
        "FDSWU_ASSERT((plan.now & plan.next_edge) == 0U);",
        "FDSWU_ASSERT((plan.now & plan.next_edge) != 0U);",
    ),
    Mutant(
        "edge handler stops advancing the plan",
        "src/write_data_edge.S",
        "    out _SFR_IO_ADDR(FDSWU_NEXT_EDGE_LINES), rAfter\n",
        "",
    ),
)


def line_variants(line: str) -> list[tuple[str, str | None]]:
    instruction = INSTRUCTION.match(line)
    if instruction is None:
        return []
    mnemonic = instruction.group(1)
    variants: list[tuple[str, str | None]] = [("deleted", None)]
    if mnemonic in SWAPPED_MNEMONICS:
        swapped = f"    {SWAPPED_MNEMONICS[mnemonic]}{instruction.group(2)}"
        variants = [
            *variants,
            (f"{mnemonic} became {SWAPPED_MNEMONICS[mnemonic]}", swapped),
        ]
    immediate = LOAD_IMMEDIATE.match(line)
    if immediate and immediate.group(2) != "0":
        variants = [*variants, ("immediate became 0", f"{immediate.group(1)}0")]
    return variants


def enclosing_functions(lines: list[str]) -> list[str]:
    labels = [FUNCTION_LABEL.match(line) for line in lines]
    return [
        next((m.group(1) for m in reversed(labels[: index + 1]) if m), "")
        for index in range(len(lines))
    ]


def candidate_mutants(root: Path) -> tuple[LineMutant, ...]:
    return tuple(
        LineMutant(
            f"{path}:{index + 1} {change}: {line.strip()}",
            path,
            function,
            index,
            line,
            new,
        )
        for path in ASSEMBLY_SOURCES
        for lines in [(root / path).read_text().split("\n")]
        for index, (line, function) in enumerate(
            zip(lines, enclosing_functions(lines), strict=True)
        )
        for change, new in line_variants(line)
    )


def equivalence_key(mutant: LineMutant) -> tuple[str, str, str, str]:
    change = mutant.name.split(" ", 1)[1].split(":", 1)[0]
    return (mutant.path, mutant.function, change, mutant.old.strip())


def assembly_mutants(root: Path) -> tuple[LineMutant, ...]:
    return tuple(
        mutant
        for mutant in candidate_mutants(root)
        if equivalence_key(mutant) not in EQUIVALENT_MUTANTS
    )


def apply(root: Path, mutant: Mutant | LineMutant) -> None:
    path = root / mutant.path
    path.write_text(mutant.mutate(path.read_text()))


def copy_repository(root: Path, destination: Path) -> None:
    shutil.copytree(root, destination, ignore=IGNORED)


def run_command(command: list[str], cwd: Path) -> int:
    return subprocess.run(command, cwd=cwd, check=False, capture_output=True).returncode


def outcome(root: Path, runner: Runner) -> str:
    if runner(["make", BUILD_TARGET], root) != 0:
        return KILLED_BY_BUILD
    if runner(["make", TEST_TARGET], root) != 0:
        return KILLED_BY_TESTS
    return SURVIVED


def judge(root: Path, mutant: Mutant | LineMutant | None, runner: Runner) -> str:
    with tempfile.TemporaryDirectory() as scratch:
        copy = Path(scratch) / "repository"
        copy_repository(root, copy)
        if mutant is not None:
            apply(copy, mutant)
        return outcome(copy, runner)


def main(arguments: list[str], runner: Runner = run_command) -> int:
    if len(arguments) != ARGUMENT_COUNT:
        print("usage: mutation.py <repository>", file=sys.stderr)
        return USAGE_ERROR
    root = Path(arguments[1])
    baseline = judge(root, None, runner)
    if baseline != SURVIVED:
        print(
            f"unmutated copy is {baseline}; no mutant result means anything",
            file=sys.stderr,
        )
        return 1
    failures = []
    for mutant in (*MUTANTS, *assembly_mutants(root)):
        try:
            result = judge(root, mutant, runner)
        except MutationError as error:
            failures = [*failures, str(error)]
            continue
        line = f"{result}: {mutant.name}"
        if result == SURVIVED:
            failures = [*failures, line]
        else:
            print(line)
    for failure in failures:
        print(failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
