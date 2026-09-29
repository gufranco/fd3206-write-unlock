# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

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
        "head latch set, so an enabled head drives high",
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
        "heads stay enabled when writing is not allowed",
        "src/heads.c",
        "allowed ? FDSWU_HEADS_MASK : 0U",
        "allowed ? FDSWU_HEADS_MASK : FDSWU_HEADS_MASK",
    ),
    Mutant(
        "interrupts left off after an update",
        "src/port.S",
        "    sei\n    ret\n.Lcommit_set:",
        "    ret\n.Lcommit_set:",
    ),
    Mutant(
        "edge handler stops advancing the plan",
        "src/write_data_edge.S",
        "    out _SFR_IO_ADDR(FDSWU_NEXT_EDGE_HEADS), rAfter\n",
        "",
    ),
)


def apply(root: Path, mutant: Mutant) -> None:
    path = root / mutant.path
    text = path.read_text()
    count = text.count(mutant.old)
    if count != 1:
        raise MutationError(f"{mutant.name}: text found {count} times in {mutant.path}")
    path.write_text(text.replace(mutant.old, mutant.new))


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


def judge(root: Path, mutant: Mutant | None, runner: Runner) -> str:
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
    for mutant in MUTANTS:
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
