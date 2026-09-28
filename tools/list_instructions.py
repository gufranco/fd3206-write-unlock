# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import re
import subprocess
import sys
from pathlib import Path

HEADER = re.compile(r"^([0-9a-f]+) <([^>]+)>:$")
INSTRUCTION = re.compile(r"^\s*([0-9a-f]+):\t[^\t]*\t(\S+)")
NOT_EXECUTED = frozenset({"nop", ".word"})
LOCAL_LABEL_PREFIX = ".L"
TEXT_SYMBOL_TYPES = frozenset({"T", "t"})
SYMBOL_FIELDS = 3
MINIMUM_ARGUMENTS = 5
USAGE_ERROR = 2


def function_names(nm_output: str) -> set[str]:
    fields = (line.split() for line in nm_output.splitlines())
    return {
        parts[2]
        for parts in fields
        if len(parts) == SYMBOL_FIELDS and parts[1] in TEXT_SYMBOL_TYPES
    }


def base_name(symbol: str) -> str:
    return symbol.split(".", 1)[0]


def instruction_addresses(
    objdump_output: str, owned: set[str]
) -> list[tuple[int, str]]:
    current: str | None = None
    lines: list[tuple[int, str]] = []
    for line in objdump_output.splitlines():
        header = HEADER.match(line)
        if header and header.group(2).startswith(LOCAL_LABEL_PREFIX):
            continue
        if header:
            current = header.group(2) if base_name(header.group(2)) in owned else None
            continue
        instruction = INSTRUCTION.match(line)
        if current and instruction and instruction.group(2) not in NOT_EXECUTED:
            lines = [*lines, (int(instruction.group(1), 16), current)]
    return lines


def run(command: list[str]) -> str:
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def main(arguments: list[str]) -> int:
    if len(arguments) < MINIMUM_ARGUMENTS:
        print(
            f"usage: {Path(arguments[0]).name} <objdump> <nm> <elf> <object>...",
            file=sys.stderr,
        )
        return USAGE_ERROR
    objdump_tool, nm_tool, elf, *objects = arguments[1:]
    owned = function_names(run([nm_tool, "--defined-only", *objects]))
    if not owned:
        print(f"no functions defined in {' '.join(objects)}", file=sys.stderr)
        return 1
    addresses = instruction_addresses(run([objdump_tool, "-d", elf]), owned)
    print("\n".join(f"{address} {name}" for address, name in addresses))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
