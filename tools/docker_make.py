# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import hashlib
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
IMAGE_INPUTS = ("Dockerfile", "docker/requirements-tools.txt")
IMAGE_NAME = "fdswriteunlock-toolchain"
TAG_LENGTH = 12


def image_tag(root: Path) -> str:
    digest = hashlib.sha256()
    for name in IMAGE_INPUTS:
        digest.update((root / name).read_bytes())
    return f"{IMAGE_NAME}:{digest.hexdigest()[:TAG_LENGTH]}"


def toolchain_command(
    root: Path, tag: str, user: tuple[int, int], targets: list[str]
) -> list[str]:
    uid, gid = user
    return [
        "docker",
        "run",
        "--rm",
        "--user",
        f"{uid}:{gid}",
        "-e",
        "HOME=/tmp",
        "-v",
        f"{root}:/src",
        "-w",
        "/src",
        tag,
        "make",
        *targets,
    ]


def ensure_image(root: Path, tag: str) -> None:
    present = subprocess.run(
        ["docker", "image", "inspect", tag], capture_output=True, check=False
    )
    if present.returncode != 0:
        subprocess.run(["docker", "build", "-t", tag, str(root)], check=True)


def main(arguments: list[str]) -> int:
    tag = image_tag(ROOT)
    ensure_image(ROOT, tag)
    command = toolchain_command(ROOT, tag, (os.getuid(), os.getgid()), arguments[1:])
    return subprocess.run(command, check=False).returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv))
