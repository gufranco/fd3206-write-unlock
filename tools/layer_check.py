# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import re
import sys
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
STANDARD_HEADERS = frozenset({"stdint.h", "stddef.h", "stdbool.h"})
LOGIC_PREFIX = "fdswu/"
PORT_PREFIX = "port/"
HARDWARE_PREFIXES = ("avr/", "util/", PORT_PREFIX)
PLATFORM_SOURCES = frozenset({"main.c"})
ASSEMBLY_SUFFIX = ".S"
SOURCE_SUFFIX = ".c"


@dataclass(frozen=True, slots=True)
class Violation:
    path: Path
    included: str
    reason: str


def includes(path: Path) -> list[str]:
    matches = (INCLUDE.match(line) for line in path.read_text().splitlines())
    return [match.group(1) for match in matches if match]


def logic_allows(included: str) -> bool:
    return included in STANDARD_HEADERS or included.startswith(LOGIC_PREFIX)


def platform_c_allows(included: str) -> bool:
    return logic_allows(included) or included.startswith(PORT_PREFIX)


def hardware_allows(included: str) -> bool:
    return logic_allows(included) or included.startswith(HARDWARE_PREFIXES)


def is_hardware(path: Path) -> bool:
    return path.suffix == ASSEMBLY_SUFFIX or PORT_PREFIX.rstrip("/") in path.parts


def rule_for(path: Path) -> Callable[[str], bool]:
    if is_hardware(path):
        return hardware_allows
    return platform_c_allows if path.name in PLATFORM_SOURCES else logic_allows


def check(path: Path) -> list[Violation]:
    allows = rule_for(path)
    found = [
        Violation(path, name, "source file included")
        for name in includes(path)
        if name.endswith(SOURCE_SUFFIX)
    ]
    layered = [
        Violation(path, name, "crosses a layer")
        for name in includes(path)
        if not allows(name)
    ]
    return [
        *found,
        *(
            violation
            for violation in layered
            if violation.included not in {v.included for v in found}
        ),
    ]


def violations(root: Path) -> list[Violation]:
    files = sorted(
        [*root.glob("include/**/*.h"), *root.glob("src/*.c"), *root.glob("src/*.S")]
    )
    return [violation for path in files for violation in check(path)]


def main(arguments: list[str]) -> int:
    found = violations(Path(arguments[1]))
    for violation in found:
        print(
            f"{violation.path}: {violation.reason}: {violation.included}",
            file=sys.stderr,
        )
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
