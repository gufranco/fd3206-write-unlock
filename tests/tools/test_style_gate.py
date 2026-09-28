# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import io
import tempfile
import unittest
import unittest.mock
from pathlib import Path

from tools import style_gate

COPYRIGHT_TAG = "SPDX-" + "FileCopyrightText"
LICENSE_TAG = "SPDX-" + "License-Identifier"

CLEAN_SOURCE = """#include <stdint.h>

static uint8_t twice(uint8_t value) {
    return (uint8_t)(value * 2U);
}

int main(void) {
    return twice(1U) == 2U ? 0 : 1;
}
"""


def findings_for(name: str, text: str) -> list[str]:
    path = Path(tempfile.mkdtemp()) / name
    path.write_text(text)
    return [finding.rule for finding in style_gate.check_file(path)]


class CommentTest(unittest.TestCase):
    def test_spdx_header_is_the_one_allowed_comment(self) -> None:
        header = (
            f"/* {COPYRIGHT_TAG}: 2026 A Person <a@example.invalid> */\n"
            f"/* {LICENSE_TAG}: MIT */\n"
        )

        rules = findings_for("a.c", header) + findings_for("a.S", header)

        self.assertEqual(rules, [])

    def test_spdx_text_inside_a_prose_comment_is_reported(self) -> None:
        rules = findings_for("a.c", f"/* note: {LICENSE_TAG}: MIT here */\n")

        self.assertIn("comment", rules)

    def test_line_comment_is_reported(self) -> None:
        rules = findings_for("a.c", "void f(void) {\n    g(); // why\n}\n")

        self.assertIn("comment", rules)

    def test_block_comment_is_reported(self) -> None:
        rules = findings_for("a.h", "/* header */\n")

        self.assertIn("comment", rules)

    def test_comment_markers_inside_literals_are_ignored(self) -> None:
        rules = findings_for(
            "a.c", "void f(void) {\n    put('/'); put(\"/* x */\");\n}\n"
        )

        self.assertNotIn("comment", rules)

    def test_assembly_semicolon_comment_is_reported(self) -> None:
        rules = findings_for("a.S", "    push r16 ; save\n")

        self.assertIn("comment", rules)


class TypeTest(unittest.TestCase):
    def test_platform_width_type_is_reported(self) -> None:
        rules = findings_for("a.c", "static long total;\n")

        self.assertIn("type", rules)

    def test_int_outside_main_is_reported(self) -> None:
        rules = findings_for("a.c", "static int count;\n")

        self.assertIn("type", rules)

    def test_int_main_is_the_one_exception(self) -> None:
        rules = findings_for("a.c", "int main(void) {\n    return 0;\n}\n")

        self.assertNotIn("type", rules)


class ConstructTest(unittest.TestCase):
    def test_heap_and_jumps_are_reported(self) -> None:
        rules = findings_for(
            "a.c", "void f(void) {\n    goto end;\n    p = malloc(4U);\n}\n"
        )

        self.assertEqual(rules.count("construct"), 2)

    def test_function_pointer_is_reported(self) -> None:
        rules = findings_for("a.c", "static void (*handler)(void);\n")

        self.assertIn("function-pointer", rules)

    def test_recursion_is_reported(self) -> None:
        rules = findings_for("a.c", "void walk(void) {\n    walk();\n}\n")

        self.assertIn("recursion", rules)

    def test_long_function_is_reported(self) -> None:
        body = "".join("    step();\n" for _ in range(61))

        rules = findings_for("a.c", f"void long_one(void) {{\n{body}}}\n")

        self.assertIn("length", rules)


class CleanTest(unittest.TestCase):
    def test_clean_source_has_no_findings(self) -> None:
        rules = findings_for("a.c", CLEAN_SOURCE)

        self.assertEqual(rules, [])


class MainTest(unittest.TestCase):
    def test_exit_code_reflects_findings(self) -> None:
        root = Path(tempfile.mkdtemp())
        (root / "clean.c").write_text(CLEAN_SOURCE)
        (root / "dirty.c").write_text("static long total;\n")

        with unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as errors:
            clean_exit = style_gate.main(["style_gate.py", str(root / "clean.c")])
            dirty_exit = style_gate.main(
                ["style_gate.py", str(root / "clean.c"), str(root / "dirty.c")]
            )

        self.assertEqual((clean_exit, dirty_exit), (0, 1))
        self.assertIn("dirty.c:1: type", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
