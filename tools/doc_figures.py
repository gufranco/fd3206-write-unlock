# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

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
LICENCE_HEADER = re.compile(r"^\s*(?:/\*|#) SPDX-")
SECTION_ROW = re.compile(r"^(\.\w+)\s+(\d+)\s+\d+$")
SOURCE_GLOBS = ("src/*.c", "src/*.S", "include/**/*.h")
HANDLER_SYMBOL = "__vector_1"
POSITIONAL_ARGUMENTS = 5
USAGE_ERROR = 2


class MarkerError(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class Labels:
    header: str
    flash: str
    handler: str
    source: str


ENGLISH = Labels(
    header="| Figure | Value |",
    flash="| Flash used | {} bytes |",
    handler="| Edge handler | {} instructions |",
    source="| Firmware source | {} non-blank lines |",
)
JAPANESE = Labels(
    header="| 項目 | 値 |",
    flash="| フラッシュ使用量 | {} バイト |",
    handler="| エッジ割り込みハンドラ | {} 命令 |",
    source="| ファームウェアのソース | 空行を除き {} 行 |",
)
SIMPLIFIED_CHINESE = Labels(
    header="| 项目 | 数值 |",
    flash="| 闪存占用 | {} 字节 |",
    handler="| 边沿中断处理程序 | {} 条指令 |",
    source="| 固件源代码 | 非空行 {} 行 |",
)
HONG_KONG_CHINESE = Labels(
    header="| 項目 | 數值 |",
    flash="| 快閃記憶體用量 | {} 位元組 |",
    handler="| 邊緣中斷處理程式 | {} 條指令 |",
    source="| 韌體原始碼 | 非空行 {} 行 |",
)
READMES = {
    "README.md": ENGLISH,
    "README.ja.md": JAPANESE,
    "README.zh-CN.md": SIMPLIFIED_CHINESE,
    "README.zh-HK.md": HONG_KONG_CHINESE,
}


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
        1
        for path in files
        for line in path.read_text().splitlines()
        if line.strip() and not LICENCE_HEADER.match(line)
    )


def render(figures: Figures, labels: Labels) -> str:
    return "\n".join(
        [
            labels.header,
            "|---|---|",
            labels.flash.format(figures.flash_bytes),
            labels.handler.format(figures.handler_instructions),
            labels.source.format(figures.source_lines),
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


def check_readme(readme: Path, figures: Figures, labels: Labels, update: bool) -> str:
    if not readme.is_file():
        return f"{readme}: missing"
    current = readme.read_text()
    try:
        expected = replace_block(current, render(figures, labels))
    except MarkerError as error:
        return f"{readme}: figure markers: {error}"
    if update:
        readme.write_text(expected)
        return ""
    if expected != current:
        return f"{readme}: figures are stale, run make figures"
    return ""


def main(arguments: list[str]) -> int:
    if len(arguments) < POSITIONAL_ARGUMENTS:
        print(
            "usage: doc_figures.py <root> <size> <objdump> <firmware.elf> [--update]",
            file=sys.stderr,
        )
        return USAGE_ERROR
    root = Path(arguments[1])
    figures = measure(root, *arguments[2:POSITIONAL_ARGUMENTS])
    update = UPDATE_FLAG in arguments[POSITIONAL_ARGUMENTS:]
    results = [
        check_readme(root / name, figures, labels, update)
        for name, labels in READMES.items()
    ]
    errors = [result for result in results if result]
    for error in errors:
        print(error, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
