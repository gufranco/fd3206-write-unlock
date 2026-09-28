import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

from tools.list_instructions import instruction_addresses

BEGIN = "<!-- figures:begin -->"
END = "<!-- figures:end -->"
UPDATE_FLAG = "--update"
FLASH_SECTIONS = frozenset({".text", ".data"})
SECTION_ROW = re.compile(r"^(\.\w+)\s+(\d+)\s+\d+$")
SOURCE_GLOBS = ("src/*.c", "src/*.S", "include/**/*.h")
README_NAME = "README.md"
HANDLER_SYMBOL = "__vector_1"
POSITIONAL_ARGUMENTS = 5
USAGE_ERROR = 2


class MarkerError(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class Figures:
    flash_bytes: int
    handler_instructions: int
    source_lines: int


def flash_bytes(size_output: str) -> int:
    rows = (SECTION_ROW.match(line.strip()) for line in size_output.splitlines())
    return sum(
        int(row.group(2)) for row in rows if row and row.group(1) in FLASH_SECTIONS
    )


def handler_instructions(objdump_output: str, symbol: str) -> int:
    return len(instruction_addresses(objdump_output, {symbol}))


def source_lines(root: Path) -> int:
    files = sorted({path for pattern in SOURCE_GLOBS for path in root.glob(pattern)})
    return sum(
        1 for path in files for line in path.read_text().splitlines() if line.strip()
    )


def render(figures: Figures) -> str:
    return "\n".join(
        [
            "| Figure | Value |",
            "|---|---|",
            f"| Flash used | {figures.flash_bytes} bytes |",
            f"| Edge handler | {figures.handler_instructions} instructions |",
            f"| Firmware source | {figures.source_lines} non-blank lines |",
        ]
    )


def replace_block(text: str, block: str) -> str:
    start = text.find(BEGIN)
    end = text.find(END)
    if start < 0 or end < start:
        raise MarkerError(f"expected {BEGIN} followed by {END}")
    return f"{text[: start + len(BEGIN)]}\n{block}\n{text[end:]}"


def run(command: list[str]) -> str:
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def measure(root: Path, size_tool: str, objdump_tool: str, elf: str) -> Figures:
    return Figures(
        flash_bytes=flash_bytes(run([size_tool, "-A", elf])),
        handler_instructions=handler_instructions(
            run([objdump_tool, "-d", elf]), HANDLER_SYMBOL
        ),
        source_lines=source_lines(root),
    )


def main(arguments: list[str]) -> int:
    if len(arguments) < POSITIONAL_ARGUMENTS:
        print(
            "usage: doc_figures.py <root> <size> <objdump> <firmware.elf> [--update]",
            file=sys.stderr,
        )
        return USAGE_ERROR
    root = Path(arguments[1])
    readme = root / README_NAME
    current = readme.read_text()
    try:
        expected = replace_block(
            current, render(measure(root, *arguments[2:POSITIONAL_ARGUMENTS]))
        )
    except MarkerError as error:
        print(f"{readme}: figure markers: {error}", file=sys.stderr)
        return 1
    if UPDATE_FLAG in arguments[POSITIONAL_ARGUMENTS:]:
        readme.write_text(expected)
        return 0
    if expected != current:
        print(
            f"{readme}: figures are stale, run make figures",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
