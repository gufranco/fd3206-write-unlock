# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import re
import sys
from dataclasses import dataclass
from pathlib import Path

LITERAL = re.compile(r"'(?:\\.|[^'\\])*'|\"(?:\\.|[^\"\\])*\"")
C_COMMENT = re.compile(r"//|/\*")
ASSEMBLY_COMMENT = re.compile(r";|//|/\*")
SPDX_HEADER = re.compile(
    r"^/\* SPDX-(?:FileCopyrightText|License-Identifier): [^*]+ \*/$"
)
PLATFORM_WIDTH_TYPE = re.compile(r"\b(char|short|long|unsigned|signed|float|double)\b")
INT_TYPE = re.compile(r"\bint\b")
MAIN_DEFINITION = re.compile(r"^int main\(void\)")
BANNED_CONSTRUCT = re.compile(
    r"\b(malloc|calloc|realloc|free|alloca|goto|setjmp|longjmp)\b"
)
FUNCTION_POINTER = re.compile(r"\(\s*\*\s*\w+\s*\)\s*\(")
DEFINITION = re.compile(r"^[A-Za-z_][\w\s*]*?\b(\w+)\s*\([^;]*\)\s*\{$")
ASSEMBLY_SUFFIX = ".S"
MAX_FUNCTION_LINES = 60


@dataclass(frozen=True, slots=True)
class Finding:
    path: Path
    line: int
    rule: str
    text: str


@dataclass(frozen=True, slots=True)
class OpenFunction:
    name: str
    start: int


def strip_literals(line: str) -> str:
    return LITERAL.sub('""', line)


def line_findings(path: Path, number: int, code: str) -> list[Finding]:
    checks = [
        ("comment", C_COMMENT.search(code) and not SPDX_HEADER.match(code)),
        (
            "type",
            PLATFORM_WIDTH_TYPE.search(code)
            or (INT_TYPE.search(code) and not MAIN_DEFINITION.match(code)),
        ),
        ("construct", BANNED_CONSTRUCT.search(code)),
        ("function-pointer", FUNCTION_POINTER.search(code)),
    ]
    return [Finding(path, number, rule, code.strip()) for rule, hit in checks if hit]


def function_findings(
    path: Path, number: int, code: str, open_function: OpenFunction | None
) -> list[Finding]:
    if open_function is None:
        return []
    if code == "}":
        length = number - open_function.start - 1
        too_long = length > MAX_FUNCTION_LINES
        return (
            [Finding(path, number, "length", f"{open_function.name} is {length} lines")]
            if too_long
            else []
        )
    calls_itself = re.search(rf"\b{re.escape(open_function.name)}\s*\(", code)
    return [Finding(path, number, "recursion", code.strip())] if calls_itself else []


def track_function(
    number: int, code: str, open_function: OpenFunction | None
) -> OpenFunction | None:
    definition = DEFINITION.match(code)
    if definition:
        return OpenFunction(definition.group(1), number)
    return None if code == "}" else open_function


def check_c(path: Path, lines: list[str]) -> list[Finding]:
    findings: list[Finding] = []
    open_function: OpenFunction | None = None
    for number, raw in enumerate(lines, start=1):
        code = strip_literals(raw.rstrip("\n"))
        findings = [*findings, *line_findings(path, number, code)]
        findings = [*findings, *function_findings(path, number, code, open_function)]
        open_function = track_function(number, code, open_function)
    return findings


def check_assembly(path: Path, lines: list[str]) -> list[Finding]:
    return [
        Finding(path, number, "comment", line.strip())
        for number, line in enumerate(lines, start=1)
        if ASSEMBLY_COMMENT.search(line) and not SPDX_HEADER.match(line.strip())
    ]


def check_file(path: Path) -> list[Finding]:
    lines = path.read_text().splitlines()
    return (
        check_assembly(path, lines)
        if path.suffix == ASSEMBLY_SUFFIX
        else check_c(path, lines)
    )


def main(arguments: list[str]) -> int:
    findings = [finding for name in arguments[1:] for finding in check_file(Path(name))]
    for finding in findings:
        print(
            f"{finding.path}:{finding.line}: {finding.rule}: {finding.text}",
            file=sys.stderr,
        )
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
