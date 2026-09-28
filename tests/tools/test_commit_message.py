# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import io
import subprocess
import tempfile
import unittest
import unittest.mock
from pathlib import Path

from tools import commit_message

ZERO_SHA = "0" * 40


def git(root: Path, *arguments: str) -> str:
    return subprocess.run(
        ["git", "-C", str(root), *arguments],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()


def make_repository(messages: list[str]) -> tuple[Path, list[str]]:
    root = Path(tempfile.mkdtemp())
    git(root, "init", "-q")
    git(root, "config", "user.email", "tester@example.invalid")
    git(root, "config", "user.name", "Tester")
    git(root, "config", "commit.gpgsign", "false")
    shas = []
    for message in messages:
        git(root, "commit", "-q", "--allow-empty", "--no-verify", "-m", message)
        shas.append(git(root, "rev-parse", "HEAD"))
    return root, shas


class CheckTest(unittest.TestCase):
    def test_conventional_headers_pass(self) -> None:
        headers = [
            "feat: add a thing",
            "fix(ci): repair the release",
            "refactor!: c17 with zero findings",
            "ci(deps): bump the actions group across 1 directory with 2 updates",
        ]

        problems = [commit_message.check(header) for header in headers]

        self.assertEqual(problems, [[], [], [], []])

    def test_git_generated_headers_pass(self) -> None:
        problems = commit_message.check("Merge pull request #1 from x/y\n\nbody")

        self.assertEqual(problems, [])

    def test_unknown_type_is_reported(self) -> None:
        problems = commit_message.check("feature: add a thing")

        self.assertEqual(len(problems), 1)
        self.assertIn("type(scope): subject", problems[0])

    def test_missing_space_after_colon_is_reported(self) -> None:
        problems = commit_message.check("feat(x):add")

        self.assertEqual(len(problems), 1)

    def test_long_header_is_reported(self) -> None:
        problems = commit_message.check("feat: " + "a" * 100)

        self.assertIn("header is 106 characters, over 100", problems)

    def test_trailing_period_is_reported(self) -> None:
        problems = commit_message.check("docs: explain the pins.")

        self.assertEqual(problems, ["subject ends with a period"])

    def test_comment_lines_are_ignored(self) -> None:
        problems = commit_message.check("# comment\nfix: repair\n")

        self.assertEqual(problems, [])


class MainTest(unittest.TestCase):
    def run_main(self, *arguments: str, cwd: Path | None = None) -> tuple[int, str]:
        with (
            unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as err,
            unittest.mock.patch.object(commit_message, "REPOSITORY", cwd),
        ):
            code = commit_message.main(["commit_message.py", *arguments])
        return code, err.getvalue()

    def test_message_file_is_checked(self) -> None:
        good = Path(tempfile.mkdtemp()) / "MSG"
        good.write_text("fix: repair the gate\n")
        bad = Path(tempfile.mkdtemp()) / "MSG"
        bad.write_text("repair the gate\n")

        results = (
            self.run_main("--file", str(good))[0],
            self.run_main("--file", str(bad)),
        )

        self.assertEqual(results[0], 0)
        self.assertEqual(results[1][0], 1)
        self.assertIn("repair the gate", results[1][1])

    def test_range_reports_only_bad_commits(self) -> None:
        root, shas = make_repository(["feat: first", "bad message", "fix: third"])

        code, errors = self.run_main("--range", f"{shas[0]}..{shas[2]}", cwd=root)

        self.assertEqual(code, 1)
        self.assertIn(shas[1][:12], errors)
        self.assertNotIn(shas[2][:12], errors)

    def test_range_from_zero_checks_the_head_commit(self) -> None:
        root, shas = make_repository(["bad message", "feat: fine"])

        code, _ = self.run_main("--range", f"{ZERO_SHA}..{shas[1]}", cwd=root)

        self.assertEqual(code, 0)

    def test_wrong_arguments_print_usage(self) -> None:
        code, errors = self.run_main("--nope")

        self.assertEqual(code, 2)
        self.assertIn("usage", errors)


if __name__ == "__main__":
    unittest.main()
