# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import io
import tempfile
import unittest
import unittest.mock
from pathlib import Path

from tools import release_notes

CHANGELOG = """# Changelog

## [1.0.5](https://example.invalid/compare/v1.0.4...v1.0.5) (2026-09-29)

### Refactoring

* call pins 14 and 15 write lines

## [1.0.4](https://example.invalid/compare/v1.0.3...v1.0.4) (2026-09-29)

### Bug Fixes

* drop writes that cannot fail

## 1.0.0 (2026-09-28)

### Features

* first release
"""


class NotesForTest(unittest.TestCase):
    def test_a_middle_section_stops_at_the_next_version(self) -> None:
        notes = release_notes.notes_for(CHANGELOG, "1.0.4")

        self.assertEqual(
            notes,
            "## [1.0.4](https://example.invalid/compare/v1.0.3...v1.0.4) (2026-09-29)"
            "\n\n### Bug Fixes\n\n* drop writes that cannot fail\n",
        )

    def test_the_last_section_runs_to_the_end_of_the_file(self) -> None:
        notes = release_notes.notes_for(CHANGELOG, "1.0.0")

        self.assertEqual(
            notes, "## 1.0.0 (2026-09-28)\n\n### Features\n\n* first release\n"
        )

    def test_a_missing_version_raises(self) -> None:
        with self.assertRaisesRegex(release_notes.NotesError, "no CHANGELOG section"):
            release_notes.notes_for(CHANGELOG, "1.0.40")


class VersionOfTest(unittest.TestCase):
    def test_a_release_tag_gives_its_version(self) -> None:
        version = release_notes.version_of("v1.0.5")

        self.assertEqual(version, "1.0.5")

    def test_anything_else_raises(self) -> None:
        with self.assertRaisesRegex(release_notes.NotesError, "not a release tag"):
            release_notes.version_of("main")


class MainTest(unittest.TestCase):
    def run_main(self, *arguments: str) -> tuple[int, str, str]:
        with (
            unittest.mock.patch("sys.stdout", new_callable=io.StringIO) as out,
            unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as err,
        ):
            code = release_notes.main(["release_notes.py", *arguments])
        return code, out.getvalue(), err.getvalue()

    def changelog(self) -> Path:
        path = Path(tempfile.mkdtemp()) / "CHANGELOG.md"
        path.write_text(CHANGELOG)
        return path

    def test_notes_are_printed(self) -> None:
        code, out, _ = self.run_main(str(self.changelog()), "v1.0.5")

        self.assertEqual(code, 0)
        self.assertIn("call pins 14 and 15 write lines", out)
        self.assertNotIn("1.0.4", out.split("\n", 1)[1])

    def test_a_bad_tag_fails(self) -> None:
        code, out, err = self.run_main(str(self.changelog()), "1.0.5")

        self.assertEqual((code, out), (1, ""))
        self.assertIn("not a release tag: 1.0.5", err)

    def test_wrong_arguments_print_usage(self) -> None:
        code, _, err = self.run_main()

        self.assertEqual(code, 2)
        self.assertIn("usage", err)


if __name__ == "__main__":
    unittest.main()
