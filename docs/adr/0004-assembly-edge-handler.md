# 0004. The WRITE DATA edge handler is assembly

Date: 2026-09-28. Status: accepted.

## Context

WRITE DATA falling edges can arrive 4.7 us apart, 37 cycles at 8 MHz. A C interrupt that called the heads-plan function saved every call-clobbered register and measured about 70 cycles, 9 us, by instruction count; the fastest-rate simulation scenario failed.

## Decision

- `src/write_data_edge.S` is the INT0 handler: 15 instructions.
- The main loop precomputes the next two head states into `GPIOR0` and `GPIOR1`. The handler writes `GPIOR0` to `DDRB` in its third instruction, 10 cycles after the interrupt is taken, then rotates the two values and flips a toggle bit in `GPIOR2`.
- The handler changes no `SREG` flag, so it saves no `SREG`. It touches only `r16`, `r17`, `GPIOR0`, `GPIOR1`, `GPIOR2` and `DDRB`.
- Branch targets are `.L` local labels. avr-ld still lists them for relaxation, so the coverage and figures tools treat a `.L` header as part of the enclosing function.
- The C side updates the three registers with interrupts disabled for the few instructions it takes.

## Consequences

- The handler is the one place the style gate cannot check types; it has no types. Its instruction count is published in the README figures block and checked by `make analyse`.
- Any change to the handler reruns the fastest-rate scenario, 1000 edges 4.7 us apart.
