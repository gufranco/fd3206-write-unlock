# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import io
import tempfile
import unittest
import unittest.mock
from pathlib import Path

from tools import layer_check


def make_tree(files: dict[str, str]) -> Path:
    root = Path(tempfile.mkdtemp())
    for name, text in files.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    return root


CLEAN_TREE = {
    "include/fdswu/write_plan.h": "#include <stdint.h>\n",
    "include/port/registers.h": "#include <avr/io.h>\n",
    "src/write_plan.c": '#include "fdswu/write_plan.h"\n#include "fdswu/pins.h"\n',
    "include/port/port.h": "#include <stdint.h>\n",
    "src/main.c": '#include "port/port.h"\n#include "fdswu/write_plan.h"\n',
    "src/write_data_edge.S": '#include "port/registers.h"\n',
}


class ViolationTest(unittest.TestCase):
    def test_clean_tree_has_no_violations(self) -> None:
        root = make_tree(CLEAN_TREE)

        violations = layer_check.violations(root)

        self.assertEqual(violations, [])

    def test_logic_module_reaching_the_hardware_is_reported(self) -> None:
        root = make_tree({**CLEAN_TREE, "src/write_plan.c": "#include <avr/io.h>\n"})

        violations = layer_check.violations(root)

        self.assertEqual(
            [(v.path.name, v.included) for v in violations],
            [("write_plan.c", "avr/io.h")],
        )

    def test_public_header_including_the_port_is_reported(self) -> None:
        root = make_tree(
            {
                **CLEAN_TREE,
                "include/fdswu/write_plan.h": '#include "port/registers.h"\n',
            }
        )

        violations = layer_check.violations(root)

        self.assertEqual([v.included for v in violations], ["port/registers.h"])

    def test_including_a_source_file_is_reported(self) -> None:
        root = make_tree({**CLEAN_TREE, "src/main.c": '#include "write_plan.c"\n'})

        violations = layer_check.violations(root)

        self.assertEqual([v.included for v in violations], ["write_plan.c"])


class PlatformCTest(unittest.TestCase):
    def test_c_platform_source_including_avr_is_reported(self) -> None:
        root = make_tree({**CLEAN_TREE, "src/main.c": "#include <avr/io.h>\n"})

        violations = layer_check.violations(root)

        self.assertEqual(
            [(v.path.name, v.included) for v in violations], [("main.c", "avr/io.h")]
        )


class MainTest(unittest.TestCase):
    def test_exit_code_reflects_violations(self) -> None:
        clean = make_tree(CLEAN_TREE)
        dirty = make_tree({**CLEAN_TREE, "src/write_plan.c": "#include <avr/io.h>\n"})

        with unittest.mock.patch("sys.stderr", new_callable=io.StringIO) as errors:
            codes = (
                layer_check.main(["layer_check.py", str(clean)]),
                layer_check.main(["layer_check.py", str(dirty)]),
            )

        self.assertEqual(codes, (0, 1))
        self.assertIn("write_plan.c", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
