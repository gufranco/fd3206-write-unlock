# 0003. BARR-C naming, fixed-width types and a two-layer header tree

Date: 2026-09-28. Status: accepted.

## Context

The chip and the test hosts disagree on the width of `int`, `long`, `double` and pointers, and on whether `char` is signed. Logic that is unit-tested on the host must mean the same thing on the chip. Symbols share one flat namespace in C.

## Decision

- Every external symbol starts with `fdswu_`, every type ends with `_t`, every constant and macro starts with `FDSWU_`. File-local helpers are `static` and unprefixed.
- Firmware code uses only `<stdint.h>` types and `bool`. `tools/style_gate.py` rejects the platform-width keywords, except `int main(void)`.
- `tests/type_widths.c` records the widths per compiler as `static_assert`s and runs in `make analyse` for avr-gcc and the host gcc, so the table in `AGENTS.md` cannot drift from the compilers.
- Headers live in `include/fdswu/` for the logic layer and `include/port/` for register names. Includes are written as `"fdswu/heads.h"`, never relative paths. `tools/layer_check.py` keeps the logic layer free of `avr/`, `util/` and `port/`.

## Consequences

- `src/heads.c` and `src/conditions.c` compile unchanged on the host, which is what makes 100 percent host branch coverage possible.
- A new logic module that needs a register belongs in the platform layer, or takes the register value as a parameter.
