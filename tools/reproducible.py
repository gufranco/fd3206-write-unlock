# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import hashlib
import sys
import tempfile
from pathlib import Path

from tools.mutation import Runner, copy_repository, run_command

HEX_PATHS = (
    "build/attiny2313a/release/fd3206-write-unlock-attiny2313a.hex",
    "build/attiny4313/release/fd3206-write-unlock-attiny4313.hex",
)
BUILD_TARGET = "images"
COPY_NAMES = ("first", "second-at-another-path")
USAGE_ERROR = 2
ARGUMENT_COUNT = 2

__all__ = ["BUILD_TARGET", "HEX_PATHS", "Runner", "main"]


class BuildError(RuntimeError):
    pass


def build_digests(root: Path, destination: Path, runner: Runner) -> tuple[str, ...]:
    copy_repository(root, destination)
    if runner(["make", BUILD_TARGET], destination) != 0:
        raise BuildError(f"build failed in {destination}")
    return tuple(
        hashlib.sha256((destination / path).read_bytes()).hexdigest()
        for path in HEX_PATHS
    )


def main(arguments: list[str], runner: Runner = run_command) -> int:
    if len(arguments) != ARGUMENT_COUNT:
        print("usage: reproducible.py <repository>", file=sys.stderr)
        return USAGE_ERROR
    root = Path(arguments[1])
    with tempfile.TemporaryDirectory() as scratch:
        try:
            first, second = (
                build_digests(root, Path(scratch) / name, runner) for name in COPY_NAMES
            )
        except BuildError as error:
            print(str(error), file=sys.stderr)
            return 1
    pairs = list(zip(HEX_PATHS, first, second, strict=True))
    for path, one, other in pairs:
        if one == other:
            print(f"reproducible: {path} is {one} from both copies")
        else:
            print(f"{path} differs: {one} and {other}", file=sys.stderr)
    return 0 if all(one == other for _, one, other in pairs) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
