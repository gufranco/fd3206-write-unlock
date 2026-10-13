# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import re
import sys
from pathlib import Path

TAG = re.compile(r"^v(\d+\.\d+\.\d+)$")
SECTION = re.compile(r"^## \[?(\d+\.\d+\.\d+)\]?[ (]")
ARGUMENT_COUNT = 3
USAGE_ERROR = 2


class NotesError(ValueError):
    pass


def version_of(tag: str) -> str:
    match = TAG.match(tag)
    if match is None:
        raise NotesError(f"not a release tag: {tag}")
    return match.group(1)


def section_starts(lines: list[str]) -> list[tuple[int, str]]:
    matches = [(index, SECTION.match(line)) for index, line in enumerate(lines)]
    return [(index, match.group(1)) for index, match in matches if match]


def notes_for(changelog: str, version: str) -> str:
    lines = changelog.splitlines()
    starts = section_starts(lines)
    found = [position for position, (_, name) in enumerate(starts) if name == version]
    if not found:
        raise NotesError(f"no CHANGELOG section for {version}")
    first = starts[found[0]][0]
    following = starts[found[0] + 1 :]
    last = following[0][0] if following else len(lines)
    return "\n".join(lines[first:last]).strip() + "\n"


def main(arguments: list[str]) -> int:
    if len(arguments) != ARGUMENT_COUNT:
        print("usage: release_notes.py <CHANGELOG.md> <vX.Y.Z>", file=sys.stderr)
        return USAGE_ERROR
    try:
        text = notes_for(Path(arguments[1]).read_text(), version_of(arguments[2]))
    except NotesError as error:
        print(str(error), file=sys.stderr)
        return 1
    sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
