import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools import docker_make

DOCKERFILE = "FROM scratch\n"
REQUIREMENTS = "ruff==1.0\n"


def make_root(dockerfile: str) -> Path:
    root = Path(tempfile.mkdtemp())
    (root / "docker").mkdir()
    (root / "Dockerfile").write_text(dockerfile)
    (root / "docker" / "requirements-tools.txt").write_text(REQUIREMENTS)
    return root


def completed(returncode: int) -> subprocess.CompletedProcess[bytes]:
    return subprocess.CompletedProcess(args=[], returncode=returncode)


class ImageTagTest(unittest.TestCase):
    def test_tag_names_the_image_and_a_twelve_character_digest(self) -> None:
        root = make_root(DOCKERFILE)

        tag = docker_make.image_tag(root)

        name, digest = tag.split(":")
        self.assertEqual(name, "fdswriteunlock-toolchain")
        self.assertEqual(len(digest), 12)

    def test_tag_changes_when_the_dockerfile_changes(self) -> None:
        first = docker_make.image_tag(make_root(DOCKERFILE))

        second = docker_make.image_tag(make_root(DOCKERFILE + "RUN true\n"))

        self.assertNotEqual(first, second)


class ToolchainCommandTest(unittest.TestCase):
    def test_command_mounts_the_root_and_runs_make_as_the_caller(self) -> None:
        root = Path("/work/fds")

        command = docker_make.toolchain_command(
            root, "image:tag", (501, 20), ["all", "test"]
        )

        self.assertEqual(
            command,
            [
                "docker",
                "run",
                "--rm",
                "--user",
                "501:20",
                "-e",
                "HOME=/tmp",
                "-v",
                "/work/fds:/src",
                "-w",
                "/src",
                "image:tag",
                "make",
                "all",
                "test",
            ],
        )


class MainTest(unittest.TestCase):
    def test_existing_image_is_not_rebuilt_and_make_exit_code_is_returned(self) -> None:
        with mock.patch.object(
            docker_make.subprocess, "run", side_effect=[completed(0), completed(3)]
        ) as run:
            exit_code = docker_make.main(["docker_make.py", "all"])

        self.assertEqual(exit_code, 3)
        self.assertEqual(run.call_count, 2)
        self.assertEqual(run.call_args_list[1].args[0][-2:], ["make", "all"])

    def test_missing_image_is_built_before_make_runs(self) -> None:
        with mock.patch.object(
            docker_make.subprocess,
            "run",
            side_effect=[completed(1), completed(0), completed(0)],
        ) as run:
            exit_code = docker_make.main(["docker_make.py", "test"])

        self.assertEqual(exit_code, 0)
        self.assertEqual(run.call_args_list[1].args[0][:3], ["docker", "build", "-t"])


if __name__ == "__main__":
    unittest.main()
