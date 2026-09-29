# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import re
import subprocess
import sys
from pathlib import Path

TYPES = (
    "feat",
    "fix",
    "perf",
    "refactor",
    "docs",
    "style",
    "test",
    "build",
    "ci",
    "chore",
    "revert",
)
HEADER = re.compile(rf"^(?:{'|'.join(TYPES)})(?:\([a-z0-9][a-z0-9._/-]*\))?!?: \S")
GIT_GENERATED = re.compile(r'^(?:Merge |Revert ")')
ZERO_SHA = re.compile(r"^0+$")
SKIP_TOKEN = re.compile(
    r"\[(?:skip ci|ci skip|no ci|skip actions|actions skip)\]|^skip-checks: *true",
    re.IGNORECASE | re.MULTILINE,
)
RELEASE_HEADER = re.compile(r"^chore\(release\): \d+\.\d+\.\d+ \[skip ci\]$")
SKIP_PROBLEM = "message asks GitHub to skip every workflow for this commit"
MAX_HEADER = 100
USAGE_ERROR = 2
ARGUMENT_COUNT = 3
REPOSITORY: Path | None = None


def content_lines(message: str) -> list[str]:
    return [line for line in message.splitlines() if not line.startswith("#")]


def header_of(message: str) -> str:
    return next((line for line in content_lines(message) if line.strip()), "")


def skips_ci(message: str, header: str) -> bool:
    found = SKIP_TOKEN.search("\n".join(content_lines(message))) is not None
    return found and not RELEASE_HEADER.match(header)


def check(message: str) -> list[str]:
    header = header_of(message)
    if GIT_GENERATED.match(header):
        return []
    problems = []
    if not HEADER.match(header):
        problems = [f"'{header}' is not type(scope): subject, types {', '.join(TYPES)}"]
    if len(header) > MAX_HEADER:
        problems = [*problems, f"header is {len(header)} characters, over {MAX_HEADER}"]
    if header.endswith("."):
        problems = [*problems, "subject ends with a period"]
    if skips_ci(message, header):
        problems = [*problems, SKIP_PROBLEM]
    return problems


def git(*arguments: str) -> str:
    location = ["-C", str(REPOSITORY)] if REPOSITORY else []
    command = ["git", *location, *arguments]
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def commits_in(revision_range: str) -> list[str]:
    base, _, head = revision_range.partition("..")
    if ZERO_SHA.match(base):
        return [head]
    return git("rev-list", "--reverse", revision_range).split()


def report(label: str, problems: list[str]) -> list[str]:
    return [f"{label}: {problem}" for problem in problems]


def main(arguments: list[str]) -> int:
    mode = arguments[1] if len(arguments) == ARGUMENT_COUNT else ""
    if mode == "--file":
        errors = report("commit message", check(Path(arguments[2]).read_text()))
    elif mode == "--range":
        errors = [
            error
            for sha in commits_in(arguments[2])
            for error in report(sha[:12], check(git("log", "-1", "--format=%B", sha)))
        ]
    else:
        print(
            "usage: commit_message.py --file <path> | --range <a>..<b>", file=sys.stderr
        )
        return USAGE_ERROR
    for error in errors:
        print(error, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
