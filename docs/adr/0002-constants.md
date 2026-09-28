# 0002. constexpr for constants, #define only where assembly shares a name

Date: 2026-09-28. Status: accepted.

## Context

Power of 10 rule 8 limits the preprocessor. C23 `constexpr` gives typed, scoped compile-time constants that `static_assert` can check. The INT0 handler is assembly, and the GNU assembler reads only preprocessor macros, not C declarations.

## Decision

- Pin masks and bit patterns are `static constexpr uint8_t` in `include/fdswu/pins.h`, with `static_assert` on their relationships: the two head masks are disjoint and make up the heads mask, and no pull-up lands on a soldered pin.
- `#define` is used only for include guards, `FDSWU_ASSERT`, and the register names in `include/port/registers.h` that both `src/main.c` and `src/write_data_edge.S` must agree on.
- Register bit masks in `src/main.c` are `constexpr` built from the avr-libc bit numbers, never `_BV`, whose `int` result MISRA rules 10.1 and 10.4 flag.

## Consequences

- A wrong mask relationship fails the compile, not the simulation.
- `static constexpr` in a header gives each translation unit its own copy. It costs no flash, because every use folds to an immediate, and MISRA rules 5.9 and 8.9 flag it; see [`../misra.md`](../misra.md).
