import re
import subprocess
import sys
from pathlib import Path

HEADER = re.compile(r"^([0-9a-f]+) <([^>]+)>:$")
INSTRUCTION = re.compile(r"^\s*([0-9a-f]+):\t[^\t]*\t(\S+)")
NOT_EXECUTED = {"nop", ".word"}
FUNCTION_TYPES = {"T", "t"}


def defined_functions(nm_tool: str, objects: list[str]) -> set[str]:
    output = subprocess.run(
        [nm_tool, "--defined-only", *objects],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    fields = (line.split() for line in output.splitlines())
    return {
        parts[2] for parts in fields if len(parts) == 3 and parts[1] in FUNCTION_TYPES
    }


def instruction_lines(objdump_tool: str, elf: str, functions: set[str]) -> list[str]:
    output = subprocess.run(
        [objdump_tool, "-d", elf], check=True, capture_output=True, text=True
    ).stdout
    current = None
    lines = []
    for line in output.splitlines():
        header = HEADER.match(line)
        if header:
            current = header.group(2) if header.group(2) in functions else None
            continue
        instruction = INSTRUCTION.match(line)
        if current and instruction and instruction.group(2) not in NOT_EXECUTED:
            lines.append(f"{int(instruction.group(1), 16)} {current}")
    return lines


def main(arguments: list[str]) -> int:
    if len(arguments) < 5:
        print(
            f"usage: {Path(arguments[0]).name} <avr-objdump> <avr-nm> <firmware.elf> <object>...",
            file=sys.stderr,
        )
        return 2
    objdump_tool, nm_tool, elf, *objects = arguments[1:]
    functions = defined_functions(nm_tool, objects)
    if not functions:
        print(f"no functions defined in {' '.join(objects)}", file=sys.stderr)
        return 1
    print("\n".join(instruction_lines(objdump_tool, elf, functions)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
