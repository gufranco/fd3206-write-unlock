# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import re
import subprocess
import sys
from dataclasses import dataclass

from tools.list_instructions import HEADER, LOCAL_LABEL_PREFIX

LINE = re.compile(
    r"^\s*([0-9a-f]+):\t[^\t]*\t(\S+)(?:\t([^;]*))?(?:;\s*0x([0-9a-f]+))?"
)
REGISTER = re.compile(r"\br(\d+)\b")
HANDLER_SYMBOL = "__vector_1"
COMMIT_SYMBOL = "fdswu_port_commit_write_lines"
HANDLER_MNEMONICS = frozenset(
    {"push", "pop", "in", "out", "sbis", "sbic", "sbi", "cbi", "rjmp", "reti"}
)
HANDLER_REGISTERS = frozenset({"r16", "r17"})
CYCLES = {
    "push": 2,
    "pop": 2,
    "in": 1,
    "out": 1,
    "sbi": 2,
    "cbi": 2,
    "rjmp": 2,
    "reti": 4,
    "ret": 4,
    "cli": 1,
    "sei": 1,
    "nop": 1,
}
SKIPS = frozenset({"sbis", "sbic"})
RETURNS = frozenset({"ret", "reti"})
INSTRUCTION_BYTES = 2
RESPONSE_CYCLES = 4
VECTOR_JUMP_CYCLES = 2
IN_PROGRESS_CYCLES = 3
AFTER_SEI_CYCLES = 4
ARGUMENT_COUNT = 5
USAGE_ERROR = 2


class ListingError(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class Instruction:
    address: int
    mnemonic: str
    operands: str
    target: int | None


def instructions(objdump_output: str, symbol: str) -> list[Instruction]:
    inside = False
    found: list[Instruction] = []
    for line in objdump_output.splitlines():
        header = HEADER.match(line)
        if header and not header.group(2).startswith(LOCAL_LABEL_PREFIX):
            inside = header.group(2) == symbol
            continue
        match = LINE.match(line)
        if inside and match:
            target = int(match.group(4), 16) if match.group(4) else None
            found = [
                *found,
                Instruction(
                    int(match.group(1), 16),
                    match.group(2),
                    (match.group(3) or "").strip(),
                    target,
                ),
            ]
    if not found:
        raise ListingError(f"{symbol} not found")
    return found


def cost(mnemonic: str) -> int:
    if mnemonic not in CYCLES:
        raise ListingError(f"{mnemonic} has no cycle count")
    return CYCLES[mnemonic]


def worst_cycles(program: list[Instruction], index: int, stop: str) -> int:
    instruction = program[index]
    if instruction.mnemonic == stop:
        return cost(stop) + AFTER_SEI_CYCLES
    if instruction.mnemonic in RETURNS:
        return cost(instruction.mnemonic)
    if instruction.mnemonic in SKIPS:
        return max(
            1 + worst_cycles(program, index + 1, stop),
            2 + worst_cycles(program, index + 2, stop),
        )
    if instruction.mnemonic == "rjmp":
        addresses = [item.address for item in program]
        return cost("rjmp") + worst_cycles(
            program, addresses.index(instruction.target), stop
        )
    return cost(instruction.mnemonic) + worst_cycles(program, index + 1, stop)


def handler_cycles(program: list[Instruction]) -> int:
    return worst_cycles(program, 0, stop="")


def window_cycles(program: list[Instruction]) -> int:
    start = [item.mnemonic for item in program].index("cli")
    return worst_cycles(program, start, stop="sei")


def handler_problems(program: list[Instruction]) -> list[str]:
    mnemonics = [
        f"{item.mnemonic} is not allowed in the edge handler"
        for item in program
        if item.mnemonic not in HANDLER_MNEMONICS
    ]
    registers = [
        f"r{number} is not saved by the edge handler"
        for item in program
        for number in REGISTER.findall(item.operands)
        if f"r{number}" not in HANDLER_REGISTERS
    ]
    return [*mnemonics, *registers]


def run(command: list[str]) -> str:
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def check(listing: str, handler_budget: int, window_budget: int) -> list[str]:
    handler = instructions(listing, HANDLER_SYMBOL)
    forbidden = handler_problems(handler)
    if forbidden:
        return forbidden
    window = window_cycles(instructions(listing, COMMIT_SYMBOL))
    total = (
        handler_cycles(handler)
        + RESPONSE_CYCLES
        + VECTOR_JUMP_CYCLES
        + IN_PROGRESS_CYCLES
    )
    print(f"edge handler: {total} of {handler_budget} cycles")
    print(f"interrupts held off: {window} of {window_budget} cycles")
    problems: list[str] = []
    if total > handler_budget:
        problems = [
            *problems,
            f"edge handler takes {total} cycles, over {handler_budget}",
        ]
    if window > window_budget:
        problems = [
            *problems,
            f"interrupts held off {window} cycles, over {window_budget}",
        ]
    return problems


def main(arguments: list[str]) -> int:
    if len(arguments) != ARGUMENT_COUNT:
        print(
            "usage: isr_check.py <objdump> <elf> <handler-cycles> <window-cycles>",
            file=sys.stderr,
        )
        return USAGE_ERROR
    objdump_tool, elf, handler_budget, window_budget = arguments[1:]
    try:
        problems = check(
            run([objdump_tool, "-d", elf]), int(handler_budget), int(window_budget)
        )
    except ListingError as error:
        problems = [str(error)]
    for problem in problems:
        print(problem, file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
